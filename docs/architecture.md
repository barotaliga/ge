# Architecture

The layout gameengine2 builds toward, and the decisions behind it (§ Decisions).
`docs/goals.md` says in which order. Where a part is not built yet, this
page is the plan it is built to. Change this page in the same PR that changes the plan.

## The layers

```
games/fall_line/app      the demo game's shell: cards, HUD, its own look, the bank
        │
games/fall_line/sim      the game itself, headless: create_game, the step's order, the modes,
        │                the bot (M5; deterministic, no platform, no GPU)
        ▼
plugins/<name>           reusable by any game of one genre or world (§ Plugins):
        │                racing (tape, records, standings), mountain (mapgen, analysis),
        │                skier (the skier on the snow: step_skier, snow, limits)
        ▼
engine/render  engine/audio  engine/ui     generic draw passes, the synth, the 2D layer
        │             │          │
engine/gpu     engine/platform             webgpu.h wrapper; SDL3 window, input, audio device, files
        │             │
        ▼             ▼
engine/core  engine/loop                   the deterministic pool; the run clock
```

One direction of dependency, held by `tools/check-agents.sh` once each layer exists:

- **`engine/core`**: the deterministic pool: prng, math, libm, num (the number semantics
  helpers), noise, heightfield, quat, solar and output. It depends on nothing but the C++
  standard library headers that do no I/O and
  no floating-point transcendentals: no `<cmath>` trig, exp or pow; `sqrt`, `fabs` and
  `floor` are allowed.
- **`engine/loop`**: the fixed-step run clock. Depends on nothing.
- **`plugins/<name>`**: a mechanism one genre or world shares (§ Plugins). Its simulation
  half may include only the engine targets and plugins its `ge_add_plugin` call declares.
- **`games/fall_line/sim`**: Fall Line's own simulation. It may
  include only `engine/core`, `engine/loop`, the plugins it links and itself. It has no platform, no GPU and no wall
  clock.
- **`engine/platform`**: SDL3 behind a narrow C++ API: the window and its native handles,
  input events and the frame's monotonic clock (built, M6), then the gamepad and rumble,
  touch, the audio device and storage paths as their milestones need them. It is the only
  directory that includes SDL headers, and it hands the GPU a plain `NativeWindow` struct.
- **`engine/gpu`**: the native GPU device over Dawn (built, M6): the device and its feature
  tiers, error lines, shader modules from embedded WGSL, the pipeline manifest and its
  creation counter, the surface, offscreen targets and readback, and per-pass timestamps.
  It uses Dawn's `webgpu_cpp.h` handles (already reference-counted RAII) directly rather
  than wrapping them again, so every call reads as Dawn's API spells it. It is the
  only directory that includes `webgpu.h`.
- **`engine/render`**: generic rendering mechanisms (the pass table and named targets,
  GPU-driven instance families, the heightfield clipmap, shadows, the sky tables and
  clouds, TAA-U, post, compute particles, skinning). A pass owns its GPU buffers and
  exposes `prepare(view)` and `encode(pass)`. **`docs/rendering.md` is the plan** for
  this layer and `engine/gpu`.
- **`engine/audio`**: the synth: voice definitions, a DSP graph
  (oscillators, noise, biquads, envelopes, echo), racks and `play_def`. It runs on the
  platform's audio stream.
- **`engine/ui`**: the immediate-mode 2D layer (rects, images, SDF text), plus Dear
  ImGui behind `GE_DEV_UI`.
- **`games/fall_line/app`**: Fall Line's presentation: it builds the frame from
  `GameState`, Fall Line's own shaders and looks, its HUD and cards, and its sound beds.
- **`engine/lab`**: the lab contract and the `labs` runner (`docs/labs.md`): how any
  subsystem, plugin or game declares a lab, the CPU raster and PNG writer its pictures are
  drawn with, and the report, contact sheet and digest a run leaves. Labs themselves sit in
  a `labs/` directory beside their subject (`engine/lab/labs/`, `plugins/<name>/labs/`,
  `games/<name>/labs/`) and are compiled into one `labs` executable. A lab may include
  anything, and nothing includes a lab. Lab code keeps the simulation's numeric rules (no
  `<cmath>` transcendental, `core::Rng` only), because its digest is checked on every CI row.
- **`tools/`**: the structure checks, the headless `sim` CLI, level-map image output and
  the labs. Tools may include anything, and nothing includes a tool.
