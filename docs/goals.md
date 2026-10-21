# Goals and roadmap

This is the long-term spec every task in this repository serves. Read it before you
start a task. When the task is done, update the milestone's **Status** column in the
same PR, then name the next open row as the next step.

## The goal

gameengine2 is a C++ game engine that runs **Fall Line** (game5) natively and fast. Fall
Line is ported here and stays in the tree as the engine's demo game. The original
TypeScript game (`niclaslindstedt/game5`) and its shared kit
(`niclaslindstedt/oss-game-framework`) are the **oracle**: they are read-only, and a
port is correct when it reproduces their results.

### High-level goals

1. **G1: Bit parity with the oracle.** The ported simulation produces the same bits as
   game5 for the same seed and inputs: the same `generateLevel` digest, the same
   `make sim` digests, the same ghost replays. Being close is not good enough, because
   the pinned maps, the ghosts and the record book all depend on exact bits.
2. **G2: Fast.** Every component does one job and does it quickly. A level generates
   faster than in game5. A simulation step costs a fraction of a 120 Hz budget. A
   frame hits the display's refresh rate on the reference machines with the picture
   at its top. Every speed-up claim comes with a before-and-after measurement
   (`perf` skill).
3. **G3: Built for agents.** Only agents write code here. The code and the docs are laid
   out so an agent finds the one place an answer lives and can change it safely: one
   subject per file, flat data, explicit names, every rule stated once, and a skill for
   every subsystem (`AGENTS.md`).
4. **G4: Only what game5 needs.** Each component traces back to something game5 does.
   Nothing is built for a hypothetical second game. A general engine feature earns its
   place when the port needs it.
5. **G5: Native on desktop and mobile.** The engine runs as a native app on macOS, Linux
   and Windows, and in the stores on iOS, iPadOS, the Mac App Store and Android, from
   one codebase. There is no web build.
6. **G6: Consumed by agents.** Downstream game repos that use the engine are also
   built by agents, so the engine is a product for agents too. Every error, assert
   and log line says what went wrong, where, and the next action to take, so it reads
   as a prompt rather than a code. Every task a consumer has (start a game, add a
   system, add a draw pass, add a sound, pin a seed, cut a release) has a skill.
   A `ge` CLI scaffolds a consumer repo: the layout, AGENTS.md, and the engine's
   consumer skills copied into `.agents/skills/`.
7. **G7: Always green.** CI builds and tests every push on every compiler and OS row.
   PRs merge when they are green. A parity failure is fixed, never waived.

### Non-goals

- A general-purpose editor, a scripting language, or a plugin system.
- Any physics, renderer or audio middleware that game5 does not need. game5's skier
  physics is its own and is ported as it is.
- Music. game5 has none by design.
- A web build. The engine is native only.
- Changes to game5 or oss-game-framework. They are read-only from here.

## Milestones

Each row is one or more PRs. **Status** is one of `done`, `in progress` or `open`. A
row's exit criterion is what makes it `done`.

