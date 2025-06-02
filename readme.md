# Advanced Video Transcoder with Audio Sync

A high-performance C++20 application for transcoding video files with intelligent dual-audio synchronization, designed for professional video workflows.

## Features

### Core Transcoding
- **Multiple Output Formats**: ProRes 422 HQ, ProRes 4444, ProRes Basic, H.264 10-bit, MJPEG
- **High Quality**: Preserves 10-bit color depth and professional audio standards
- **Real-time Progress**: Live progress bars with time estimates

### Advanced Audio Sync
- **Dual-Channel Recording**: Automatically syncs high-gain (0dB) and low-gain (-6dB) lav mic recordings
- **Smart Matching**: 
  - Exact filename matching
  - Duration-based matching (±10 second tolerance)
  - Automatic _D suffix detection for low-gain files
- **Multi-track Output**: Creates files with HighLav, LowLav, and Camera audio tracks
- **Intelligent Fallback**: Two-pass processing handles edge cases and b-roll footage

## Requirements

### System Dependencies
```bash
# Fedora/RHEL/CentOS
sudo dnf install cmake gcc-g++ ffmpeg-devel pkg-config

# Ubuntu/Debian
sudo apt install cmake g++ libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev pkg-config

# macOS (with Homebrew)
brew install cmake ffmpeg pkg-config
```

### Build Requirements
- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.20+
- FFmpeg libraries (libavformat, libavcodec, libavutil, libswscale, libswresample)

## Building

### Quick Build
```bash
chmod +x build.sh
./build.sh
```

### Manual Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### CLion Setup
1. Open CLion
2. Open the project directory
3. CLion will automatically detect CMakeLists.txt
4. Build → Build Project

## Usage

### Basic Usage
```bash
./video_transcoder
```
Interactive mode will guide you through format selection and output naming.

### Command Line Options
```bash
./video_transcoder [options]

Options:
  -h, --help            Show help message
  -d, --dir DIR         Input directory (default: ./forTranscoding)
  -o, --output NAME     Output directory name
  -f, --format NUM      Format (1-5, see formats below)
```

### Formats
1. **ProRes 422 HQ** - Best for color grading (10-bit 4:2:2)
2. **ProRes 4444** - Highest quality with alpha support
3. **ProRes Basic** - Good quality, smaller files
4. **H.264 10-bit** - Smallest files, good for editing
5. **MJPEG MOV** - Legacy format (8-bit)

### File Organization

**Input Structure:**
```
forTranscoding/
├── C0001.MP4           # Video file
├── C0001.WAV           # High-gain audio (0dB)
├── C0001_D.WAV         # Low-gain audio (-6dB)
├── C0002.MP4           # B-roll (no audio sync needed)
└── ...
```

**Output Structure:**
```
output_folder/
├── C0001.mov           # 3-track: HighLav, LowLav, Camera
├── C0002.mov           # 1-track: Camera only
└── ...
```

## Audio Sync Workflow

### First Pass: Auto-Matching
1. **Exact Name Matching**: `C0001.MP4` → `C0001.WAV` + `C0001_D.WAV`
2. **Duration Matching**: Files within ±10 seconds duration
3. **Sync Testing**: Automatic cross-correlation analysis
4. **Smart Pairing**: If high-gain syncs, low-gain uses same timing

### Second Pass: Manual Review
- Processes remaining unmatched files
- Option to force processing without audio sync
- Handles b-roll and edge cases

## Development

### Project Structure
```
├── CMakeLists.txt          # Build configuration
├── build.sh               # Quick build script
├── src/
│   ├── main.cpp           # Application entry point
│   ├── transcoder.cpp     # Core transcoding logic
│   ├── audio_sync.cpp     # Audio synchronization
│   ├── progress.cpp       # Progress tracking
│   └── file_utils.cpp     # File operations
└── include/
    ├── transcoder.h       # Main transcoder interface
    ├── audio_sync.h       # Audio sync manager
    ├── progress.h         # Progress tracker
    ├── file_utils.h       # File utilities
    └── codec_settings.h   # Codec configurations
```

### Key Classes
- **VideoTranscoder**: Main orchestrator
- **AudioSyncManager**: Handles audio matching and sync testing
- **ProgressTracker**: Real-time progress with threading
- **FileUtils**: Cross-platform file operations
- **CodecSettings**: Codec parameter management

## Performance Notes

- **Multi-threading**: Progress tracking runs on separate thread
- **Memory Efficient**: Processes files sequentially to avoid memory bloat
- **FFmpeg Integration**: Leverages hardware acceleration when available
- **Smart Matching**: Optimized to test minimal audio combinations

## Troubleshooting

### Common Issues
1. **FFmpeg not found**: Ensure FFmpeg is installed and in PATH
2. **Permission errors**: Check read/write permissions on directories
3. **Codec errors**: Verify FFmpeg includes ProRes support
4. **Audio sync fails**: Check audio file formats and sample rates

### Debug Mode
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./video_transcoder  # Will show additional debug information
```

## License

This project is designed for professional video production workflows. Ensure you have appropriate licenses for any proprietary codecs used.
