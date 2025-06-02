#!/bin/bash

echo "Building Video Transcoder"
echo "========================"

# Check if dependencies are available
echo "Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    echo "❌ cmake not found! Run: ./install_deps.sh"
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "❌ C++ compiler not found! Run: ./install_deps.sh"
    exit 1
fi

echo "✅ Basic build tools found"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo ""
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ CMake configuration failed!"
    echo ""
    echo "If you're missing FFmpeg development libraries, try:"
    echo "  ./install_deps.sh"
    echo ""
    echo "Or install manually:"
    echo "  Fedora/RHEL: sudo dnf install ffmpeg-devel"
    echo "  Ubuntu/Debian: sudo apt install libavformat-dev libavcodec-dev libavutil-dev"
    echo "  macOS: brew install ffmpeg"
    exit 1
fi

# Build
echo ""
echo "Building..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo ""
echo "🎉 Build successful!"
echo ""
echo "Executable: $(pwd)/video_transcoder"
echo ""
echo "To test:"
echo "  ./video_transcoder --help"
echo ""
echo "To install system-wide:"
echo "  sudo make install"
echo ""
echo "To run with your files:"
echo "  cd /path/to/your/forTranscoding/folder"
echo "  /path/to/build/video_transcoder"