- **`porting/`**: the WORKING area, deleted when porting ends (`AGENTS.md` § The working
  area): the porting guide and map, and the generators of the golden tables. Nothing
  outside it includes or names anything in it but the `make golden` targets.
- **`tests/`**: one executable per topic.

A new game would sit beside `games/fall_line` and link the plugins it shares with it.
Fall Line's needs decide WHAT is built (G4); § Plugins decides WHERE it goes (G8).

## Plugins

The engine is genre-agnostic. Every piece of code has one of three homes, decided before its
first line is written (the `plugins` skill has the questions):

| Home | Holds | Words it may use |
| --- | --- | --- |
| `engine/<module>/` | What a game of any genre would use unchanged | The mechanism's only (vehicle, checkpoint, track) |
| `plugins/<name>/` | What any game of one genre or world would use unchanged | Its world's (a ski plugin says skier, piste, gate), never one game's |
| `games/<game>/` | What makes this game this game: modes, rules, the order of the step, the bot, pinned maps, strings, the HUD, its look | Its own |

**The contract**, stated here once:

- **Layout.**
  ```
  plugins/<name>/CMakeLists.txt          ge_add_plugin(<name> ENGINE core PLUGINS racing)
  plugins/<name>/[<module>/]*.{hpp,cpp} the simulation half, a directory per module
  plugins/<name>/app/                    the presentation half (refused by the build until the
                                         first one splits it into ge_<name>_app, M7+)
  plugins/<name>/labs/                   its labs, never compiled into ge_<name>
  .agents/skills/<name>/                 its skill and lessons
  tests/<name>_<topic>_test.cpp          its tests; goldens tests/golden/<name>_<topic>.txt
  ```
- **Naming.** A plugin's name is one lower-case word, and it is the directory, the namespace
  (`ge::<name>`), the target (`ge_<name>`), the skill and the test prefix. It never equals an
  `engine/` or `games/` directory. Headers are included as `"<name>/<file>.hpp"` or, when the plugin
  has more than one module, `"<name>/<module>/<file>.hpp"`.
- **Build.** `cmake/Plugins.cmake` defines `ge_add_plugin`; the root `CMakeLists.txt` adds every
  `plugins/*/CMakeLists.txt` it finds, so there is no list to register in. Tests link every
  plugin. The plugin's targets carry the same FP flags as the core, through `ge_core`.
- **Dependencies.** A plugin's `ENGINE` and `PLUGINS` lists are its whole dependency contract.
  Games depend on plugins, plugins on the engine and on the plugins they declare, with no
  cycle; the engine never includes a plugin. `tools/check-plugins.mjs` (`make agents-check`)
  holds the includes to the lists; the compiler holds the rest, since a target only sees the
  include roots it links.
- **Registration is the build.** No run-time registry, no static-initializer
  self-registration, no virtual plugin interface: they cost a pointer chase on hot paths,
  static libraries drop unreferenced self-registering objects, and their order is
  unspecified, which breaks determinism. A plugin exposes plain functions and POD data; the
  game links `ge_<name>` and calls it from its own `create_game` and `step`, in the order it
  states once.
- **Skills and tags.** Each plugin owns the skill named after it. Every file in the plugin
  names that skill first in its `ge:skill` line, so `make find` and `docs/index.md` route to
  it. When the `ge` CLI (M12) syncs consumer skills, a consumer receives the skill of each
  plugin it links.
- **Labs.** A plugin's labs live in `plugins/<name>/labs/` and register through the engine's
  lab contract, the same way the engine's and a game's do; a lab that needs a game (the bot,
  the modes) belongs to the game.
- **Determinism.** A plugin's simulation half is simulation code: `make libm-check` holds it,
  and every rule in § Determinism contract applies.

## The frame

```
platform.poll()  →  input snapshot (the engine's signs: steer +right, etc.)
run_clock.frame(dt) → n steps
for n:  sim::step(state, input)                      // 120 Hz, deterministic, no allocation
extract(state, alpha) → FrameView                    // plain data, interpolated
render: prepare passes (CPU, may be parallel) → encode the pass table (fixed order) → submit → present
audio: beds read FrameView; the device thread pulls samples
ui:    HUD + cards drawn last
```

The simulation never sees the frame rate, and the renderer never writes `GameState`.

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
  honest exceptions). A file opens with a comment that states what it owns, in the
  engine's own terms (never where it came from: `AGENTS.md` § A clean slate).
- **Every number has a unit in its name or its comment** (`speed_mps`, `cell_m`), and
  every tuning number lives in its `defs/` table.
