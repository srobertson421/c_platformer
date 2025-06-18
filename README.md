# Chipmunk2D + SDL2 Box Collision Demo

A simple C demonstration of physics simulation using Chipmunk2D physics engine and SDL2 for rendering.

## Features
- **Animated Player Sprite**: Player box replaced with animated 2D sprite
- **Animation System**: State-based animations (idle, walk, jump)
- **Player Control**: Move and jump with the first box (platformer-style)
- Interactive box spawning with mouse clicks
- Multiple boxes with realistic physics interactions
- Static ground collision with ground detection
- Debug visualization toggle to show physics body outlines
- Basic physics simulation with proper timestep
- SDL2 rendering at 60 FPS with sprite support
- Support for up to 100 simultaneous boxes

## Prerequisites

Install the required libraries:

### Ubuntu/Debian:
```bash
sudo apt-get install libsdl2-dev libsdl2-image-dev libchipmunk-dev cmake build-essential
```

### Fedora:
```bash
sudo dnf install SDL2-devel SDL2_image-devel chipmunk-devel cmake gcc
```

### macOS (with Homebrew):
```bash
brew install sdl2 sdl2_image chipmunk cmake
```

## Building

### Using Make:
```bash
make
```

### Using CMake:
```bash
mkdir build
cd build
cmake ..
make
```

## Running
```bash
./physics_demo
```

Or with make:
```bash
make run
```

## Controls

### Player Movement (First Box)
- **A / Left Arrow**: Move left
- **D / Right Arrow**: Move right  
- **W / Up Arrow / Spacebar**: Jump (only when on ground)

### Game Controls
- **Left Mouse Button**: Click anywhere to spawn a new box at that location
- **F1 Key**: Toggle debug visualization to show/hide physics body outlines
  - Yellow outlines: Dynamic bodies (boxes)
  - Green outlines: Static bodies (ground)
- **Close Window**: Click the X button to quit the application

## What it does
- Starts with one red box at the top of the screen (this is the player)
- **Control the first box** with WASD/Arrow keys for platformer-style movement
- **Ground detection**: Player can only jump when touching the ground
- **Physics-based movement**: Uses forces and impulses for realistic feel
- Click anywhere to spawn additional boxes at the mouse position
- All boxes fall due to gravity (-980 units/s²) and interact with each other
- Boxes collide with each other and the gray ground at the bottom
- Press 'F1' to toggle debug visualization showing exact physics body shapes
- Physics simulation runs at 60 FPS with proper delta time
- Supports up to 100 boxes simultaneously for performance

### Movement Physics
- **Horizontal movement**: Applied as forces with speed limiting and damping
- **Jumping**: Applied as impulse only when grounded
- **Ground detection**: Checks position and vertical velocity
- **Speed limits**: Prevents infinite acceleration