# Proposal

## Why

The repository is empty and the game exists only as a plan: a space freighter
captain sim where the player walks a city-sized ship rendered as a true ASCII
3D framebuffer, steers it from the bridge between ports, and runs its
economy. This change delivers that MVP as a C17/SDL3/OpenGL application with
a fully deterministic, headless-testable core.

## What Changes

- New C17 project with CMake build: native (Linux/Windows-portable) and web
  (emscripten) presets; SDL3 and cJSON vendored, Unity test framework.
- Software 3D rasterizer that renders a tile level into a 160x90 character
  grid (glyph ramp + RGB332 colors + per-cell depth) with billboard sprites.
- Seeded deterministic ship generation: 5 decks (command, habitation, cargo,
  engineering, hydroponics), corridor skeleton, carved rooms, auto doors,
  turbolift, airlock, bridge/engineering/cargo console anchors, prop sprites.
- Fixed-tick deterministic world simulation (240 ticks/day, PCG32 RNG):
  ports, markets with supply/demand price drift, cargo contracts with
  deadlines and penalties, running costs (fuel, wages, insurance, wear,
  loan interest), engine upgrades, bankruptcy.
- First-person walking with collision and auto-opening doors; station
  concourse maps when docked; airlock transitions between ship and station.
- Bridge navigation console (star map, course plotting, time compression
  0/1/4/16/64x) and a real-time docking approach sequence with thruster
  physics.
- Versioned JSON save/load including RNG state; autosave on docking.
- Automated testing: Unity unit tests, scripted headless game scenarios via
  a scenario runner (input scripts + assertions), golden ASCII frame
  comparisons, all wired into CTest.

## Capabilities

### New Capabilities
- `ascii-renderer`: character-cell framebuffer and ASCII 3D rasterization
- `ship-interior`: seeded ship deck generation and first-person traversal
- `world-sim`: deterministic tick simulation of time, travel, and ship state
- `economy`: markets, contracts, costs, loans and upgrades
- `bridge-navigation`: course plotting, time compression, docking sequence
- `save-load`: versioned JSON save games
- `platform-layer`: SDL3/OpenGL window, input, glyph blit, emscripten loop
- `test-harness`: unit tests, scenario runner, golden frames via CTest

### Modified Capabilities

None (greenfield project).

## Impact

- New code under `src/sim`, `src/render`, `src/game`, `src/platform`,
  `tools/`, `tests/`; vendored dependencies under `third_party/`.
- Build requires CMake >= 3.24, a C17 compiler, and (game target only)
  SDL3 3.2.20 via the vendored tarball; web build requires emscripten.
- Deviation from the original plan: static game data (ports, commodities,
  ship manifest) is compiled into C tables instead of `data/*.json`; JSON is
  used for save games only. Docking renders as a 2D top-down ASCII view
  rather than an external 3D camera view.
