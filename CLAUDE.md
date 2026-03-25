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

## Generation Roadmap

Each phase is a discrete implementation step. Implement them in order — each builds on the previous.

### Foundation — Tectonic Plates & Boundaries
The tectonic system is the substrate all generation phases build on. It produces plates, tile assignments, and classified boundary chains.

**Plates** — each plate has a center (normalized [0,1]×[0,1]), a unit drift vector, a speed (0.3–1.0), and a type (oceanic/continental). Two polar cap plates are always present: north oceanic, south continental.

**Tile assignment** — each tile is assigned to its nearest plate using AR-corrected Voronoi distance, domain-warped with FBm noise to produce organic plate shapes rather than clean geometric cells.

**Boundary chains** — grid edges where two different plates meet are grouped into chains. Each chain stores:
- A sequence of world-space points
- Per-point tangent and normal vectors (precomputed, needed by Phase 1)
- A boundary type: Convergent, Divergent, or Transform
  - Classification: `compression = dot(relVelocity, boundaryNormal)` — strongly positive = Convergent, strongly negative = Divergent, near-zero = Transform

**Debug render** — plate colors and boundary lines are baked to a texture and rendered as a visual verification step before any elevation is computed.

### Phase 1 — Tectonic Elevation
Elevation is computed from plate boundary interactions. Each boundary edge splatts an asymmetric Gaussian contribution to nearby tiles:

- `bell = exp(-d_perp²/σ_perp²) × exp(-d_along²/σ_along²)`
  - `σ_perp ≈ 3–4 tiles` — controls ridge/trench width
  - `σ_along ≈ 10–15 tiles` — controls fade along the chain
- Contribution strength = `bell × compression`
  - `compression = dot(relVelocity, boundaryNormal)` — positive at convergent boundaries (ridges), negative at divergent (trenches)
- Vary strength along each boundary chain using low-frequency noise for natural irregularity

### Phase 2 — Landmass Distribution
Landmass shape is determined by a noise field weighted by plate type:

- `land_prob = plate_bias + fbm_noise × 0.5`
  - Continental plates: `plate_bias = 0.7`
  - Oceanic plates: `plate_bias = 0.3`
- Tiles with `land_prob > 0.55` are land; below is ocean
- Plate type sets the regional tendency; noise determines the actual coastline shape

### Phase 3 — Coastal Elevation Falloff
Land tiles near the coast slope down smoothly toward sea level:

- BFS from ocean tiles computes `dist_to_ocean` for each land tile
- `elevation -= smoothstep(0, k, dist_to_ocean)` pulls coastal tiles toward the waterline
- The `dist_to_ocean` lookup is domain-warped using low-frequency FBm noise to produce irregular coastline shapes

### Phase 4 — Highland Surface Detail
Highland and mountain areas receive secondary ridge texture from a cellular noise field:

- CellularDistance noise (FastNoiseLite: Cellular, Distance mode), domain-warped for variation
- Masked to elevated terrain: contribution = `noise × smoothstep(0.55, 0.75, elevation)`
- Produces rocky surface texture and secondary ridgelines within mountain ranges

### Phase 5 — Erosion
Two erosion passes refine the terrain into natural-looking landforms:

1. **Hydraulic (droplet) erosion** — ~200,000 droplets follow steepest-descent paths, carving valleys and depositing sediment in low-energy areas
2. **Thermal erosion** — smoothing pass that redistributes material from steep slopes; run after hydraulic erosion

### Generation Pipeline Order
```
// Foundation
generatePlates()            ← plate centers, drift, speed, type
assignTilePlates()          ← Voronoi + domain warp
buildBoundaryChains()       ← classify type; precompute tangent/normal per point
bakePlateTexture()          ← debug render: plate colors + boundary lines

// Phase 1
computeElevation()          ← anisotropic bells per boundary edge

// Phase 2
distributeLandmass()        ← plate bias + noise threshold

// Phase 3
buildCoastline()            ← BFS dist-to-ocean + domain warp falloff

// Phase 4
applySecondaryRidges()      ← cellular noise masked to highlands

// Phase 5
applyHydraulicErosion()     ← droplet erosion
applyThermalErosion()       ← smoothing pass

// Climate & biomes
computeClimate()            ← temperature + moisture
classifyBiomes()            ← classifyTerrain(e, t, m)
bakeTextures()              ← heightmap / terrain render targets
```
