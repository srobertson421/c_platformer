#!/bin/bash
# Build script for MSYS2 MinGW environment

echo "Building c_platformer in MSYS2..."

# Check if we're in MSYS2 MinGW environment
if [[ "$MSYSTEM" != "MINGW64" ]]; then
    echo "ERROR: This script should be run from MSYS2 MinGW 64-bit terminal"
    echo "Current MSYSTEM: $MSYSTEM"
    exit 1
fi

# Check for required tools
echo "Checking dependencies..."
for cmd in gcc pkg-config; do
    if ! command -v $cmd &> /dev/null; then
        echo "ERROR: $cmd not found!"
        echo "Install with: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-pkg-config"
        exit 1
    fi
done

# Check for SDL2
if ! pkg-config --exists sdl2; then
    echo "ERROR: SDL2 not found!"
    echo "Install with: pacman -S mingw-w64-x86_64-SDL2"
    exit 1
fi

if ! pkg-config --exists SDL2_image; then
    echo "ERROR: SDL2_image not found!"
    echo "Install with: pacman -S mingw-w64-x86_64-SDL2_image"
    exit 1
fi

# Clean previous build
rm -f physics_demo.exe *.o

# Get SDL flags
SDL_CFLAGS=$(pkg-config --cflags sdl2 SDL2_image)
SDL_LIBS=$(pkg-config --libs sdl2 SDL2_image)

# Compile Chipmunk2D source files
echo "Compiling Chipmunk2D..."
CHIPMUNK_SRC="Chipmunk2D/src"
CHIPMUNK_OBJS=""

# Compile all Chipmunk source files
for src in $CHIPMUNK_SRC/*.c; do
    obj=$(basename ${src%.c}.o)
    gcc -c -O2 -I"Chipmunk2D/include" "$src" -o "$obj"
    CHIPMUNK_OBJS="$CHIPMUNK_OBJS $obj"
done

# Build
echo "Compiling main program..."
gcc -Wall -Wextra -O2 $SDL_CFLAGS -I"Chipmunk2D/include" \
    main.c logging.c $CHIPMUNK_OBJS \
    -o physics_demo.exe \
    $SDL_LIBS -lm -ldbghelp -limagehlp

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Created: physics_demo.exe"
    echo ""
    echo "You can run it with: ./physics_demo.exe"
else
    echo "Build failed!"
    echo ""
    echo "If chipmunk is missing, build it with:"
    echo "  git clone https://github.com/slembcke/Chipmunk2D.git"
    echo "  cd Chipmunk2D"
    echo "  mkdir build && cd build"
    echo "  cmake .. -G 'MSYS Makefiles'"
    echo "  make && make install"
fi