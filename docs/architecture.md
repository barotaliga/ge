# Architecture

The layout gameengine2 builds toward. `docs/research.md` explains why each decision
was made, and `docs/goals.md` says in which order. Where a part is not built yet, this
page is the plan it is built to. Change this page in the same PR that changes the plan.

## The layers

```
games/fall_line/app      the demo game's shell: cards, HUD, draw systems of its own look, beds
        │
games/fall_line/sim      the game itself, headless: mapgen, the step, the bot, analysis
        │                (port of game5's engine/, deterministic, no platform, no GPU)
        ▼
engine/render  engine/audio  engine/ui     generic draw passes, the synth, the 2D layer
        │             │          │
engine/gpu     engine/platform             webgpu.h wrapper; SDL3 window, input, audio device, files
        │             │
        ▼             ▼
engine/racing  ──►  engine/core            tape, records, standings; the deterministic pool
```

One direction of dependency, held by `tools/check-agents.sh` once each layer exists:

- **`engine/core`**: the deterministic pool (port of the framework's `core/`): prng,
  math, libm, js (JS semantics helpers), noise, heightfield, polyline, quat, solar and
  output. It depends on nothing but the C++ standard library headers that do no I/O and
  no floating-point transcendentals: no `<cmath>` trig, exp or pow; `sqrt`, `fabs` and
  `floor` are allowed.
- **`engine/racing`**: tape, records and standings (port of `racing/`). Depends on
  `core`.
- **`engine/loop`**: the fixed-step run clock (port of `loop/run-clock`). Depends on
  nothing.
- **`games/fall_line/sim`**: the port of game5's `engine/`. It may include only
  `engine/core`, `engine/racing` and itself. It has no platform, no GPU and no wall
  clock. This is the line game5's `imports_test` holds, restated for C++.
- **`engine/platform`**: SDL3 behind a narrow C++ API: the window, input events, the
  gamepad and rumble, the audio device, time, storage paths and the clipboard. It is the
  only directory that includes SDL headers.
- **`engine/gpu`**: a thin RAII wrapper over `webgpu.h`: the device, the surface, buffers,
  textures, pipelines, the frame's command encoder, and timestamp queries. It is the only
  directory that includes `webgpu.h`.
- **`engine/render`**: generic draw passes Fall Line needs (the terrain clipmap,
  instanced meshes with LOD bands, skinned meshes, particles, the sky and haze, sun
  shadows, the post grade). A pass owns its GPU buffers and exposes `prepare(view)` and
  `encode(pass)`.
- **`engine/audio`**: the port of the framework's synth: voice definitions, a DSP graph
  (oscillators, noise, biquads, envelopes, echo), racks and `play_def`. It runs on the
  platform's audio stream.
- **`engine/ui`**: the immediate-mode 2D layer (rects, images, SDF text), plus Dear
  ImGui behind `GE_DEV_UI`.
- **`games/fall_line/app`**: the port of game5's `pwa/`: it builds the frame from
  `GameState`, Fall Line's own shaders and looks, its HUD and cards, and its sound beds.
- **`tools/`**: the parity oracle scripts (Node, which run the TypeScript sources),
  the headless `sim` CLI, level-map image output and the labs. Tools may include
  anything, and nothing includes a tool.
- **`tests/`**: one executable per topic.

A new game would sit beside `games/fall_line`. Until a second game exists, a generic
directory earns a piece of code only when Fall Line needs it and it carries none of
Fall Line's words (the framework's rule: name the mechanism, never one game's use of it).

## The frame

```
platform.poll()  →  input snapshot (the engine's signs: steer +right, etc.)
run_clock.frame(dt) → n steps
for n:  sim::step(state, input)                      // 120 Hz, deterministic, no allocation
extract(state, alpha) → FrameView                    // plain data, interpolated
render: prepare passes (CPU, may be parallel) → encode passes (fixed order) → submit → present
audio: beds read FrameView; the device thread pulls samples
ui:    HUD + cards drawn last
```

The simulation never sees the frame rate, and the renderer never writes `GameState`.
These are the same two rules game5 has.

## Data and code conventions (for agents and for speed)

- **Plain data.** State is POD structs with fixed-capacity arrays or `std::vector`
  reserved once. There is no inheritance in engine or game code, no virtual dispatch on
  hot paths, and no hidden global state. Module state, when unavoidable (the output
  sink), lives in one `.cpp` and is named in its header.
- **No allocation in a step or a frame.** Arenas and reserved vectors are filled at
  create time. A step that allocates is a bug, and the `perf` skill says how to prove
  it does not.
- **SoA where a loop is hot.** Trees, particles and spatial-hash cells use structure of
  arrays. Everything else uses the natural struct. Change a layout only with a
  measurement.
- **One subject per file, under 1000 lines** (`agents:allow-large-file: <reason>` for
  honest exceptions). A file opens with a comment that states what it owns and which
  game5 or framework file it ports.
- **Every number has a unit in its name or its comment** (`speed_mps`, `cell_m`), and
  every tuning number lives in its `defs/` table exactly as game5's `TUNING` does.
- **Stated once.** Each answer (the sign conventions, what the snow does, which
  generator built a map) is written in one function, and `AGENTS.md` lists where.
  A second copy is a bug.
- **Mirror game5's names.** `createGame` becomes `create_game`, and
  `engine/game/skier.ts` becomes `games/fall_line/sim/game/skier.{hpp,cpp}`. The same
  path means an agent can open the oracle beside the port.
- **Headers declare, sources define.** Small hot math is `inline` in headers. A system's
  header shows its whole API with comments, so it can be read without the `.cpp`.
- **Errors**: engine code returns status values or asserts; there are no exceptions on
  paths a frame runs.

## Diagnostics (agent-actionable output)

Every assert, error and warning the engine prints is a prompt for an agent: it names
**what** failed, **where** (file:line and the game-level context, such as seed, step
and system), and **the next action** (the test to run, the skill to load, or the doc
to read). For example:

```
ge: parity: core_math_test row 1834: hypot3 got 0x41fc7f55b85cfc98 want 0x41fc7f55b85cfc97.
    A last-bit difference means an FP contraction or a libm call crept in.
    Check -ffp-contract=off for this target; skill: determinism.
```

`engine/core/output` (M2) carries the convention. Until then, tests print in this
shape through `tests/check.hpp`.

## Determinism contract

- Only `core/libm` provides transcendentals to `sim/` code. `<cmath>`'s `sin`, `exp`
  and `pow` are forbidden there (a check enforces this from M1).
- FP flags: `-ffp-contract=off -fno-fast-math` (GCC: `-fexcess-precision=standard`;
  MSVC: `/fp:strict`), set once on `ge_flags` in the root `CMakeLists.txt`.
- Randomness only through `core::Rng` streams owned by the state. No clock in `sim/`.
- Iteration order is explicit. No `unordered_map` iteration feeds a result. Sorting
  uses `std::stable_sort`, because JS `Array.prototype.sort` is stable.
- Parity is proven by golden files the TypeScript oracle writes (`tools/parity/`), not
  by eye.

## Build

CMake ≥ 3.25 + Ninja, `CMakePresets.json` (`debug` with ASan/UBSan, `release`).
Dependencies come through `FetchContent`, pinned to a tag or commit (SDL3, wgpu-native
or Dawn, cgltf, Dear ImGui, Tracy), and each one is added in the milestone that needs
it. `Makefile` names the everyday commands. CI (`.github/workflows/ci.yml`) runs format,
agents-check, lint and the test matrix.
