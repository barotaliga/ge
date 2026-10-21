# Goals and roadmap

This is the long-term spec every task in this repository serves. Read it before you
start a task. When the task is done, update the milestone's **Status** column in the
same PR, then name the next open row as the next step.

## The goal

gameengine2 is a C++ game engine that runs **Fall Line** (game5) natively and fast. Fall
Line is ported here and stays in the tree as the engine's demo game. The engine is not a ski
engine: what is generic is in `engine/`, what a genre or a world shares (racing, a generated ski
mountain, a skier on snow) is in plugins any game of that kind links, and only what makes Fall
Line Fall Line is in `games/fall_line/` (G8). The original
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
   place when the port needs it. G8 decides WHERE a component lives, never WHETHER it is
   built.
5. **G5: Ships where game5 ships.** The engine runs as a desktop app (Linux, macOS and
   Windows) and in the browser (WebAssembly + WebGPU), from one codebase.
6. **G6: Consumed by agents.** Downstream game repos that use the engine are also
   built by agents, so the engine is a product for agents too. Every error, assert
   and log line says what went wrong, where, and the next action to take, so it reads
   as a prompt rather than a code. Every task a consumer has (start a game, add a
   system, add a draw pass, add a sound, pin a seed, cut a release) has a skill.
   A `ge` CLI scaffolds a consumer repo: the layout, AGENTS.md, and the engine's
   consumer skills copied into `.agents/skills/`.
7. **G7: Always green.** CI builds and tests every push on every compiler and OS row.
   PRs merge when they are green. A parity failure is fixed, never waived.
8. **G8: Genre-agnostic engine, reusable plugins.** Code is placed in one of three homes:
   `engine/` (any game of any genre would use it), `plugins/<name>/` (any game of one genre
   or world would use it unchanged) or `games/<game>/` (only this game). Dependencies run
   game → plugin → engine, plugins depend only on plugins they declare, and nothing is
   registered at run time. The contract is `docs/architecture.md` § Plugins, the procedure
   the `plugins` skill, and `make agents-check` holds it.

### Non-goals

- A general-purpose editor, a scripting language, or plugins loaded at run time (shared
  libraries, scripts). Plugins are compile-time modules linked by the game (G8).
- Any physics, renderer or audio middleware that game5 does not need. game5's skier
  physics is its own and is ported as it is.
- Music. game5 has none by design.
- Changes to game5 or oss-game-framework. They are read-only from here.

## Milestones

Each row is one or more PRs. **Status** is one of `done`, `in progress` or `open`. A
row's exit criterion is what makes it `done`.

