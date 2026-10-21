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
5. **G5: Desktop and web.** The engine runs as a desktop app (Linux, macOS and
   Windows) and in the browser (WebAssembly + WebGPU), from one codebase.
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

### Non-goals

- A general-purpose editor, a scripting language, or plugins loaded at run time (shared
  libraries, scripts). Plugins are compile-time modules linked by the game (G8).
- Any physics, renderer or audio middleware the game does not need. The skier's physics
  is the game's own.
- Music. Fall Line has none by design: its sound is its effects.
- Changes to the reference sources used while porting. They are read-only from here.

## Milestones

Each row is one or more PRs. **Status** is one of `done`, `in progress` or `open`. A
row's exit criterion is what makes it `done`.

| # | Milestone | Exit criterion | Status |
| --- | --- | --- | --- |
| M0 | **Groundwork**: AGENTS.md and its symlinks, skills, the research and architecture docs, the CMake build, the test harness, CI on five OS/compiler rows, and `core/prng` and `core/math` (hypot), bit-exact | CI green; the golden tests pass on every row | done |
| M0.1 | **Publishing and secrets**: how the engine is published from GitHub, with credentials only in GitHub secrets | Release workflow documented in AGENTS.md; no credential in the tree | done (`docs/publishing.md`) |
| M0.2 | **Discoverability and skill reflection**: the `ge:` tag block on every source file, the generated `docs/index.md`, `make find`, and lesson fragments read and checked by `tools/skill-lessons.mjs` | `make agents-check` holds the tags, the index and the lessons on CI | done |
| M0.3 | **Plugin system** (G8): `plugins/<name>/` with one `ge_add_plugin` call each, a skill per plugin, the engine / plugin / game test, and `tools/check-plugins.mjs`; the racing primitives and the mountain generator moved out of `engine/` and `games/fall_line/` into `plugins/racing` and `plugins/mountain` | `make agents-check` holds the contract on CI; every golden passes unchanged after the move | done |
| M0.4 | **A clean slate**: no legacy references outside the `porting/` working area, the retired reference tag, the porting map, the legacy-word scan in `make agents-check`, `core/num`, and bit parity with any reference dropped as a requirement | `make agents-check` fails a reference name outside `porting/`; every golden passes | done |
| M1 | **The deterministic libm**: `core/libm` (sin, cos, tan, atan, atan2, asin, acos, exp, expm1, log, log2, pow, cbrt, tanh) with the same bits on every platform, plus `core/num` (the number semantics helpers) | The `libm_*` and `core_num` golden tables match bit for bit on every CI row | done (vendored fdlibm + glibc pow) |
| M2 | **The rest of the core pool**: noise, heightfield, quat, solar, output, and the run clock (`loop/run_clock`) | Every function the simulation calls has a golden test | done |
| M3 | **Racing primitives** (`plugins/racing`): tape (byte-identical encoding), records, standings | A golden ghost tape decodes and re-encodes byte for byte | done (the stored-JSON readers move to M10 with the ghost and record stores) |
| M4a | **The single piste** (generator v1, the single piste): `plugins/mountain/mapgen` (the mountain, the piste, its kickers, cliffs, drifts and crust, the forest, the day and the weather, the gates and the start line, the search over attempts, the compile and `level_digest`) and the single piste's `analyze_level` (`plugins/mountain/analysis`) | `level_digest`, every grid and every section of the level match the golden on the level seeds, the benchmark's map, a map in each crusted region and two seeds whose first attempts are refused (`tests/mountain_generate_test.cpp`) | done (0.53–0.77 s a map; the terrain park, R20, waits for M4b) |
| M4b | **The ski area**: the massif, the lifts, the network of graded runs and the lanes between them (R23–R30), the courses, the village, the terrain park (R20) and the area's analysis, landed stage by stage in the rows below | Every stage held by `tests/mountain_resort_build_test.cpp`: an attempt built twice is the same to the bit, its digest is the golden's row (rewritten on purpose with `GE_GOLDEN_WRITE=1`), and the rule book's invariants hold on the area built; a lab draws the area | in progress |
| M4b1 | **The massif and the lifts**: the massif (R25), the plan (the stations, the lifts, the run slots, R26–R27), the station pads, and the way off the gondola's top to the peak's chair | Through the `pads` stage: deterministic, the digest holds | done |
| M4b2 | **The runs walked and linked**: the walk and the network index, the run starts and the ramps' room off the tops, the lane search, R29's access lanes and drag lifts, and the branch lanes between sectors | Through the `links` stage: deterministic, the digest holds, every run a line on the map, at least `runs_min` pistes (R27), every piste kept skied again (R29) | done |
| M4b3 | **The area finished**: every run graded and pressed (drops, kickers, drifts on each), the pads levelled again, the ramps off the tops, R29 again on the measured colours, the stations cleared off the runs, the off-piste kickers and cliffs, the crust, the hub, the woods and the courses | Through the `courses` stage: deterministic, the digest holds, the rule book's invariants on the finished area; a lab picture of the area | open |
| M4b4 | **The race maps**: a course stood up as a map (its gates, its day), choosing the course, and the area's analysis that accepts an attempt | Every race discipline gets a map off a seed sweep that the analysis accepts; deterministic; the digests hold | open |
| M4b5 | **The terrain park on a ski area** (R20): the park laid down a course and its analysis | A park on every seed of a sweep, accepted by its analysis; the digests hold | open |
| M4c | **The ski routes** (v9, v10, R42): the orange routes, the wild line, the felled trunks and the jumps on it | Every jump hill and a v10 seed sweep build clean, with golden digests | open |
| M4d | **The real faces** (R25): the baked heights and hints, the woods by height, and the generator's real-face branch | Every real face builds a clean map, with golden digests | open |
| M5 | **The run**: `games/fall_line/sim`: `create_game` and `step` (the skier, the snow, the course, the field) and the bot | The bot finishes the reference seeds, golden `sim` digests pin the runs, and the headless `sim` CLI exists | open |
| M6 | **The platform and the GPU**: an SDL3 window, input, the frame loop and webgpu.h (desktop: wgpu-native or Dawn) | A window clears to the sky colour at vsync on every desktop row; a headless GPU smoke test runs in CI | open |
| M7 | **The first picture**: the terrain clipmap, the sky and the haze, the sun's shadow, and the skier as a placeholder, all drawn from `GameState` | A seed's screenshots of the named views, reviewed in the PR (the `render` skill) | open |
| M8 | **The world drawn**: the forest, the snow shader, the trail map and grooves, the spray, the figures, and the skis as glTF models | Screenshots of every named view, reviewed in the PR | open |
| M9 | **Sound**: the synth as a DSP graph on SDL3 audio, plus Fall Line's beds and the bank | Every sound's rendered buffer meets its meters (the `audio` skill) | open |
| M10 | **HUD and shell**: the HUD, the cards, settings, records and ghosts, and every word in `strings` | A full race is playable from the title to the finish plate | open |
| M11 | **Web build**: Emscripten + emdawnwebgpu, served from GitHub Pages | The same race in the browser, bit for bit | open |
| M12 | **Agent-facing engine**: the `ge` CLI (`ge new`, `ge skills sync`, `ge doctor`), consumer skills shipped under `consumer-skills/`, and the diagnostics convention (`docs/architecture.md` § Diagnostics) enforced by a test | `ge new` scaffolds a repo that builds against the released engine and passes its own `agents-check` | open |
| M13+ | **The free ride**: the machines, the crowd and every other Fall Line system, one row per system | One row each, added when it is started | open |

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
