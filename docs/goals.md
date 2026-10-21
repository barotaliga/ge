# Goals and roadmap

This is the long-term spec every task in this repository serves. Read it before you
start a task. When the task is done, update the milestone's **Status** column in the
same PR, then name the next open row as the next step.

## The goal

gameengine2 is a C++ game engine that is fast, deterministic to the bit, and built for
agents. Its first game, **Fall Line** (a downhill skiing game), lives in the tree as the
engine's demo game. The engine is not a ski
engine: what is generic is in `engine/`, what a genre or a world shares (racing, a generated ski
mountain, a skier on snow) is in plugins any game of that kind links, and only what makes Fall
Line Fall Line is in `games/fall_line/` (G8).

### High-level goals

1. **G1: Deterministic on its own terms.** The simulation produces the same bits for the
   same seed and inputs on every compiler and OS: the same `level_digest`, the same `sim`
   digests, the same ghost replays, held by the golden tables in `tests/golden/`. Being
   close across platforms is not good enough, because the pinned maps, the ghosts and the
   record book all depend on exact bits. Matching any other implementation's bits is not a
   goal: the goldens are the engine's own, and a deliberate change rewrites them.
2. **G2: Fast.** Every component does one job and does it quickly. A level generates
   in well under a second. A simulation step costs a fraction of a 120 Hz budget. A
   frame hits the display's refresh rate on the reference machines with the picture
   at its top. Every speed-up claim comes with a before-and-after measurement
   (`perf` skill).
3. **G3: Built for agents.** Only agents write code here. The code and the docs are laid
   out so an agent finds the one place an answer lives and can change it safely: one
   subject per file, flat data, explicit names, every rule stated once, and a skill for
   every subsystem (`AGENTS.md`).
4. **G4: Only what the game needs.** Each component traces back to something Fall Line
   does. Nothing is built for a hypothetical second game. A general engine feature earns
   its place when the game needs it. G8 decides WHERE a component lives, never WHETHER it is
   built.
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
   PRs merge when they are green. A golden failure is fixed, never waived.
8. **G8: Genre-agnostic engine, reusable plugins.** Code is placed in one of three homes:
   `engine/` (any game of any genre would use it), `plugins/<name>/` (any game of one genre
   or world would use it unchanged) or `games/<game>/` (only this game). Dependencies run
   game → plugin → engine, plugins depend only on plugins they declare, and nothing is
   registered at run time. The contract is `docs/architecture.md` § Plugins, the procedure
   the `plugins` skill, and `make agents-check` holds it.

9. **G9: Measured and looked at.** Every subsystem, plugin and game has labs: sim and
   scenario runs, captioned screenshots, bot and agent drives, cost benches, all on one
   engine-level contract (`docs/labs.md`) that runs headless on a GPU-less Linux machine. An
   agent tunes by running a lab before and after, not by guessing, and every lab doubles as a
   determinism test through its digest.

### Non-goals

- A general-purpose editor, a scripting language, or plugins loaded at run time (shared
  libraries, scripts). Plugins are compile-time modules linked by the game (G8).
- Any physics, renderer or audio middleware the game does not need. The skier's physics
  is the game's own.
- Music. Fall Line has none by design: its sound is its effects.
- Changes to the reference sources used while porting. They are read-only from here.
- A web build. The engine is native only.

## Milestones

Each row is one or more PRs. **Status** is one of `done`, `in progress` or `open`. A
row is `done` when its exit criterion holds AND the labs in its **Labs it lands** cell run in
`make labs-smoke` (`docs/labs.md`). A subsystem an agent cannot measure and look at is not
finished.