| # | Milestone | Exit criterion | Status |
| --- | --- | --- | --- |
| M0 | **Groundwork**: AGENTS.md and its symlinks, skills, the research and architecture docs, the CMake build, the test harness, CI on five OS/compiler rows, and `core/prng` and `core/math` (hypot) at bit parity | CI green; the parity tests pass on every row | done |
| M0.1 | **Publishing and secrets**: how the engine is published from GitHub, with credentials only in GitHub secrets | Release workflow documented in AGENTS.md; no credential in the tree | in progress (separate thread) |
| M0.2 | **Discoverability and skill reflection**: the `ge:` tag block on every source file, the generated `docs/index.md`, `make find`, and lesson fragments read and checked by `tools/skill-lessons.mjs` | `make agents-check` holds the tags, the index and the lessons on CI | done |
| M1 | **The deterministic libm**: `core/libm` (sin, cos, tan, atan, atan2, asin, acos, exp, expm1, log, log2, pow, cbrt, tanh) that matches the oracle's runtime, plus `core/js` (the JS semantics helpers listed in `docs/porting.md`) | Golden tables from game5's pinned Node match bit for bit on every CI row | done (V8 fdlibm + glibc pow; `docs/research.md` §2) |
| M2 | **The rest of the core pool**: noise, heightfield, quat, solar, output, and the run clock (`loop/run-clock`) | Every function game5 calls has a golden test from the framework | done (polyline, `tiledValueNoise` and `TWILIGHT` skipped: game5 calls none of them) |
| M3 | **Racing primitives**: tape (byte-identical encoding), records, standings | A game5 ghost tape decodes and re-encodes byte for byte | done (`readBook` and `isControlTape`, the stored-JSON readers, move to M10 with the ghost and record stores) |
| M4a | **The single piste** (generator v1, `singlePiste`): `games/fall_line/sim/mapgen` (the mountain, the piste, its kickers, cliffs, drifts and crust, the forest, the day and the weather, the gates and the start line, the search over attempts, the compile and `levelDigest`) and the single piste's `analyzeLevel` (`games/fall_line/sim/analysis`) | `levelDigest`, every grid and every section of the level match game5 on `LEVEL_SEEDS`, the benchmark's map, a map in each crusted region and two seeds whose first attempts are refused (`tests/mapgen_generate_test.cpp`) | done (about 1.5× faster than the oracle: 0.53–0.77 s a map against 0.76–1.0 s under Node 24; the terrain park, R20, waits for M4b) |
| M4b | **The ski area** (v8): the massif, the lifts, the network of graded runs (R23–R28), the courses, the village, the terrain park (R20) and `analyzeResort` | `levelDigest` matches game5 on every pinned race map and trick map (`race-maps.ts`, `trick-maps.ts`, all v8) | open |
| M4c | **The ski routes** (v9, v10, R42): the orange routes, the wild line, the felled trunks and the jumps on it | `levelDigest` matches game5 on every jump hill (`jump-hills.ts`, v9) and on a v10 seed sweep | open |
| M4d | **The real faces** (R25): the baked heights and hints, the woods by height, and the generator's real-face branch | `levelDigest` matches game5 on every real face's map | open |
| M5 | **The run**: `games/fall_line/sim`, which ports `createGame` and `step` (the skier, the snow, the course, the field) and the bot | The `make sim` digests match game5's for the reference seeds; the headless `sim` CLI exists | open |
| M6 | **The platform and the GPU** (`docs/rendering.md` § The backend, § Feature tiers): an SDL3 window, input and the frame loop; `engine/gpu` over Dawn (the device and its feature tiers, RAII handles, the pipeline manifest and its creation counter, timestamp queries, staging uploads, offscreen targets and readback); WGSL embedding and `make shaders-check` (Tint offline); Dawn on lavapipe on CI's Linux x64 row | A window clears to the sky colour at vsync on every desktop row; on CI, a headless capture of a test scene matches its golden image within tolerance, and `make shaders-check` runs | open |
| M7 | **The frame core** (`engine/render`, generic): the pass table and named targets, `FrameView` extraction, GPU-driven instance families with two-phase depth-pyramid culling, the depth prepass and `equal`-tested opaque pass, the heightfield clipmap, the horizon map, cached shadow cascades and the figures' shadow atlas, the sky tables and aerial perspective, TAA-U, the post pass, and the skier as a placeholder, all drawn from `GameState` | The screenshot lab's views of a seed are recognisably the demo game's; GPU ms per pass is reported; no pipeline is created after load; the renderer's CPU half is under 1 ms a frame | open |
| M8 | **The world drawn**: the forest as an instance family, the snow material, the trail and deformation maps stamped in compute, the cloud volume and night sky, compute particles (spray, snow cloud, snowfall), skinned figures and the crowd, the skis as glTF models (cgltf), and the quality ladder with its automatic fit | Side-by-side screenshots against the reference pictures for each lab view; the frame inside `docs/rendering.md` § Measurement's budgets at the top and middle of the ladder | open |
| M9 | **Sound**: a port of the framework's synth as a DSP graph on SDL3 audio, plus game5's beds and the bank | `audition` parity: rendered buffers compared with an offline WebAudio render | open |
| M10 | **HUD and shell**: the HUD, the cards, settings, records and ghosts, and every word in `strings` | A full race is playable from the title to the finish plate | open |
| M11 | **Mobile and store builds**: iOS and iPadOS (Metal), Android (Vulkan) and the Mac App Store; touch input, Dawn built from source and cached in CI, the mobile tiers of `docs/rendering.md` § Mobile GPUs | The same race on an iPad, an iPhone and an Android phone inside the mobile budget; each store build signs and installs | open |
| M12 | **Agent-facing engine**: the `ge` CLI (`ge new`, `ge skills sync`, `ge doctor`), consumer skills shipped under `consumer-skills/`, and the diagnostics convention (`docs/architecture.md` § Diagnostics) enforced by a test | `ge new` scaffolds a repo that builds against the released engine and passes its own `agents-check` | open |
| M13+ | **The free ride**: the machines, the crowd and every other game5 system, one row per system as `docs/features.md` lists them in game5 | One row each, added when it is started | open |

## How a task uses this page

1. Find the row the task belongs to. If no row fits, the task is probably out of scope
   (G4), so ask before you build it.
2. Do the work under that row's skill (`AGENTS.md` routes it).
3. In the same PR, update the row's Status. If the work showed that a row needs
   splitting, split it.
4. Finish by naming the next open row (or the next unfinished part of this row) as the
   next step.
