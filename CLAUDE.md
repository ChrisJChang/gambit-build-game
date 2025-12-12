# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

ClaudeGame is an OpenGL-based graphical application built with C++ and GLFW. The project uses modern OpenGL (3.3 core profile) with GLSL shaders for rendering.

## Development Commands

### Building the Project
```bash
make              # Build the executable
make clean        # Remove build artifacts
make run          # Build and run the game
```

### Dependencies
- GLFW3 (windowing and OpenGL context)
- GLEW (OpenGL extension loader)
- FreeType (font rendering)
- OpenGL 3.3+
- C++17 compiler (g++)

On Ubuntu/Debian, install dependencies with:
```bash
sudo apt-get install libglfw3-dev libgl1-mesa-dev libglew-dev libfreetype-dev
```

### Running the Game
The game requires two command line arguments: the red text file and the green text file.

```bash
./game <red_text_file> <green_text_file>
```

**Example:**
```bash
./game test_text.cpp test_text2.cpp
```

Or use the Makefile:
```bash
make run              # Runs with default files: test_text.cpp test_text2.cpp
```

**Controls:**
- **Left/Right Arrow Keys**: Move the grey square left and right
- **ESC**: Close the window

**Gameplay:**
- **Health Bar** displayed at the top of the screen shows player health (0-100)
  - **Green** when health > 66%
  - **Yellow** when health > 33%
  - **Red** when health ≤ 33%
  - **Damage**: Colliding with **red text** deals 5 damage every 0.5 seconds
  - **Safe**: Colliding with **green text** does not deal damage
- **Game Over**: When health reaches 0, "Git Gud" message appears in the center of the screen in red
  - Text spawning stops when game over
  - No more damage can be taken
- Every 5 seconds, text spawns and falls down the screen
- The game alternates between two text files (specified as command line arguments):
  - First argument (red text file) - spawns **red** text (DANGEROUS - deals damage!)
  - Second argument (green text file) - spawns **green** text (SAFE)
  - Default files: `test_text.cpp` (red) and `test_text2.cpp` (green)
- Each file cycles through its lines sequentially (not just the last line)
  - After reaching the last line, wraps back to the first line
  - Comment lines (starting with //) are skipped
- **Files are reloaded every 10 seconds** - edit the text files while the game is running to see new content!
- The player square turns **red** when it collides with any falling text
- Text is automatically removed when it falls off the bottom
- Window size: 1200x1200 pixels

## Code Architecture

The application uses a single-file architecture (`main.cpp`) with the following structure:

- **Program Initialization**:
  - Accepts two command line arguments: red text file path and green text file path
  - Displays usage message if arguments are missing
  - File paths are stored and used throughout the game for loading and reloading text
- **Shader System**:
  - Geometry shaders for rendering colored squares with offset support
  - Text shaders for rendering textured font glyphs with orthographic projection
- **Text Rendering**:
  - FreeType library loads DejaVu Sans font at 48pt
  - Characters are pre-rendered as textures stored in a map
  - Text is rendered using textured quads with alpha blending
- **Rendering Pipeline**: Modern OpenGL with VAOs, VBOs, and EBOs for geometry management
- **Window Management**: GLFW handles window creation, input processing, and the main render loop
- **Geometry**:
  - Outer white square border
  - Inner black center square
  - Small grey player square (movable with arrow keys)
  - Health bar (background and dynamic foreground at top of screen)
  - Dynamic falling text objects
- **Game Loop**:
  - Delta time calculation for smooth frame-rate independent movement
  - Player health system (0-100) with dynamic health bar visualization
  - Health bar color changes based on current health percentage
  - Damage system: 5 HP lost every 0.5 seconds when colliding with red text
  - Damage timer prevents constant health drain, resets when not colliding
  - Game over state triggered when health reaches 0
    - Text spawning halted
    - "Git Gud" message displayed in center (red, large scale 1.5)
    - Damage system disabled
  - Timer system spawns falling text every 5 seconds, alternating between the two files specified as command line arguments (red file and green file)
  - Text spawning only occurs when game is not over
  - Files are reloaded every 10 seconds from the command line argument paths, allowing live editing during gameplay
  - Lines are cycled through sequentially with wraparound
  - Each file maintains its own line index, incrementing after each spawn
  - If a file becomes shorter after reload, indices are clamped to prevent out-of-bounds access
  - Each text object stores its own color (RGB)
  - AABB collision detection between player square and all falling text
  - Collision detection updated for 1200x1200 window and smaller player size (0.05)
  - Collision detection differentiates between red (damaging) and green (safe) text
  - Player square changes color to red when colliding with any text
  - Falling text objects are updated and culled when off-screen