| # | Milestone | Exit criterion | Labs it lands | Status |
| --- | --- | --- | --- | --- |
| M0 | **Groundwork**: AGENTS.md and its symlinks, skills, the research and architecture docs, the CMake build, the test harness, CI on five OS/compiler rows, and `core/prng` and `core/math` (hypot), bit-exact | CI green; the golden tests pass on every row | golden tests (labs did not exist yet) | done |
| M0.1 | **Publishing and secrets**: how the engine is published from GitHub, with credentials only in GitHub secrets | Release workflow documented in AGENTS.md; no credential in the tree | none (no runtime code) | done (`docs/publishing.md`) |
| M0.2 | **Discoverability and skill reflection**: the `ge:` tag block on every source file, the generated `docs/index.md`, `make find`, and lesson fragments read and checked by `tools/skill-lessons.mjs` | `make agents-check` holds the tags, the index and the lessons on CI | none (agent tooling) | done |
| M0.3 | **Plugin system** (G8): `plugins/<name>/` with one `ge_add_plugin` call each, a skill per plugin, the engine / plugin / game test, and `tools/check-plugins.mjs`; the racing primitives and the mountain generator moved out of `engine/` and `games/fall_line/` into `plugins/racing` and `plugins/mountain` | `make agents-check` holds the contract on CI; every golden passes unchanged after the move | the `labs/` directory of each plugin (M0.5 gives it the runner) | done |
| M0.4 | **A clean slate**: no legacy references outside the `porting/` working area, the retired reference tag, the porting map, the legacy-word scan in `make agents-check`, `core/num`, and bit parity with any reference dropped as a requirement | `make agents-check` fails a reference name outside `porting/`; every golden passes | none (agent tooling) | done |
| M0.5 | **Labs** (G9): the lab contract every subsystem, plugin and game plugs into (`engine/lab`: `LabSpec`, `GE_LAB`, the runner, the CPU raster, the PNG writer, the captioned report and its contact sheet), the `labs` executable, the smoke pass and its digest baseline on every CI row, the `labs` skill and `docs/labs.md` | `make labs-smoke` passes on every CI row; every later row names the labs it lands | `noise`, `run-clock`, `level`, `analyze`, `mapgen-cost` | done |
| M1 | **The deterministic libm**: `core/libm` (sin, cos, tan, atan, atan2, asin, acos, exp, expm1, log, log2, pow, cbrt, tanh) with the same bits on every platform, plus `core/num` (the number semantics helpers) | The `libm_*` and `core_num` golden tables match bit for bit on every CI row | golden tests | done (vendored fdlibm + glibc pow) |
| M2 | **The rest of the core pool**: noise, heightfield, quat, solar, output, and the run clock (`loop/run_clock`) | Every function the simulation calls has a golden test | `noise`, `run-clock` | done |
| M3 | **Racing primitives** (`plugins/racing`): tape (byte-identical encoding), records, standings | A golden ghost tape decodes and re-encodes byte for byte | golden tests; the tape records every drive lab (M5) | done (the stored-JSON readers move to M10 with the ghost and record stores) |
| M4a | **The single piste** (generator v1, the single piste): `plugins/mountain/mapgen` (the mountain, the piste, its kickers, cliffs, drifts and crust, the forest, the day and the weather, the gates and the start line, the search over attempts, the compile and `level_digest`) and the single piste's `analyze_level` (`plugins/mountain/analysis`) | `level_digest`, every grid and every section of the level match the golden on the level seeds, the benchmark's map, a map in each crusted region and two seeds whose first attempts are refused (`tests/mountain_generate_test.cpp`) | `level`, `analyze`, `mapgen-cost` | done (0.53–0.77 s a map; the terrain park, R20, waits for M4b) |
| M4b | **The ski area**: the massif, the lifts, the network of graded runs and the lanes between them (R23–R30), the courses, the village, the terrain park (R20) and the area's analysis, landed stage by stage in the rows below | Every stage held by `tests/mountain_resort_build_test.cpp`: an attempt built twice is the same to the bit, its digest is the golden's row (rewritten on purpose with `GE_GOLDEN_WRITE=1`), and the rule book's invariants hold on the area built; a lab draws the area | `level` and `analyze` on the ski area; `resort`, `junctions` | in progress |
| M4b1 | **The massif and the lifts**: the massif (R25), the plan (the stations, the lifts, the run slots, R26–R27), the station pads, and the way off the gondola's top to the peak's chair | Through the `pads` stage: deterministic, the digest holds | none (the area's picture, `resort`, lands with M4b3d) | done |
| M4b2 | **The runs walked and linked**: the walk and the network index, the run starts and the ramps' room off the tops, the lane search, R29's access lanes and drag lifts, and the branch lanes between sectors | Through the `links` stage: deterministic, the digest holds, every run a line on the map, at least `runs_min` pistes (R27), every piste kept skied again (R29) | none (the area's picture, `resort`, lands with M4b3d) | done |
| M4b3 | **The area finished**, in the four stages below | Through the `courses` stage: deterministic, the digest holds, the rule book's invariants on the finished area; a lab picture of the area | `resort` (a picture of the area) | done |
| M4b3a | **Graded and pressed**: every run graded onto the mountain in walk order (a merging run onto the other's surface), its drops, kickers and drifts laid clear of the junctions, pressed in with the cores of the runs before it kept; the pads levelled again; each run's colour measured | Through the `graded` stage: deterministic, the digest holds, every run falls, is packed along its line but in its drifts, and has no kicker over a junction | none (the area's picture, `resort`, lands with M4b3d) | done |
| M4b3b | **Access on the pressed mountain**: the ramps off the tops, R29 again on the measured colours (drag lifts settled), the stations stood clear of the runs, the drag tops' ramps | Through the `access` stage: deterministic, the digest holds, every piste kept skied again on its measured colour, no station on a run | none (the area's pictures, `resort` and `junctions`, land with M4b3d) | done |
| M4b3c | **The mountain's own features**: the off-piste kickers and cliffs clear of the runs, the crust, the hub and its wind tunnels | Through the `features` stage: deterministic, the digest holds, no feature on a run | none (the area's pictures, `resort` and `junctions`, land with M4b3d, showing the features clear of the runs) | done |
| M4b3d | **The woods and the courses**: the woods clear of the runs, lifts and hub; the day's bearings; every course down the network to the village | Through the `courses` stage: deterministic, the digest holds, no tree on a run, at least one course; a lab picture of the area | `resort` (a picture of the area, the courses drawn), `junctions` (where runs meet) | done |
| M4b4 | **The race maps**, in the two rows below | Every race discipline gets a map off a seed sweep that the analysis accepts; deterministic; the digests hold | `level` and `analyze` on the race maps; the disciplines' own | in progress |
| M4b4a | **A ski area raced as a map** (generator v8): a course stood up as a `Level` (its slalom gates, its own day, the area published as `Level::resort`), choosing the course (by id, by colour, or the seed's dealt colour), the area's analysis that accepts an attempt and the course's that offers a course, and the bot weaving through the gates | Deterministic, with golden digests (`tests/mountain_resort_level_test.cpp`); a 30-seed sweep builds clean; the bot finishes the reference seeds at v8 | `level` and `analyze` at `--version=1,8`; `sim --version=8` | done (30 of 30 seeds built, 2.3–5 s a map; 36 of 36 racers home on 9 seeds, none missed a gate) |
| M4b4b | **The race disciplines** set over a map (R31–R36): slalom, giant slalom, super-G, downhill, speed skiing, ski cross, each its course rule and its gates | Every discipline gets a map off a seed sweep that its analysis accepts; deterministic; the digests hold | a lab per discipline's course | open |
| M4b5 | **The terrain park on a ski area** (R20): the park laid down a course and its analysis | A park on every seed of a sweep, accepted by its analysis; the digests hold | `resort` showing the park | open |
| M4c | **The ski routes** (v9, v10, R42): the orange routes, the wild line, the felled trunks and the jumps on it | Every jump hill and a v10 seed sweep build clean, with golden digests | `routes`, `skijump-hills` | open |
| M4d | **The real faces** (R25): the baked heights and hints, the woods by height, and the generator's real-face branch | Every real face builds a clean map, with golden digests | `real-faces` | open |
| M5 | **The run**: `games/fall_line/sim`: `create_game` and `step` (the skier, the snow, the course, the field) and the bot | The bot finishes the reference seeds, golden `sim` digests pin the runs, and the headless `sim` CLI exists | `sim` (the bot over seeds, digests), `ride` (scenarios), `play` (an agent over stdio), rivals on one grid | done (M5a to M5c) |
| M5a | **The skier on the snow** (`plugins/skier`): the snow (sink, friction, plough, grip, brake), the limits a pilot plans on, `step_skier` (the contact impulse, the carve, the pivot, the tuck and the push, the jump, flight and landing, the trunks, the wipeout), the hand-built maps and the ride scenarios | Every scenario rides the same twice and holds its golden digest (`tests/skier_scenarios_test.cpp`); the schusses meet `terminal_speed` within 1%; the carve holds no more than its grip; the kicker flies; a trunk and a harsh drop wipe him out | `ride` (scenarios, measured and drawn) | done |
| M5b | **The run and the bot** (`games/fall_line/sim`): `create_game` and `step` on a generated map (the player's skier, the course: the start gate, the gates owed, a miss, the finish, the reset), the bot, `simulate` and the `sim` CLI (`make sim`, the `sim` lab) | The bot finishes the reference seeds; the `sim` digests are golden | `sim` (the bot over seeds: the run report table, its line drawn on the map) | done (9 of 9 reference seeds finished, none missed a gate or wiped out) |
| M5c | **The field and an agent at the controls**: rivals on one grid (bots with profiles, skier against skier, the standings off `plugins/racing`), and `play`: an agent over stdio | A field of four finishes the reference seeds with golden digests; a scripted `play` session replays to its digest | `sim` with rivals; `play` | done (36 of 36 racers home on the 9 reference seeds, none missed a gate or wiped out; an agent sending the bot's moves rides the bot's run to the bit) |
| M6 | **The platform and the GPU** (`docs/rendering.md` § The backend, § Feature tiers): an SDL3 window, input and the frame loop; `engine/gpu` over Dawn (the device and its feature tiers, RAII handles, the pipeline manifest and its creation counter, timestamp queries, staging uploads, offscreen targets and readback); WGSL embedding and `make shaders-check` (Tint offline); Dawn on lavapipe on CI's Linux x64 row | A window clears to the sky colour at vsync on every desktop row; on CI, a headless capture of a test scene matches its golden image within tolerance, and `make shaders-check` runs | `frame-pace`, `gpu-capture` (the headless GPU capture behind `Lab::shot`, a measured picture, on the software adapter) | done (`engine/platform`, `engine/gpu`, the golden capture on lavapipe, the window under a virtual display, `make shaders-check`, the `gpu-capture` and `frame-pace` labs; the audio device landed with M9a; gamepad and touch land with M10 and M11) |
| M7 | **The frame core** (`engine/render`, generic): the pass table and named targets, `FrameView` extraction, GPU-driven instance families with two-phase depth-pyramid culling, the depth prepass and `equal`-tested opaque pass, the heightfield clipmap, the horizon map, cached shadow cascades and the figures' shadow atlas, the sky tables and aerial perspective, TAA-U, the post pass, and the skier as a placeholder, all drawn from `GameState`, landed in the rows below | The screenshot lab's views of a seed are reviewed in the PR (the `render` skill); GPU ms per pass is reported; no pipeline is created after load; the renderer's CPU half is under 1 ms a frame | `world` (named views, captured from the GPU), `profile` | in progress |
| M7a | **The frame, the sky and the ground**: `engine/render` (`FrameView` and the camera, reversed-z; the frame block, the named targets, the pass table and its check, the per-pass recorder and stats), the sky's tables by compute (transmittance, multiple scattering, sky view, the sky's light) and the aerial-perspective volume behind one haze function, the GPU-culled geometry clipmap with morphed seams and the grid mirrored past the map, the post pass (exposure from the sky, tone curve, dither, encoding); Fall Line's frame (`games/fall_line/app`: its pass table, its ground's albedo, its named views) in the window and the `world` lab | The `world` lab's five views of seed 38 reviewed in the PR, zero GPU errors and no pipeline after load on lavapipe; GPU ms per pass in the lab's metrics; the frame's CPU half measured | `world` | done |
| M7b | **The ground's own shadow**: the horizon map (two bearings blended, rebaked one at a time as the sun turns), read by the terrain's draw | The `world` views show ridge shadows that move with `--hour`; the maps rebake only when the sun crosses a bearing step (a counter in the lab); the GPU's bake agrees with the CPU reference | `world` (`--hour` sweeps), `shadows` (the maps themselves, the bake counter) | done |
| M7c | **Depth and time**: the depth prepass and the `equal`-tested opaque pass, the depth pyramid (Hi-Z), the motion target, TAA-U (Halton jitter, YCoCg clamp, the reactive mask), `reset_history` for stills | Every opaque pixel shaded once (a lab counter); stills repeat with history reset; the frame upscales from 50–100 % | `world` (TAA on and off), `taa` (the history's ghosting on a moving camera) | open |
| M7d | **Instances and the run**: GPU-driven instance families with two-phase culling and LOD buckets, the skier as a placeholder figure drawn from `GameState` (M5), the cached sun cascades for what stands still and the figures' shadow atlas (they need casters, so they land with them), bloom and the colour grade in the post pass, the `profile` lab, the CPU half under 1 ms | The `world` lab's views include a run's moments; cascades redraw only past their thresholds (a counter in a lab); `profile` reports GPU ms per pass and the CPU half; the M7 row's exit criterion holds | `world` (a run's moments), `profile` | open |
| M8 | **The world drawn**: the forest as an instance family, the snow material, the trail and deformation maps stamped in compute, the cloud volume and night sky, compute particles (spray, snow cloud, snowfall), skinned figures and the crowd, the skis as glTF models (cgltf), and the quality ladder with its automatic fit | Screenshots of every named view, reviewed in the PR; the frame inside `docs/rendering.md` § Measurement's budgets at the top and middle of the ladder | `forest`, `snow`, `trails`, `spray`, `skis`, `skier` | open |
| M9 | **Sound**: the synth as a DSP graph on SDL3 audio, plus Fall Line's beds and the bank, in the two rows below | Every sound's rendered buffer meets its meters (the `audio` skill) | `synth`, `audition` (rendered buffers, metered) | in progress |
| M9a | **The synth and the device** (`engine/audio`, `engine/platform/audio`): the vocabulary (tones, noise, layers), the DSP graph (polyBLEP oscillators, the noise pools, the state-variable filter and its sweep, the envelope, the saturation curve, the detuned pair and vibrato, pan, four buses, the echo, the soft-knee limiter), the producer-to-render ring, `SoundDef` and `play_def`, racks, the meter and WAV bytes, and the audio device pulled on SDL's audio thread | The reference renders hold their golden hashes bit for bit on every CI row; the device pulls the synth on SDL's dummy driver; a busy race's load renders in a few % of a core | `synth` (every voice, colour, filter, drive, echo, layer and the limiter, metered, pictured and written as WAV; the render's cost, measured) | done |
| M9b | **Fall Line's sound**: the skier's wind and snow beds (`plugins/skier/app`), Fall Line's bank and the event route (the gates, the finish, the landings, the wipeouts, the trunks, the start), the listener table and the faders (`mix_of`), and the app playing the run through the device | Every bank sound and bed preset meets its meters (the bank's levels in order, every bed under the limiter, the tuck louder than the cruise); the app sounds the run | `audition` (every sound in the bank and every bed preset, metered) | open |
| M10 | **HUD and shell**: the HUD, the cards, settings, records and ghosts, and every word in `strings` | A full race is playable from the title to the finish plate | `screenshots` (HUD and cards at 1280x720, 390x844, 844x390) | open |
| M11 | **Mobile and store builds**: iOS and iPadOS (Metal), Android (Vulkan) and the Mac App Store; touch input, Dawn built from source and cached in CI, the mobile tiers of `docs/rendering.md` § Mobile GPUs | The same race on an iPad, an iPhone and an Android phone inside the mobile budget; each store build signs and installs | every lab above that can run on a device build (a `needs-device` issue otherwise) | open |
| M12 | **Agent-facing engine**: the `ge` CLI (`ge new`, `ge skills sync`, `ge doctor`), consumer skills shipped under `consumer-skills/`, and the diagnostics convention (`docs/architecture.md` § Diagnostics) enforced by a test | `ge new` scaffolds a repo that builds against the released engine and passes its own `agents-check` | `ge new` scaffolds a game's `labs/` directory and the labs skill | done (`ge new`, `ge skills sync`, `ge doctor`, `ge lessons` in `engine/cli`; seven consumer skills; `tests/cli_diagnostics_test.cpp` holds every message in the tree; packaging's `scaffold` job builds, tests and doctors a fresh repo against the tree on every PR; the download and checksum path verified against v0.1.0; a scaffold builds from the first release that ships `ge`) |
| M13+ | **The free ride**: the machines, the crowd and every other Fall Line system, one row per system | One row each, added when it is started | one picture, sim or scenario lab per system, named in its row | open |

## Where each milestone's code goes (G8)

Classify each file with the `plugins` skill when its row starts; this is the expected split.

| Rows | engine/ | plugins/ | games/fall_line/ |
| --- | --- | --- | --- |
| M4b–M4d | | `mountain` (the ski area, the routes, the real faces) | which seeds and maps the game races (the pinned lists) |
| M5 | | `skier` (the skier on snow: the legs, the snow, the edge, the poles, flight, the crash and the body); `racing` (the field's standings) | `create_game`, the order of systems in `step`, the modes and their rules, the course as Fall Line runs it, the bot, the `sim` CLI |
| M6, M11 | `platform`, `gpu`, the mobile and store builds | | |
| M7–M8 | `render`: the pass table, GPU-driven instance families, the heightfield clipmap, shadows, sky and clouds, TAA-U, post, compute particles, skinning (`docs/rendering.md`) | `mountain/app` (the snow shader, the trail map and grooves, the forest's look), `skier/app` (the figure, the spray) | Fall Line's own look and tuning of those passes |
| M9 | the synth's DSP graph, racks, `play_def`, the meter; the audio device | `skier/app`: the wind and the skis on the snow, as beds | the bank, the event route, the listener table, the faders |
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
2. Do the work under that row's skill (`AGENTS.md` routes it). Run the labs of the subject
   you change before and after (the `labs` skill), and land the row's labs with it.
3. In the same PR, update the row's Status. If the work showed that a row needs
   splitting, split it.
4. Finish by naming the next open row (or the next unfinished part of this row) as the
   next step.
