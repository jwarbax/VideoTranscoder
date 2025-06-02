#!/bin/bash

echo "Installing FFmpeg Development Dependencies"
echo "========================================="

# Detect OS
if [ -f /etc/fedora-release ]; then
    OS="fedora"
elif [ -f /etc/redhat-release ]; then
    OS="rhel"
elif [ -f /etc/debian_version ]; then
    OS="debian"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    OS="unknown"
fi

echo "Detected OS: $OS"
echo ""

case $OS in
    "fedora"|"rhel")
        echo "Installing on Fedora/RHEL/CentOS..."
        
        # Install basic build tools first
        sudo dnf install -y cmake gcc-g++ pkg-config
        
        # Check if RPM Fusion is already enabled
        if ! dnf repolist enabled | grep -q rpmfusion; then
            echo ""
            echo "Enabling RPM Fusion repositories for full FFmpeg support..."
            sudo dnf install -y https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
            sudo dnf install -y https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm
        fi
        
        # Handle FFmpeg conflicts by replacing free versions with full versions
        echo ""
        echo "Resolving FFmpeg package conflicts..."
        echo "This will replace limited 'free' FFmpeg packages with full-featured versions."
        
        # Remove conflicting free packages and install full FFmpeg
        sudo dnf swap -y ffmpeg-free ffmpeg --allowerasing
        sudo dnf swap -y libavcodec-free libavcodec --allowerasing
        sudo dnf swap -y libavformat-free libavformat --allowerasing
        sudo dnf swap -y libavutil-free libavutil --allowerasing
        sudo dnf swap -y libswresample-free libswresample --allowerasing
        sudo dnf swap -y libswscale-free libswscale --allowerasing
        
        # Install development packages
        sudo dnf install -y ffmpeg-devel
        ;;
        
    "debian")
        echo "Installing on Ubuntu/Debian..."
        sudo apt update
        sudo apt install -y cmake g++ pkg-config
        
        # Try to install FFmpeg dev libraries
        sudo apt install -y libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev
        
        # If that fails, add universe repository
        if ! dpkg -l | grep -q libavformat-dev; then
            echo ""
            echo "Adding universe repository for FFmpeg..."
            sudo add-apt-repository universe
            sudo apt update
            sudo apt install -y libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev
        fi
        ;;
        
    "macos")
        echo "Installing on macOS..."
        
        # Check if Homebrew is installed
        if ! command -v brew &> /dev/null; then
            echo "Homebrew not found. Installing Homebrew first..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        brew install cmake ffmpeg pkg-config
        ;;
        
    *)
        echo "Unknown OS. Please install manually:"
        echo ""
        echo "Required packages:"
        echo "- cmake (3.20+)"
        echo "- C++20 compiler (g++/clang++)"
        echo "- FFmpeg development libraries:"
        echo "  - libavformat-dev"
        echo "  - libavcodec-dev" 
        echo "  - libavutil-dev"
        echo "  - libswscale-dev"
        echo "  - libswresample-dev"
        echo "- pkg-config"
        exit 1
        ;;
esac

echo ""
echo "Verifying installation..."

# Check for required tools
echo -n "Checking cmake... "
if command -v cmake &> /dev/null; then
    echo "✓ $(cmake --version | head -1)"
else
    echo "✗ Not found"
fi

echo -n "Checking compiler... "
if command -v g++ &> /dev/null; then
    echo "✓ $(g++ --version | head -1)"
elif command -v clang++ &> /dev/null; then
    echo "✓ $(clang++ --version | head -1)"
else
    echo "✗ No C++ compiler found"
fi

echo -n "Checking pkg-config... "
if command -v pkg-config &> /dev/null; then
    echo "✓ $(pkg-config --version)"
else
    echo "✗ Not found"
fi

echo -n "Checking FFmpeg libraries... "
if pkg-config --exists libavformat libavcodec libavutil; then
    echo "✓ Found via pkg-config"
    echo "  FFmpeg version: $(pkg-config --modversion libavformat)"
else
    echo "✗ Not found via pkg-config"
    echo ""
    echo "Checking for FFmpeg executables as fallback..."
    
    if command -v ffmpeg &> /dev/null && command -v ffprobe &> /dev/null; then
        echo "✓ ffmpeg and ffprobe found in PATH"
        echo "  FFmpeg version: $(ffmpeg -version 2>/dev/null | head -1)"
        echo "  Build will use system command mode"
    else
        echo "✗ ffmpeg/ffprobe not found in PATH either"
        echo ""
        echo "Something went wrong with the installation."
        echo "Try manually: sudo dnf install ffmpeg ffmpeg-devel --allowerasing"
    fi
fi

echo ""
echo "Dependencies installation complete!"
echo ""
echo "If you encountered conflicts, they should now be resolved."
echo "You can now run: ./build.sh"
