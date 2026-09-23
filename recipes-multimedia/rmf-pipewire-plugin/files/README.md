# RMF Audio Capture PipeWire Source Plugin

This plugin bridges the RMF Audio Capture HAL interface with PipeWire, allowing captured audio to be routed through the PipeWire audio subsystem.

## Features

- Captures audio from RMF Audio Capture HAL (primary or auxiliary audio sources)
- Exposes captured audio as a PipeWire source
- Supports multiple audio formats:
  - 16-bit stereo/mono PCM
  - 24-bit stereo PCM
  - 24-bit 5.1 multichannel
- Handles various sampling rates (16kHz - 48kHz)
- Thread-safe ring buffer for smooth audio delivery
- Automatic silence filling when no audio data is available

## Building

### Prerequisites

- PipeWire development libraries (`libpipewire-0.3-dev`)
- RMF Audio Capture library and headers
- pkg-config
- CMake (3.16+)
- GCC or compatible C compiler

### Compilation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This will produce the `rmf-audio-capture-source` executable.

### Installation

```bash
sudo cmake --install build --prefix /usr
```

This installs the binary to `/usr/bin/rmf-audio-capture-source`.

## Usage

### Basic Usage (Primary Audio)

```bash
./rmf-audio-capture-source
```

This captures the primary audio source by default.

### Auxiliary Audio Source

```bash
./rmf-audio-capture-source auxiliary
```

This captures the auxiliary audio source (e.g., alternate language track).

## How It Works

1. **Initialization**: Opens the RMF Audio Capture interface and creates a PipeWire stream
2. **Configuration**: Queries default audio settings and configures the capture parameters
3. **Ring Buffer**: Maintains a thread-safe ring buffer to handle audio data flow between RMF callbacks and PipeWire
4. **Audio Flow**:
   - RMF Audio Capture HAL delivers audio data via `buffer_ready_callback()`
   - Data is written to the ring buffer
   - PipeWire's `on_process()` callback reads from the ring buffer and delivers to PipeWire clients
5. **Cleanup**: Gracefully stops capture and releases resources on exit (Ctrl+C)

## Architecture

```
┌─────────────────────┐
│  RMF Audio Capture  │
│       HAL           │
└──────────┬──────────┘
           │ Callback
           ▼
    ┌─────────────┐
    │ Ring Buffer │
    │  (thread-   │
    │   safe)     │
    └──────┬──────┘
           │ Read
           ▼
    ┌─────────────┐
    │  PipeWire   │
    │   Stream    │
    └──────┬──────┘
           │
           ▼
    ┌─────────────┐
    │  PipeWire   │
    │   Clients   │
    └─────────────┘
```

## Configuration

The plugin uses default settings from the RMF Audio Capture HAL but applies these policies:

- **FIFO Size**: If not provided by HAL, defaults to 500ms of audio data
- **Threshold**: Set to 1/4 of FIFO size for callback triggering
- **Ring Buffer**: Sized at 2x FIFO size for headroom

## Debugging

The plugin outputs status information to stdout:

- Stream state changes
- Audio capture status updates
- Buffer overflow warnings

## Integration with PipeWire

Once running, the audio source appears in PipeWire and can be:

- Viewed with `pw-cli` or `pw-top`
- Connected to sinks using `pw-link`
- Routed using PipeWire session managers (WirePlumber, PipeWire Media Session)

Example to list sources:
```bash
pw-cli list-objects | grep -A 5 "RMF Audio Capture"
```

## Error Handling

The plugin handles:

- RMF Audio Capture errors (returns appropriate error codes)
- PipeWire stream errors (logs and exits)
- Ring buffer overflows (logs warning, drops oldest data)
- Ring buffer underflows (fills with silence)

## WirePlumber Integration

The plugin can be automatically started when Bluetooth devices connect using WirePlumber.

### Installation with WirePlumber

Run the installation script:

```bash
chmod +x install.sh
./install.sh
```

This will:
1. Build and install the `rmf-audio-capture-source` executable
2. Install the WirePlumber Lua script to monitor Bluetooth connections
3. Configure WirePlumber to load the script
4. Restart WirePlumber (if running)

### Manual WirePlumber Setup

If you prefer manual installation:

1. **Copy the Lua script:**
   ```bash
   sudo cp rmf-audio-on-bluez.lua /etc/wireplumber/main.lua.d/51-rmf-audio-on-bluez.lua
   ```

2. **Copy the configuration:**
   ```bash
   sudo cp 51-rmf-audio-bluez.conf /etc/wireplumber/wireplumber.conf.d/
   ```

3. **Restart WirePlumber:**
   ```bash
   systemctl --user restart wireplumber
   ```

### How WirePlumber Integration Works

- **Lua Script** (`rmf-audio-on-bluez.lua`): Monitors for Bluetooth (bluez5) device connections
- **Automatic Start**: Spawns `rmf-audio-capture-source` when the first Bluetooth device connects
- **Automatic Stop**: Terminates the source when all Bluetooth devices disconnect
- **State Tracking**: Ensures only one instance runs at a time

### Monitoring

View WirePlumber logs to see the integration in action:

```bash
journalctl --user -u wireplumber -f
```

You should see messages like:
- "Bluez device connected: Device Name (ID: ...)"
- "Starting RMF audio capture source"
- "RMF audio source started with PID: ..."

### Troubleshooting WirePlumber Integration

**Plugin doesn't start:**
- Check WirePlumber logs: `journalctl --user -u wireplumber -f`
- Verify the executable path in the Lua script matches installation
- Ensure the script has correct permissions

**Multiple instances:**
- The script tracks spawned processes to prevent duplicates
- If multiple instances appear, check WirePlumber configuration for conflicts

**Script not loading:**
- Verify files are in the correct directories
- Check WirePlumber configuration syntax: `wireplumber --version`
- Ensure the configuration file is being loaded

## License

Apache License 2.0 - See source file for full license text.
