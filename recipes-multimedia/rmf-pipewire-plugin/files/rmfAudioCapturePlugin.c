/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <stdatomic.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>
#include <spa/utils/ringbuffer.h>
#include <pipewire/pipewire.h>

#include "rmfAudioCapture.h"

#define DEFAULT_RATE        48000
#define DEFAULT_CHANNELS    2
#define DEFAULT_FORMAT      SPA_AUDIO_FORMAT_S16_LE

/* Constants for buffer sizing */
#define FIFO_DURATION_MS    500        /* Default FIFO size in milliseconds */
#define FIFO_DURATION_DIV   2          /* Divisor for FIFO duration calculation (500ms = 1000/2) */
#define THRESHOLD_DIVISOR   4          /* Threshold is 1/4 of FIFO size */
#define RING_BUFFER_MULT    2          /* Ring buffer is 2x FIFO size for safety */
#define MAX_RING_BUFFER     (64 * 1024 * 1024)  /* Maximum 64MB ring buffer to prevent overflow */

struct data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    
    RMF_AudioCaptureHandle capture_handle;
    
    /* Lock-free SPSC ring buffer: producer is buffer_ready_callback (HAL
     * thread), consumer is on_process (PipeWire RT thread). No mutex is
     * needed to coordinate them, which keeps the RT path lock-free. */
    struct spa_ringbuffer ring;
    uint8_t *ring_buffer;
    uint32_t ring_buffer_size;
    uint32_t ring_stride;
    
    atomic_bool started;
    
    /* RT-safe drop/underrun counters, updated from the audio callbacks. */
    atomic_uint_least64_t overflow_count;
    atomic_uint_least64_t underrun_count;
    
    racFormat format;
    racFreq sampling_freq;
    uint32_t rate;
    uint32_t channels;
};

/* Convert RMF frequency enum to Hz */
static uint32_t rac_freq_to_rate(racFreq freq)
{
    switch (freq) {
        case racFreq_e16000: return 16000;
        case racFreq_e22050: return 22050;
        case racFreq_e24000: return 24000;
        case racFreq_e32000: return 32000;
        case racFreq_e44100: return 44100;
        case racFreq_e48000: return 48000;
        default: return 48000;
    }
}

/* Get number of channels from RMF format */
static uint32_t rac_format_to_channels(racFormat format)
{
    switch (format) {
        case racFormat_e16BitStereo:
        case racFormat_e24BitStereo:
            return 2;
        case racFormat_e16BitMonoLeft:
        case racFormat_e16BitMonoRight:
        case racFormat_e16BitMono:
            return 1;
        case racFormat_e24Bit5_1:
            return 6;
        default:
            return 2;
    }
}

/* Get sample format from RMF format */
static enum spa_audio_format rac_format_to_spa_format(racFormat format)
{
    switch (format) {
        case racFormat_e16BitStereo:
        case racFormat_e16BitMonoLeft:
        case racFormat_e16BitMonoRight:
        case racFormat_e16BitMono:
            return SPA_AUDIO_FORMAT_S16_LE;
        case racFormat_e24BitStereo:
        case racFormat_e24Bit5_1:
            return SPA_AUDIO_FORMAT_S24_32_LE;
        default:
            return SPA_AUDIO_FORMAT_S16_LE;
    }
}

/* Get bytes per sample from RMF format */
static uint32_t rac_format_to_bytes_per_sample(racFormat format)
{
    switch (format) {
        case racFormat_e16BitStereo:
        case racFormat_e16BitMonoLeft:
        case racFormat_e16BitMonoRight:
        case racFormat_e16BitMono:
            return 2;  /* 16-bit */
        case racFormat_e24BitStereo:
        case racFormat_e24Bit5_1:
            return 4;  /* 24-bit in 32-bit container */
        default:
            return 2;
    }
}

