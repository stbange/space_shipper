# Space Freighter Captain Game — Implementation Plan

## Summary

A first-person space freighter captain sim in C17: walk a city-sized, procedurally generated ship rendered as a true ASCII 3D framebuffer, steer it from the bridge (course plotting + real-time docking), and run its economy (contracts, markets, running costs, loan). Targets: Linux native, Web via emscripten, Windows (portable code, preset provided). Spec-driven via openspec; deterministic fixed-tick sim makes the whole game automated-testable.

## Resolved decisions

| Decision | Choice |
|---|---|
| Stack | C17 + SDL3 + OpenGL 3.3/WebGL2; SDL3 vendored via CMake FetchContent |
| Renderer | True ASCII 3D framebuffer: software 3D rasterizer → character grid (glyph ramp + color, per-cell depth), blitted as instanced textured quads |
| Perspective | First-person 3D inside the ship (WASD + mouse look) |
| Ship layout | Seeded deterministic procedural decks + handcrafted anchor rooms (bridge, engineering, cargo control, market); turbolift/maglev fast travel |
| Steering | Bridge nav console: star map, course plotting, time compression (pause/1×/8×/32×); real-time manual docking sequence in ASCII 3D |
| Economy | Cargo contracts + per-port markets with supply/demand price drift; costs: fuel, crew wages, maintenance, insurance; bank loan; upgrades |
| Time model | Fixed-tick deterministic sim: 1 tick = 6 game minutes (240 ticks/day); seeded PCG32 RNG; input → fixed ticks → render |
| Testing | Unity (ThrowTheSwitch) vendored in `third_party/`, CTest; headless scenario runner; golden-frame ASCII snapshots |
| Openspec | Full workflow: capability specs + one change per milestone, `openspec validate --strict` |
| MVP | Full trading loop: walk to bridge → plot course → travel → dock → buy/accept contract → load cargo → deliver → get paid → pay costs; save/load |

## Environment facts (verified)

- gcc, cmake, make, node/npm, emscripten (`/opt/emsdk/upstream/emscripten/emcc`), and `openspec` CLI are installed.
- SDL3 is NOT installed system-wide → vendor via CMake `FetchContent`.
- No MinGW/clang → Windows is code-portable + preset/CI recipe only; local validation = Linux + Web.

## Architecture

```
space_shipper/
  CMakeLists.txt            # top-level
  CMakePresets.json         # presets: linux (release), web (emscripten), tests (headless)
  src/
    sim/                    # library: freighter_sim — PURE C, NO SDL includes
      rng.c                 #   PCG32 seeded
      tick.c                #   game clock, speed control (pause/1x/8x/32x)
      world.c               #   galaxy: 1 system, ~4 ports, positions
      shipgen.c             #   seeded deck/district generation from manifest + anchor templates
      travel.c              #   course legs, fuel burn, wear accrual, en-route events
      economy.c             #   commodities, market price drift, contracts, costs, loan
      cargo.c               #   hold capacity, loading/unloading
      save.c                #   JSON save/load (versioned)
    render/                 # library: freighter_render — no SDL; writes to a char grid
      grid.c                #   CharGrid {glyph, fg, bg, depth} default 160x90
      raster3d.c            #   camera/projection, per-cell depth, brightness→glyph ramp
      sprites.c             #   billboard glyphs for NPCs/objects/props
      ui.c                  #   immediate-mode ASCII panels (nav, market, contracts, HUD)
    platform/               # library + app: freighter_platform
      sdl_app.c             #   window, input (keyboard, mouse look, pointer lock), OpenGL blit
      font.c + font_data.h  #   embedded public-domain 8x16 VGA-style bitmap font (atlas texture)
      emscripten glue       #   emscripten_set_main_loop, canvas resize
    game/
      states.c              #   mode switch: walk / bridge-console / star map / docking / market
      main.c
  data/                     # JSON: ship_manifest.json, commodities.json, ports.json
  third_party/              # unity/, sjson/ (vendored single-file libs); SDL3 via FetchContent
  tests/
    unit/                   # Unity tests: rng, shipgen reproducibility, economy, travel, save round-trip
    scenarios/              # headless game tests: input script + tick counts + world assertions
    golden/                 # recorded ASCII frames + maps; diffed by renderer tests
  tools/                    # golden_frame_dump.c (render scene → text file for recording/diffing)
  openspec/                 # project.md, specs/<capability>/spec.md, changes/<milestone>/
```

Key rules for the implementer:
- `freighter_sim` must never include SDL or OpenGL headers — this is what makes headless game tests and determinism possible.
- All randomness goes through the seeded PCG32; no `rand()`, no time-based logic in sim.
- Floats allowed only in `render/` and the docking sub-mode; sim state uses doubles/ints with fixed tick arithmetic.
- Determinism contract: same seed + same input script + same tick count ⇒ identical world state (and identical golden frames, native builds).

## Openspec setup (first task of every milestone)

1. `openspec init` (choose non-interactive flags if available; otherwise answer prompts) → `openspec/project.md`.
2. Create capability specs: `world-sim`, `ascii-renderer`, `ship-interior`, `bridge-navigation`, `economy`, `save-load`, `platform-layer`, `test-harness`.
3. Per milestone: create `openspec/changes/<milestone-id>/` (proposal.md, design.md, tasks.md) BEFORE implementing; `openspec validate --strict` must pass; archive change into specs when the milestone's tests pass.

