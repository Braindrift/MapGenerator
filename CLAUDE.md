# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

### Vision
Project MapGenerator is the first step towards building a game.
Game is to be considered a thin shell, our current focus is to create a map generator.
Procedural map generator inspired by Dwarf Fortress, using a grid-based map with tile data (terrain, elevation, etc.).
The MapGenerator is responsible for creating a world on which gameplay systems will later read from and write to.

### Direction
The world will render as tile-based with pixel graphics.
Built in C++ with a clear separation between world data and rendering (SFML), supporting scalable world size generation and visualization.
Designed for deterministic generation using seeds and extensible systems for terrain, biomes, and simulation.
Key systems: height maps, climate, biomes, river simulation.
World data and simulation logic should remain decoupled from rendering.
Additional libraries shoulde be added via FetchContent in CMakeLists.txt

## Build

This project uses CMake with Ninja and MSVC (cl.exe). SFML 3.0.0 is fetched automatically via `FetchContent` at configure time — no manual dependency installation needed.

**Configure and build (debug):**
```bash
cmake --preset x64-debug -S MapGenerator
cmake --build MapGenerator/out/build/x64-debug
```

**Run:**
```bash
./MapGenerator/out/build/x64-debug/MapGenerator.exe
```

Available presets: `x64-debug`, `x64-release`, `x86-debug`, `x86-release`.

There are no tests.

## Architecture

The source lives under `MapGenerator/` (the inner directory is the actual CMake project root):

- `source/` — `.cpp` implementation files
- `headers/` — `.h` header files; added to the include path via `target_include_directories`

**Data flow:** `main()` → `Game` → `World` → `Tile`

- **Game** owns the `sf::RenderWindow` and `World`. Its `run()` method drives the event loop: poll events → clear → `world.draw()` → display.
- **World** holds a flat `std::vector<Tile>` indexed as `y * width + x`. `initialize()` sets tile colors; `draw()` renders each tile as an `sf::RectangleShape` with 1px spacing between tiles.
- **Tile** is a plain data struct holding only an `sf::Color`.

When adding new tile properties or world logic, `Tile` is extended and `World::initialize()` / `World::draw()` are the two methods to modify.