| # | Milestone | Exit criterion | Status |
| --- | --- | --- | --- |
| M0 | **Groundwork**: AGENTS.md and its symlinks, skills, the research and architecture docs, the CMake build, the test harness, CI on five OS/compiler rows, and `core/prng` and `core/math` (hypot) at bit parity | CI green; the parity tests pass on every row | done |
| M0.1 | **Publishing and secrets**: how the engine is published from GitHub, with credentials only in GitHub secrets | Release workflow documented in AGENTS.md; no credential in the tree | done (`docs/publishing.md`) |
| M0.2 | **Discoverability and skill reflection**: the `ge:` tag block on every source file, the generated `docs/index.md`, `make find`, and lesson fragments read and checked by `tools/skill-lessons.mjs` | `make agents-check` holds the tags, the index and the lessons on CI | done |
| M0.3 | **Plugin system** (G8): `plugins/<name>/` with one `ge_add_plugin` call each, a skill per plugin, the engine / plugin / game test, and `tools/check-plugins.mjs`; the racing primitives and the mountain generator moved out of `engine/` and `games/fall_line/` into `plugins/racing` and `plugins/mountain` | `make agents-check` holds the contract on CI; every golden passes unchanged after the move | done |
| M1 | **The deterministic libm**: `core/libm` (sin, cos, tan, atan, atan2, asin, acos, exp, expm1, log, log2, pow, cbrt, tanh) that matches the oracle's runtime, plus `core/js` (the JS semantics helpers listed in `docs/porting.md`) | Golden tables from game5's pinned Node match bit for bit on every CI row | done (V8 fdlibm + glibc pow; `docs/research.md` §2) |
| M2 | **The rest of the core pool**: noise, heightfield, quat, solar, output, and the run clock (`loop/run-clock`) | Every function game5 calls has a golden test from the framework | done (polyline, `tiledValueNoise` and `TWILIGHT` skipped: game5 calls none of them) |
| M3 | **Racing primitives** (`plugins/racing`): tape (byte-identical encoding), records, standings | A game5 ghost tape decodes and re-encodes byte for byte | done (`readBook` and `isControlTape`, the stored-JSON readers, move to M10 with the ghost and record stores) |
| M4a | **The single piste** (generator v1, `singlePiste`): `plugins/mountain/mapgen` (the mountain, the piste, its kickers, cliffs, drifts and crust, the forest, the day and the weather, the gates and the start line, the search over attempts, the compile and `levelDigest`) and the single piste's `analyzeLevel` (`plugins/mountain/analysis`) | `levelDigest`, every grid and every section of the level match game5 on `LEVEL_SEEDS`, the benchmark's map, a map in each crusted region and two seeds whose first attempts are refused (`tests/mountain_generate_test.cpp`) | done (about 1.5× faster than the oracle: 0.53–0.77 s a map against 0.76–1.0 s under Node 24; the terrain park, R20, waits for M4b) |
| M4b | **The ski area** (v8): the massif, the lifts, the network of graded runs (R23–R28), the courses, the village, the terrain park (R20) and `analyzeResort` | `levelDigest` matches game5 on every pinned race map and trick map (`race-maps.ts`, `trick-maps.ts`, all v8) | open |
| M4c | **The ski routes** (v9, v10, R42): the orange routes, the wild line, the felled trunks and the jumps on it | `levelDigest` matches game5 on every jump hill (`jump-hills.ts`, v9) and on a v10 seed sweep | open |
| M4d | **The real faces** (R25): the baked heights and hints, the woods by height, and the generator's real-face branch | `levelDigest` matches game5 on every real face's map | open |
| M5 | **The run**: `games/fall_line/sim`, which ports `createGame` and `step` (the skier, the snow, the course, the field) and the bot | The `make sim` digests match game5's for the reference seeds; the headless `sim` CLI exists | open |
| M6 | **The platform and the GPU**: an SDL3 window, input, the frame loop and webgpu.h (desktop: wgpu-native or Dawn) | A window clears to the sky colour at vsync on every desktop row; a headless GPU smoke test runs in CI | open |
| M7 | **The first picture**: the terrain clipmap, the sky and the haze, the sun's shadow, and the skier as a placeholder, all drawn from `GameState` | The `make world`-style screenshots of a seed are recognisably game5's | open |
| M8 | **The world drawn**: the forest, the snow shader, the trail map and grooves, the spray, the figures, and the skis as glTF models | Side-by-side screenshots against game5 for each `make world` view | open |
| M9 | **Sound**: a port of the framework's synth as a DSP graph on SDL3 audio, plus game5's beds and the bank | `audition` parity: rendered buffers compared with an offline WebAudio render | open |
| M10 | **HUD and shell**: the HUD, the cards, settings, records and ghosts, and every word in `strings` | A full race is playable from the title to the finish plate | open |
| M11 | **Web build**: Emscripten + emdawnwebgpu, served from GitHub Pages | The same race in the browser at parity | open |
| M12 | **Agent-facing engine**: the `ge` CLI (`ge new`, `ge skills sync`, `ge doctor`), consumer skills shipped under `consumer-skills/`, and the diagnostics convention (`docs/architecture.md` § Diagnostics) enforced by a test | `ge new` scaffolds a repo that builds against the released engine and passes its own `agents-check` | open |
| M13+ | **The free ride**: the machines, the crowd and every other game5 system, one row per system as `docs/features.md` lists them in game5 | One row each, added when it is started | open |

## Where each milestone's code goes (G8)

Classify each file with the `plugins` skill when its row starts; this is the expected split.

| Rows | engine/ | plugins/ | games/fall_line/ |
| --- | --- | --- | --- |
| M4b–M4d | | `mountain` (the ski area, the routes, the real faces) | which seeds and maps the game races (the pinned lists) |
| M5 | | `skier` (the skier on snow: the legs, the snow, the edge, the poles, flight, the crash and the body); `racing` (the field's standings) | `create_game`, the order of systems in `step`, the modes and their rules, the course as Fall Line runs it, the bot, the `sim` CLI |
| M6, M11 | `platform`, `gpu`, the web build | | |
| M7–M8 | the generic passes (terrain clipmap, instancing with LOD bands, skinning, particles, sky and haze, shadows, post) | `mountain/app` (the snow shader, the trail map and grooves, the forest's look), `skier/app` (the figure, the spray) | Fall Line's own look and tuning of those passes |
| M9 | the synth's DSP graph, racks, `play_def` | the snow and edge beds | the bank, the listener table |
| M10 | the 2D layer, text, the dev UI | `racing` (the ghost and record stores) | the HUD, the cards, settings, every word in `strings` |
| M12 | the `ge` CLI, consumer skills | each plugin's skill, copied when a consumer links it | |

### Open for extension

Other games of the same shape (a rally game, a jet-ski racer, a snowmobile racer) may run on the
engine later. None is built for now (G4), but the boundaries above are drawn so they would fit:
the engine never learns a ski word, `racing` serves any racer, `mountain` never depends on the
rider (a snowmobile game generates the same mountain), the snow's surface model is expected to
be its own plugin under `skier` rather than a part of it, and a new world (roads, water) is a new
plugin beside `mountain`, which is why world generation is not in the engine.

## How a task uses this page

1. Find the row the task belongs to. If no row fits, the task is probably out of scope
   (G4), so ask before you build it. Decide each file's home (engine, plugin or game) with
   the `plugins` skill before the first edit.
2. Do the work under that row's skill (`AGENTS.md` routes it).
3. In the same PR, update the row's Status. If the work showed that a row needs
   splitting, split it.
4. Finish by naming the next open row (or the next unfinished part of this row) as the
   next step.
