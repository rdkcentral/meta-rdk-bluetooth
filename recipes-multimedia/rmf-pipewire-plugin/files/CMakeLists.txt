cmake_minimum_required(VERSION 3.16)
project(rmf_audio_capture_pipewire C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_executable(rmf-audio-capture-source
    rmfAudioCapturePlugin.c
)

target_compile_definitions(rmf-audio-capture-source PRIVATE
    _GNU_SOURCE
)

target_compile_options(rmf-audio-capture-source PRIVATE
    -Wall
    -Wextra
    -O2
    -g
)

target_include_directories(rmf-audio-capture-source PRIVATE
    ${CMAKE_SYSROOT}/usr/include/media-utils/audioCapture
)

target_link_libraries(rmf-audio-capture-source PRIVATE
    PkgConfig::PIPEWIRE
    pthread
    rmfAudioCapture
)

install(TARGETS rmf-audio-capture-source
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