## Milestones (ordered tasks)

### M0 — Scaffold
- CMake + presets (linux / web / tests); C17, `-Wall -Wextra -Werror`; static libs as above.
- Vendor unity + sjson into `third_party/`; SDL3 via FetchContent (fallback: commit a release tarball if network is unavailable at configure time).
- Empty SDL3 window; font atlas texture; clear the char grid and blit it (native + web).
- `openspec init` + capability skeletons. CTest wired: one trivial unit test + `openspec validate --strict` as a test.

### M1 — ASCII 3D renderer
- `grid.c` + `raster3d.c`: camera (pos/yaw/pitch), perspective projection, wall/floor/ceiling quads rasterized into per-cell depth + brightness → 10-level glyph ramp (` .:-=+*#%@`) + flat colors.
- Embedded 8x16 bitmap font; instanced quad blit via OpenGL 3.3 (WebGL2 on emscripten).
- First-person movement + grid collision in a hand-built test map.
- `tools/golden_frame_dump` + 3 golden frame tests (fixed camera positions/orientations).

### M2 — Ship generation & walking
- `data/ship_manifest.json`: districts (bridge, habitation, cargo bays, engineering, hydroponics, market, transit), deck counts, sizes.
- `shipgen.c`: seeded layout — corridors, rooms, doors; anchor rooms from handcrafted templates; deterministic: same seed ⇒ same ship (unit test).
- Turbolift/maglev fast-travel UI between districts; deck-map overlay (bonus: switchable top-down view).
- Game state `walk` reachable from main menu → spawn in captain's quarters → walk to bridge.

### M3 — Sim core & save/load
- `tick.c`, `rng.c`, game clock (day/hour), speed control; sim decoupled from frame rate (fixed ticks per frame by speed setting).
- Ship state (fuel, wear, hold, money, crew), JSON save/load round-trip tests; save file: `savegame.json` (SDL pref path; headless tests use temp dir); autosave on dock.

### M4 — Galaxy, bridge, travel
- `world.c` + `data/ports.json`: 1 star system, ~4 ports (planet cities/orbital stations), positions in AU.
- Bridge consoles (interact → ASCII UI screens): nav/star map (course selection, fuel/days estimate), engineering (status, repairs), cargo manifest.
- `travel.c`: course legs, fuel burn, wear accrual, en-route event checks (seeded); time compression active during travel; arrival → orbit state.
- UI for market terminals + contract board at ports (walkable locations when docked).

### M5 — Docking sequence
- Real-time sub-mode (60 fixed sub-ticks/sec, main sim paused): external ASCII 3D view of station + freighter; translation thrusters + rotation; alignment + clamp logic; collision failure states (hull damage, retry).
- Undock sequence. Golden frames for a fixed scripted docking approach.

### M6 — Economy full loop
- `economy.c` + `data/commodities.json` (~8 commodities): per-port inventory, price drift toward supply/demand equilibrium (deterministic); contract generation per port per day (seeded); deadlines, penalties.
- Costs: fuel per leg, daily wages, maintenance/wear, insurance; bank loan (interest per day).
- Cargo loading at docked ports (walk to cargo bay / market terminal UIs); delivery payout on arrival + contract completion.
- First automated end-to-end game test: scenario script completes one delivery loop and asserts profit/loss accounting.

### M7 — Polish & ships
- HUD polish, controls help, title menu, save slots, game-over (bankruptcy) state.
- Web build validated in browser (pointer lock, main loop, asset loading); ship upgrades (engines/fuel/hold) spending profits.
- Manual playtest checklist; final `openspec` archive of all changes.

## Validation

- Every milestone: `cmake --preset tests && ctest --preset tests` (unit + scenario + golden) and `openspec validate --strict`.
- Unit tests: RNG determinism, shipgen reproducibility (seed equality ⇒ identical layout hash), economy invariants (money conservation, price bounds), travel math, save round-trip.
- Game tests (headless runner, native only): scripted inputs + tick counts → world assertions; includes full-delivery-loop scenario.
- Golden-frame tests: renderer snapshots diffed against `tests/golden/` (native only; regenerate with the dump tool when intentionally changing rendering).
- Web: build with the `web` preset, load in a browser, run the manual smoke checklist (walk, bridge, travel, dock, trade).
- Windows: compile-clean portability rules only here; add CI recipe (GitHub Actions windows-latest + vcpkg SDL3) — cannot compile locally (no MinGW).

## Risks & mitigations

- **SDL3 FetchContent needs network at configure time** → pre-fetch release tarball into `third_party/` and let CMake prefer it.
- **emscripten/SDL3 compatibility** → keep to core SDL3 APIs + OpenGL ES/WebGL2-compatible subset; test the web build every milestone from M0 on, not at the end.
- **ASCII 3D readability** → tune glyph ramp + colors early (M1); make cell size and grid resolution adjustable.
- **Docking frustration** → generous alignment tolerances, clear HUD indicators, no permanent ship loss on failed attempts (hull damage only).
- **Determinism drift** → forbid `rand()`/wall-clock in sim via code review + a grep-based test; sim tests run only on native.

## Out of scope (later)

- Multiple star systems, rival NPC traders, deep production-chain economy, crew hiring/management sim, gamepad support, audio, ship-to-ship combat, ship interior redecoration.
