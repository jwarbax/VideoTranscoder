# Video Transcoder with Audio Sync

A C++20 application that processes video files and synchronizes external audio recordings using peak interval pattern matching.

## Features

- Processes MP4 video files with WAV audio synchronization
- Automatic audio-video sync detection using peak interval patterns
- Dual audio channel support (high-gain and low-gain with _D suffix)
- Multiple audio track output (HighLav, LowLav, Camera)
- Duration-based file matching with ±30 second tolerance
- Test output format: H.264 960x540 with PCM 16-bit 48kHz audio

## Requirements

### System Dependencies

**Fedora/RHEL/CentOS:**
```bash
sudo dnf install cmake gcc-g++ ffmpeg-devel pkg-config
```

**Ubuntu/Debian:**
```bash
sudo apt install cmake g++ libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev pkg-config
```

**macOS:**
```bash
brew install cmake ffmpeg pkg-config
```

### Build Requirements
- C++20 compatible compiler (GCC 10+, Clang 10+)
- CMake 3.20+
- FFmpeg libraries (libavformat, libavcodec, libavutil, libswscale, libswresample)

## Building

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Usage

The application expects files in `/s3` directory and outputs to `/s3/output`:

```bash
./video_transcoder
```

### File Structure

**Input (in /s3):**
```
/s3/
├── C0001.MP4           # Video file
├── C0001.WAV           # High-gain audio (0dB)
├── C0001_D.WAV         # Low-gain audio (-6dB, optional)
├── C0002.MP4           # Another video
└── C0002.WAV           # Corresponding audio
```

**Output (in /s3/output):**
```
/s3/output/
├── C0001.mov           # Video with synced audio tracks
└── C0002.mov           # Additional processed files
```

## Audio Sync Algorithm

The sync detection uses peak interval pattern matching:

1. **Peak Extraction**: Extracts audio peaks from both video and external audio using envelope detection
2. **Pattern Creation**: Converts peaks to interval patterns (time differences between consecutive peaks)
3. **Pattern Matching**: Compares interval sequences to find best alignment
4. **Offset Calculation**: Determines time offset needed to sync audio with video

### Sync Parameters
- Analysis window: 30 seconds (or 30% of file duration)
- Peak detection threshold: 20% of maximum envelope
- Minimum peak distance: 250ms
- Pattern matching tolerance: 300ms
- Minimum 3 peaks required for reliable sync

## File Matching

The application matches video and audio files using:

1. **Exact filename matching**: `C0001.MP4` → `C0001.WAV`
2. **Low-gain detection**: Automatically finds `C0001_D.WAV` for dual-channel recordings
3. **Duration-based fallback**: Matches files within ±30 seconds duration when exact names don't match

## Output Format

- **Video**: H.264, 960x540 resolution, CRF 23
- **Audio**: PCM 16-bit, 48kHz sample rate
- **Tracks**: 
  - Track 1: HighLav (synced external audio)
  - Track 2: LowLav (if available)
  - Track 3: Camera (original video audio)

## Project Structure

```
├── CMakeLists.txt              # Build configuration
├── include/
│   ├── transcoder.h           # Main transcoder interface
│   └── audio_sync.h           # Audio sync implementation
└── src/
    ├── main.cpp               # Application entry point
    ├── transcoder.cpp         # Core transcoding logic
    └── audio_sync.cpp         # Peak pattern matching
```

## Error Handling

- Continues processing if individual files fail
- Reports missing audio matches
- Handles files without sync requirements
- Validates file accessibility and format compatibility

## Dependencies

- **FFmpeg**: Media processing and format conversion
- **C++ Standard Library**: Filesystem operations and containers
- **System calls**: External process execution for FFmpeg commands