/* RMF Audio Capture buffer ready callback */
static rmf_Error buffer_ready_callback(void *cbBufferReadyParm, void *AudioCaptureBuffer, unsigned int AudioCaptureBufferSize)
{
    struct data *data = cbBufferReadyParm;
    int32_t filled, avail;
    uint32_t index;
    
    if (!data || !AudioCaptureBuffer || AudioCaptureBufferSize == 0) {
        return RMF_INVALID_PARM;
    }
    
    if (!atomic_load_explicit(&data->started, memory_order_acquire)) {
        return RMF_SUCCESS;
    }
    
    /* Lock-free write into the SPSC ring buffer. */
    filled = spa_ringbuffer_get_write_index(&data->ring, &index);
    avail = data->ring_buffer_size - filled;
    
    if (avail < (int32_t)AudioCaptureBufferSize) {
        /* Not enough room: drop the excess and write what fits. Overflow is
         * counted rather than logged to keep this callback non-blocking.
         * Round down to a whole number of frames so the write index stays on
         * a frame boundary: ring_buffer_size is a power of two and is not
         * necessarily a multiple of the frame size (e.g. 24-byte 24-bit 5.1),
         * so 'avail' can land mid-frame. */
        atomic_fetch_add_explicit(&data->overflow_count, 1, memory_order_relaxed);
        AudioCaptureBufferSize = avail > 0 ?
            ((unsigned int)avail / data->ring_stride) * data->ring_stride : 0;
    }
    
    if (AudioCaptureBufferSize > 0) {
        spa_ringbuffer_write_data(&data->ring,
                                   data->ring_buffer,
                                   data->ring_buffer_size,
                                   index & (data->ring_buffer_size - 1),
                                   AudioCaptureBuffer,
                                   AudioCaptureBufferSize);
        
        index += AudioCaptureBufferSize;
        spa_ringbuffer_write_update(&data->ring, index);
    }
    
    return RMF_SUCCESS;
}

/* RMF Audio Capture status change callback */
static rmf_Error status_change_callback(void *cbStatusParm)
{
    struct data *data = cbStatusParm;
    RMF_AudioCapture_Status status;
    rmf_Error err;
    
    if (!data) {
        return RMF_INVALID_PARM;
    }
    
    err = RMF_AudioCapture_GetStatus(data->capture_handle, &status);
    if (err == RMF_SUCCESS) {
        fprintf(stdout, "Audio capture status changed: started=%d, format=%d, freq=%d\n",
                status.started, status.format, status.samplingFreq);
    }
    
    return RMF_SUCCESS;
}

/* Ring buffer statistics reporting interval, in seconds. */
#define STATS_REPORT_INTERVAL_S    5

/* Periodic stats report, runs on the main loop (non-RT) so I/O is safe here. */
static void on_stats_timer(void *userdata, uint64_t expirations)
{
    struct data *data = userdata;
    uint64_t overflows = atomic_load_explicit(&data->overflow_count, memory_order_relaxed);
    uint64_t underruns = atomic_load_explicit(&data->underrun_count, memory_order_relaxed);

    if (overflows || underruns) {
        fprintf(stdout, "Audio capture stats: overflows=%" PRIu64 ", underruns=%" PRIu64 "\n",
                overflows, underruns);
    }
}

/* PipeWire stream process callback */
static void on_process(void *userdata)
{
    struct data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    uint8_t *dst;
    uint32_t n_frames, stride, size;
    int32_t avail;
    uint32_t index;
    
    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }
    
    buf = b->buffer;
    dst = buf->datas[0].data;
    if (dst == NULL) {
        pw_stream_queue_buffer(data->stream, b);
        return;
    }
    
    stride = data->ring_stride;
    n_frames = buf->datas[0].maxsize / stride;
    size = n_frames * stride;
    
    /* Lock-free read from the SPSC ring buffer. This runs on the PipeWire RT
     * thread (PW_STREAM_FLAG_RT_PROCESS), so it must not lock or do I/O. */
    avail = spa_ringbuffer_get_read_index(&data->ring, &index);
    
    if (avail < (int32_t)size) {
        /* Not enough data: fill with silence and read only what is available.
         * Underruns are counted rather than logged to stay RT-safe. */
        atomic_fetch_add_explicit(&data->underrun_count, 1, memory_order_relaxed);
        memset(dst, 0, size);
        n_frames = avail > 0 ? (uint32_t)avail / stride : 0;
        size = n_frames * stride;
    }
    
    if (size > 0) {
        spa_ringbuffer_read_data(&data->ring,
                                  data->ring_buffer,
                                  data->ring_buffer_size,
                                  index & (data->ring_buffer_size - 1),
                                  dst,
                                  size);
        
        index += size;
        spa_ringbuffer_read_update(&data->ring, index);
    }
    
    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = (buf->datas[0].maxsize / stride) * stride;
    
    pw_stream_queue_buffer(data->stream, b);
}

/* PipeWire stream state changed callback */
static void on_stream_state_changed(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    struct data *data = userdata;
    fprintf(stdout, "Stream state changed: %s -> %s\n",
            pw_stream_state_as_string(old),
            pw_stream_state_as_string(state));
    
    if (state == PW_STREAM_STATE_ERROR) {
        fprintf(stderr, "Stream error: %s\n", error);
        pw_main_loop_quit(data->loop);
    }
}

