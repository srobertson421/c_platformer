#!/bin/bash

# Install dependencies for the C Platformer project
# Supports: Ubuntu/Debian, Fedora, Arch, macOS, and MSYS2/MinGW

set -e

echo "Installing dependencies for C Platformer (SDL2 + Chipmunk2D)..."

# Detect the operating system
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    if [ -f /etc/debian_version ]; then
        # Debian/Ubuntu
        echo "Detected Debian/Ubuntu system"
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            libsdl2-dev \
            libsdl2-image-dev \
            libchipmunk-dev \
            pkg-config
            
    elif [ -f /etc/fedora-release ] || [ -f /etc/redhat-release ]; then
        # Fedora/RHEL
        echo "Detected Fedora/RHEL system"
        sudo dnf install -y \
            gcc \
            gcc-c++ \
            make \
            cmake \
            SDL2-devel \
            SDL2_image-devel \
            chipmunk-devel \
            pkg-config
            
    elif [ -f /etc/arch-release ]; then
        # Arch Linux
        echo "Detected Arch Linux system"
        sudo pacman -S --needed --noconfirm \
            base-devel \
            cmake \
            sdl2 \
            sdl2_image \
            chipmunk \
            pkg-config
            
    else
        echo "Unsupported Linux distribution"
        echo "Please install the following packages manually:"
        echo "- GCC/Clang compiler"
        echo "- CMake"
        echo "- SDL2 development libraries"
        echo "- SDL2_image development libraries"
        echo "- Chipmunk2D development libraries"
        echo "- pkg-config"
        exit 1
    fi
    
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    echo "Detected macOS system"
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Homebrew is not installed. Please install it first:"
        echo "https://brew.sh"
        exit 1
    fi
    
    echo "Installing dependencies via Homebrew..."
    brew install \
        cmake \
        sdl2 \
        sdl2_image \
        chipmunk \
        pkg-config
        
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "mingw"* ]]; then
    # Windows with MSYS2/MinGW
    echo "Detected Windows with MSYS2/MinGW"
    
    # Update package database
    pacman -Syu --noconfirm
    
    # Install dependencies
    pacman -S --needed --noconfirm \
        mingw-w64-x86_64-gcc \
        mingw-w64-x86_64-cmake \
        mingw-w64-x86_64-make \
        mingw-w64-x86_64-SDL2 \
        mingw-w64-x86_64-SDL2_image \
        mingw-w64-x86_64-chipmunk \
        mingw-w64-x86_64-pkg-config
        
elif [[ "$OSTYPE" == "cygwin" ]]; then
    # Cygwin
    echo "Detected Cygwin"
    echo "Please install the following packages using Cygwin setup:"
    echo "- gcc-core"
    echo "- gcc-g++"
    echo "- make"
    echo "- cmake"
    echo "- libSDL2-devel"
    echo "- libSDL2_image-devel"
    echo "- pkg-config"
    echo ""
    echo "Note: Chipmunk2D may need to be built from source on Cygwin"
    exit 1
    
else
    echo "Unsupported operating system: $OSTYPE"
    echo "Please install dependencies manually"
    exit 1
fi

echo ""
echo "Dependencies installed successfully!"
echo "You can now build the project with:"
echo "  make"
echo "or"
echo "  mkdir build && cd build && cmake .. && make"