- **Stated once.** Each answer (the sign conventions, what the snow does, which
  generator built a map) is written in one function, and `AGENTS.md` lists where.
  A second copy is a bug.
- **One name for a module everywhere.** `plugins/mountain/mapgen/forest.{hpp,cpp}`, its
  test `tests/mountain_forest_test.cpp`, its golden `tests/golden/mountain_forest.txt`.
  Names are `snake_case`, so `create_game` is a search key for the function, its callers
  and its test.
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
ge: golden: core_math_test row 1834: hypot3 got 0x41fc7f55b85cfc98 want 0x41fc7f55b85cfc97.
    A last-bit difference means an FP contraction or a libm call crept in.
    Check -ffp-contract=off for this target; skill: determinism.
```

`engine/core/output` (`status`, `info`, `warn`, `error`, `header`, `debug`) is the one
channel every such line goes through; the host attaches the sink that prints it, and core
itself does no I/O. Tests print in the same shape through `tests/check.hpp`.

## Determinism contract

- Only `core/libm` provides transcendentals to simulation code. `<cmath>`'s `sin`, `exp`,
  `pow` and the rest are forbidden in `engine/core`, `engine/loop`, `plugins/*` (bar a plugin's `app/`) and
  `games/*/sim`; `tools/check-libm.sh` (`make libm-check`, CI's `agents` job) fails a call.
- FP flags: `-ffp-contract=off -fno-fast-math` (GCC: `-fexcess-precision=standard`;
  MSVC: `/fp:strict`), set once on `ge_fp` in the root `CMakeLists.txt`
  (linked by `ge_core` and published with it as `ge::fp`, so a game built against a
  release gets them too).
- Randomness only through `core::Rng` streams owned by the state. No clock in `sim/`.
- Iteration order is explicit. No `unordered_map` iteration feeds a result. Sorting
  uses `std::stable_sort`: equal keys keep their order, and that order is part of the
  result.
- Every lab's digest (`docs/labs.md` § Digests) is checked twice in one process and against
  `tests/labs_digests.txt` on every CI row (`labs_smoke`), so a lab is a determinism test too.
- Determinism is proven by the golden tables in `tests/golden/`, not by eye. They are the
  engine's own: a deliberate change of behaviour rewrites them in the PR that makes it.

## Decisions

| Part | Decision |
| --- | --- |
| Language | C++20, no exceptions or RTTI in engine code paths (they may stay enabled for tools) |
| Game-object model | POD state structs and explicit arrays, systems as free functions in a written order; no generic ECS, because the simulation is already a fixed set of systems over flat arrays |
| Determinism | The engine's own `core/libm` (vendored fdlibm and glibc's pow) and `core/num`, so every platform gives the same bits; FP flags in CMake; golden tests |
| Loop | A fixed 120 Hz accumulator (`engine/loop/run_clock.hpp`), interpolated rendering |
| Platform | SDL3, statically linked: one dependency for the window, input, gamepad, audio device and files on every desktop and phone |
| GPU | Dawn, native on every target (Metal, Vulkan, D3D12), through its `webgpu.h` header; no web build (`docs/rendering.md` § The backend) |
| Shaders | WGSL, embedded at build |
| Audio | Our own DSP graph on SDL3 audio: every sound is synthesized, so no middleware |
| UI | Our own immediate-mode 2D layer + SDF text; Dear ImGui for dev tools |
| Assets | cgltf for the modelled skis; everything else generated in code or baked to embedded blobs |
| Tests | Our own header harness, CTest, golden tables |
| Build/CI | CMake + Ninja + presets; GitHub Actions on Linux x64/arm64 (GCC, Clang), macOS, Windows |

## Build

CMake ≥ 3.25 + Ninja, `CMakePresets.json` (`debug` with ASan/UBSan, `release`).
Dependencies are pinned to a tag and a SHA-256, and each one is added in the milestone that
needs it: SDL3 through `FetchContent` from its release archive (`cmake/SDL.cmake`), Dawn as its
prebuilt GitHub release (`cmake/Dawn.cmake`; Tint comes in the same archive), later cgltf,
Dear ImGui and Tracy. Downloads land in `.cache/deps/` (gitignored, shared by every preset,
cached by CI). `GE_PLATFORM` and `GE_GPU` switch the platform layer and the GPU off; the
simulation, the plugins and their tests never need either, and `GE_GPU` defaults off where
Dawn publishes no archive (Linux arm64). `Makefile` names the everyday commands. CI (`.github/workflows/ci.yml`) runs format,
agents-check, lint and the test matrix.