/* PipeWire stream param changed callback */
static void on_stream_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    struct data *data = userdata;
    
    if (param == NULL || id != SPA_PARAM_Format)
        return;
    
    fprintf(stdout, "Stream format changed\n");
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_stream_state_changed,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number)
{
    struct data *data = userdata;
    pw_main_loop_quit(data->loop);
}

int main(int argc, char *argv[])
{
    struct data data = { 0 };
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    RMF_AudioCapture_Settings settings;
    RMF_AudioCapture_Status status;
    rmf_Error err;
    int ret = 0;
    struct spa_source *stats_timer = NULL;
    
    /* Parse command line arguments */
    const char *capture_type = RMF_AC_TYPE_PRIMARY;
    if (argc > 1) {
        if (strcmp(argv[1], "primary") == 0) {
            capture_type = RMF_AC_TYPE_PRIMARY;
        } else if (strcmp(argv[1], "auxiliary") == 0) {
            capture_type = RMF_AC_TYPE_AUXILIARY;
        } else {
            fprintf(stderr, "Invalid capture type '%s'. Use 'primary' or 'auxiliary'.\n", argv[1]);
            return -1;
        }
    }
    
    /* Initialize PipeWire */
    pw_init(&argc, &argv);
    
    data.loop = pw_main_loop_new(NULL);
    if (!data.loop) {
        fprintf(stderr, "Failed to create main loop\n");
        ret = -1;
        goto cleanup;
    }
    
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);
    
    /* Open RMF Audio Capture */
    err = RMF_AudioCapture_Open_Type(&data.capture_handle, capture_type);
    if (err != RMF_SUCCESS) {
        fprintf(stderr, "Failed to open RMF Audio Capture: error %d\n", err);
        ret = -1;
        goto cleanup;
    }
    
    fprintf(stdout, "RMF Audio Capture opened for %s\n", capture_type);
    
    /* Get default settings */
    err = RMF_AudioCapture_GetDefaultSettings(&settings);
    if (err != RMF_SUCCESS) {
        fprintf(stderr, "Failed to get default settings: %d\n", err);
        ret = -1;
        goto cleanup_capture;
    }
    
    /* Configure settings */
    settings.cbBufferReady = buffer_ready_callback;
    settings.cbBufferReadyParm = &data;
    settings.cbStatusChange = status_change_callback;
    settings.cbStatusParm = &data;
    
    /* If fifoSize is 0, use a default (e.g., 500ms worth of audio) */
    if (settings.fifoSize == 0) {
        uint32_t rate = rac_freq_to_rate(settings.samplingFreq);
        uint32_t channels = rac_format_to_channels(settings.format);
        uint32_t bytes_per_sample = rac_format_to_bytes_per_sample(settings.format);
        settings.fifoSize = rate * channels * bytes_per_sample / FIFO_DURATION_DIV; /* 500ms */
    }
    
    /* Set threshold to 1/4 of FIFO size if not already set */
    if (settings.threshold == 0) {
        settings.threshold = settings.fifoSize / THRESHOLD_DIVISOR;
    }
    
    data.format = settings.format;
    data.sampling_freq = settings.samplingFreq;
    data.rate = rac_freq_to_rate(settings.samplingFreq);
    data.channels = rac_format_to_channels(settings.format);

    {
        size_t requested = settings.fifoSize * (size_t)RING_BUFFER_MULT; /* Make it larger for safety */
        size_t pow2;

        /* Validate ring buffer size to prevent overflow */
        if (requested > (size_t)MAX_RING_BUFFER) {
            fprintf(stderr, "Requested ring buffer size %zu exceeds maximum %u\n",
                    requested, MAX_RING_BUFFER);
            ret = -1;
            goto cleanup_capture;
        }

        /* Round up to a power of two. The read/write paths locate the offset
         * with 'index & (ring_buffer_size - 1)', which only equals
         * 'index % ring_buffer_size' when the size is a power of two. This
         * also keeps the masking correct across the uint32 index wraparound. */
        pow2 = 1;
        while (pow2 < requested)
            pow2 <<= 1;

        data.ring_buffer_size = (uint32_t)pow2;
    }
    
    data.ring_buffer = calloc(1, data.ring_buffer_size);
    if (!data.ring_buffer) {
        fprintf(stderr, "Failed to allocate ring buffer\n");
        ret = -1;
        goto cleanup_capture;
    }
    
    spa_ringbuffer_init(&data.ring);
    
    data.ring_stride = data.channels * rac_format_to_bytes_per_sample(settings.format);
    
    /* Create PipeWire stream */
    data.stream = pw_stream_new_simple(
            pw_main_loop_get_loop(data.loop),
            "RMF Audio Capture Source",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Capture",
                PW_KEY_MEDIA_ROLE, "Production",
                NULL),
            &stream_events,
            &data);
    
    if (!data.stream) {
        fprintf(stderr, "Failed to create PipeWire stream\n");
        ret = -1;
        goto cleanup_ring;
    }
    
    /* Set stream parameters */
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
            &SPA_AUDIO_INFO_RAW_INIT(
                .format = rac_format_to_spa_format(settings.format),
                .channels = data.channels,
                .rate = data.rate));
    
    /* Connect stream */
    ret = pw_stream_connect(data.stream,
                  PW_DIRECTION_OUTPUT,
                  PW_ID_ANY,
                  PW_STREAM_FLAG_AUTOCONNECT |
                  PW_STREAM_FLAG_MAP_BUFFERS |
                  PW_STREAM_FLAG_RT_PROCESS,
                  params, 1);
    
    if (ret < 0) {
        fprintf(stderr, "Failed to connect stream: %s\n", strerror(-ret));
        goto cleanup_stream;
    }
    
    /* Declare source latency to PipeWire graph */
    {
        uint8_t lat_buf[1024];
        struct spa_pod_builder lat_b = SPA_POD_BUILDER_INIT(lat_buf, sizeof(lat_buf));
        const char *delay_env = getenv("RMFAUDIOCAP_PIPEWIRE_LATENCY_OVERRIDE");
        uint32_t delay_ms = delay_env ? (uint32_t)atoi(delay_env) : 0;
        struct spa_latency_info latency = {
            .direction = SPA_DIRECTION_OUTPUT,
            .min_ns = (uint64_t)delay_ms * SPA_NSEC_PER_MSEC,
            .max_ns = (uint64_t)delay_ms * SPA_NSEC_PER_MSEC,
        };
        const struct spa_pod *lat_params[1];
        lat_params[0] = spa_latency_build(&lat_b, SPA_PARAM_Latency, &latency);
        pw_stream_update_params(data.stream, lat_params, 1);
        fprintf(stdout, "Source latency declared: %u ms (direction=OUTPUT)\n", delay_ms);
        settings.delayCompensation_ms = delay_ms;
    }
    
    /* Start RMF Audio Capture */
    err = RMF_AudioCapture_Start(data.capture_handle, &settings);
    if (err != RMF_SUCCESS) {
        fprintf(stderr, "Failed to start RMF Audio Capture: %d\n", err);
        ret = -1;
        goto cleanup_stream;
    }
    
    atomic_store_explicit(&data.started, true, memory_order_release);
    fprintf(stdout, "RMF Audio Capture started\n");
    
    /* Periodically report overflow/underrun counters from the main loop. */
    stats_timer = pw_loop_add_timer(pw_main_loop_get_loop(data.loop), on_stats_timer, &data);
    if (stats_timer) {
        struct timespec value = { .tv_sec = STATS_REPORT_INTERVAL_S, .tv_nsec = 0 };
        struct timespec interval = { .tv_sec = STATS_REPORT_INTERVAL_S, .tv_nsec = 0 };
        pw_loop_update_timer(pw_main_loop_get_loop(data.loop), stats_timer, &value, &interval, false);
    }
    
    /* Get status */
    err = RMF_AudioCapture_GetStatus(data.capture_handle, &status);
    if (err == RMF_SUCCESS) {
        fprintf(stdout, "Status: started=%d, format=%d, freq=%d, fifoDepth=%zu\n",
                status.started, status.format, status.samplingFreq, status.fifoDepth);
    }
    
    /* Run main loop */
    fprintf(stdout, "Running... Press Ctrl+C to quit\n");
    pw_main_loop_run(data.loop);
    
    /* Cleanup */
    atomic_store_explicit(&data.started, false, memory_order_release);
    
    if (stats_timer)
        pw_loop_destroy_source(pw_main_loop_get_loop(data.loop), stats_timer);
    
    err = RMF_AudioCapture_Stop(data.capture_handle);
    if (err != RMF_SUCCESS) {
        fprintf(stderr, "Failed to stop RMF Audio Capture: %d\n", err);
    }
    
cleanup_stream:
    if (data.stream)
        pw_stream_destroy(data.stream);
    
cleanup_ring:
    if (data.ring_buffer)
        free(data.ring_buffer);
    
cleanup_capture:
    err = RMF_AudioCapture_Close(data.capture_handle);
    if (err != RMF_SUCCESS) {
        fprintf(stderr, "Failed to close RMF Audio Capture: %d\n", err);
    }
    
cleanup:
    if (data.loop)
        pw_main_loop_destroy(data.loop);
    
    pw_deinit();
    
    return ret;
}
