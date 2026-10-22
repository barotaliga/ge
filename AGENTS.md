# Agent guidance for gameengine2

This file is the single source of truth for AI coding agents in this repo. `CLAUDE.md`,
`GEMINI.md`, `.cursorrules` and `.github/copilot-instructions.md` are symlinks to it,
and `.claude/skills` and `.gemini/skills` are symlinks to `.agents/skills`.
`tools/check-agents.sh` (CI's `agents` job) checks these links.

**This file is the ROUTER, not the manual.** It says what the project is, the rules
every task follows, where things live, and which skill owns each subject. The
procedures live in `.agents/skills/<name>/SKILL.md`. Load the skill that owns your
task's subject before your first edit.

**Only agents write code here.** Optimise everything (the code, its layout, the docs and
the output it prints) for an agent that arrives with no context, has to find the one
place an answer lives, and has to change it safely.

## What this is

A C++20 game engine: fast, deterministic to the bit, and built for agents. Its first game
is **Fall Line**, a downhill skiing game, which stays in the tree as the demo game
(`games/fall_line/`). **It is not a ski engine**: generic code is in
`engine/`, what a genre or a world shares (racing, the generated ski mountain, the skier on snow) is in
`plugins/<name>/` for any game of that kind to link, and only Fall Line's own is in
`games/fall_line/` ([`docs/architecture.md`](docs/architecture.md) § Plugins). **The goal spec and the roadmap are
[`docs/goals.md`](docs/goals.md).** Every task serves one of its milestone rows and
updates that row's Status in its PR.

| Doc | What it holds |
| --- | --- |
| [`docs/goals.md`](docs/goals.md) | The long-term goals (G1–G9), the non-goals and the milestone roadmap |
| [`docs/architecture.md`](docs/architecture.md) | The layers and their dependency direction, the frame, the code conventions, diagnostics, the determinism contract and the build |
| [`docs/labs.md`](docs/labs.md) | The labs: the contract every subsystem, plugin and game plugs into, the kinds (sim, scenario, picture, drive, bench), captions, digests, and the labs each milestone lands |
| [`docs/debugging.md`](docs/debugging.md) | Debugging a run: the log channels and the log spec, log dumps, series traces and spans files, the outlier rule, the charts and `ge graph` |
| `docs/index.md` | GENERATED from the code's `ge:` tags by `make index` (and `make agents-check`) in every checkout, never committed: every file's one-line summary, owning skill and test; keyword to files |
| [`docs/rendering.md`](docs/rendering.md) | The GPU backend (Dawn), the frame's pass table, every rendering technique and why, budgets, lab hooks, and what was considered and not taken |
| [`docs/perf.md`](docs/perf.md) | The performance ledger: every speed-up, before and after, and how it was measured |
| [`docs/publishing.md`](docs/publishing.md) | What a release is (GitHub Releases + GitHub Packages), how a game consumes it, changeset fragments, cutting a release, the credentials |

**The working area.** `porting/` holds everything that only matters while modules are
being carried over from their reference sources: the porting guide (`porting/README.md`),
the research behind it, the porting map (`porting/map.md`: which reference module each
engine file reproduces), the golden-table generators (`porting/parity/`) and their make
targets (`porting/porting.mk`). It is a WORKING area, not documentation: it is deleted when
porting ends, and nothing outside it may depend on it except `make golden`.

## Rules for every task

- **A CLEAN SLATE: NO LEGACY REFERENCES.** The engine and its games stand on their own; they
  are not a continuation of the code they were carried over from. Outside `porting/`,
  nothing (code, comments, `ge:` tags, docs, skills, lessons, this file, the Makefile,
  CMake, CI, commit messages and PR titles) names the reference repositories or kit, the
  reference language or its files and identifiers, or treats the reference as the
  authority, and no file is described by where it came from; nothing justifies a choice by
  what a reference runtime does. Say what the code does and why in
  the engine's own terms: "rounds halves toward +inf", "held bit-exact by
  `tests/golden/core_num.txt`". The link from an engine file to its reference is a row in
  `porting/map.md`; porting procedure and research go elsewhere in `porting/`. `make
  agents-check` scans every tracked file outside `porting/`, the `port` skill and
  `tests/golden/` and fails on a reference name (`tools/agents-index.mjs`, `LEGACY`).
- **C++ ONLY, AND NO INHERITED ARCHITECTURE.** The engine and every game on it are written in
  C++. The only other languages allowed are the thinnest platform glue a store demands
  (Swift or Objective-C for Apple's app shell, Java or Kotlin for Android's), kept as small as
  the platform allows, the small Node scripts under `tools/` that check the
  repository's structure and ship in no game, and Blender Python asset builders: offline
  sources of a model, run headless by `ge asset` (docs/goals.md M8g), whose output (a .glb)
  is committed and loaded by the engine in C++; they never run in a game, CI never needs
  Blender, and no other scripting enters the tree. Nothing about the reference sources'
  architecture (their layers, module split, file layout, state shape, names or data
  formats) is assumed to carry over: each design decision is made here, for this engine,
  and recorded in `docs/architecture.md`. Where a carried-over module keeps a behaviour (a
  golden pins its bits), that is a decision about the result, not about the structure
  around it.
- **WRITE ONLY TO THIS REPOSITORY.** The reference sources (`porting/README.md` names
  them) are read-only: clone them beside this checkout, read them and run them, but never
  commit, push, open issues or comment there. If a reference has a bug, the engine
  reproduces it and you tell the user.
- **THIS REPOSITORY IS PUBLIC.** Nothing sensitive goes in the tree, in workflow files or
  in repository variables: no tokens, keys, passwords, e-mail addresses, account or
  device ids, or paths under a home directory. Credentials live only in **GitHub
  Actions secrets** and are referenced as `${{ secrets.NAME }}`. Locally they come from
  the environment or a gitignored `.env`. Workflows request the narrowest
  `permissions:` they need, map a secret into `env:` (never interpolate it into a
  `run:` script) and never print one. Actions **variables** and workflow logs are public
  too. An agent never handles a credential's value: a maintainer enters secrets in the
  repository settings. Releases need only the built-in `GITHUB_TOKEN`
  ([`docs/publishing.md`](docs/publishing.md) lists every credential). Write every
  comment, doc and commit message for a stranger reading it.
- **ENGINE, PLUGIN OR GAME** (G8). Before the first edit, decide each file's home with the
  `plugins` skill: any genre would use it → `engine/`; any game of one genre or world →
  `plugins/<name>/`; only this game → `games/<game>/`. Dependencies run game → plugin →
  engine; `make agents-check` holds them.
- **ONLY WHAT THE GAME NEEDS** (G4). Before you build a component, name the Fall Line
  feature it serves. A general-purpose feature with no user is out of scope.
- **FAST, AND PROVEN FAST** (G2). Hot paths do not allocate, branch on virtuals or chase
  pointers. A performance claim carries a before-and-after measurement in the PR
  (`perf` skill).
- **DETERMINISTIC ON ITS OWN TERMS** (G1). The simulation produces the same IEEE 754
  doubles for the same seed and inputs on every compiler and OS, so replays, ghosts and
  tests reproduce everywhere; the golden tables in `tests/golden/` are the engine's own
  record of those bits. Matching a reference implementation's bits is NOT a requirement: a
  deliberate change of behaviour rewrites the affected goldens in the PR that makes it, and
  says why. No `<cmath>` transcendentals in simulation code, no FMA contraction and no
  fast-math. Randomness only from `core::Rng`. Every simulation module lands with a golden
  test (`determinism` and `testing` skills).
- **LABS, NOT GUESSES** (G9). Every subsystem, plugin and game has labs (`docs/labs.md`).
  A change in a lab's subject runs it before and after (`--tag=before`, then
  `--compare=before`) and puts both in the PR. A milestone row is done only when the labs
  in its **Labs it lands** cell run in `make labs-smoke` (`labs` skill).
- **AGENT-ACTIONABLE OUTPUT** (G6). Every error, assert and log line says what failed,
  where, and the next action to take (`docs/architecture.md` § Diagnostics).
- **GREEN, THEN MERGE** (G7). CI must pass on every row. A PR you open is yours to drive
  to green, and you merge it (squash) once it is green. Never skip or weaken a test to
  get there.
- **WHEN A TASK IS DONE, NAME THE NEXT STEP.** Update the roadmap row in
  `docs/goals.md`, then name the next open milestone row as the follow-up.
- **SAY WHAT `ge` LACKED.** `ge` is the agent's whole interface (G6), so it grows from what
  agents miss. When a task would have been easier with a `ge` command or flag that does not
  exist (to build, debug, inspect, measure or optimise), say so in the PR description under
  **ge wishlist** and to whoever directs the work: the command, what it would answer, and the
  steps it would have saved. Check `ge help` and the open rows in `docs/goals.md` first, and
  add a wish that recurs to the `ge` backlog row there.
- **SKILLS LEARN.** Read each loaded skill's lessons before the first edit, and before the
  commit record what the session learned, fix what the skill got wrong, and promote what
  is always true (the `skill-reflection` skill). Lessons ship in the same PR as the work.
- **THE MAIN ENVIRONMENT IS LINUX WITH NO GPU.** Every test, lab, scenario and screenshot
  harness runs headless in a GPU-less Linux container, rendering in software. The
  secondary environment is an Apple M3 Max on macOS (Metal), used only for what the
  container cannot do (GPU timings, Metal or real-GPU-only bugs, interactive play;
  `docs/rendering.md` § Where it is developed). When a task needs the Mac, say so to the
  user and open a GitHub issue labelled `needs-mac` with the exact commands, what to look
  at and what to paste back. Never do that because the Mac would only be faster.
- **`make precheck` BEFORE EVERY PUSH.** It runs, in about 40 seconds, the gates that cost
  threads the most CI rounds (fmt-check, agents-check, libm-check, lint on the changed files,
  `cli_diagnostics_test` and the tests the change reaches). A game repo's is `ge precheck`.
  Run it in the background when it is long (a tool's background mode re-invokes you when it
  exits); never poll it in a loop.
- **AFTER PUSHING A PR, CHECK IT EVERY 15 MINUTES UNTIL IT IS MERGED.** Each check-in looks at
  the checks AND the merge state, since a branch left behind main is where threads get stuck:
  `build/<preset>/ge ci <pr>` answers both in one command. A conflict: merge `origin/main`,
  resolve it, `make precheck`, push. A red check: fix it and push. Right after each push, arm
  GitHub's squash auto-merge on the PR (it merges itself once the required checks pass); the
  check-ins still catch conflicts, which auto-merge waits on forever. All green and mergeable
  and not yet merged: `build/<preset>/ge ci <pr> --merge` squash-merges it, pinned to the head
  the checks ran on. Schedule the check-ins; never sleep in a loop.
- **Every work session ends with the `commit` skill.**

## Build and test

```sh
make build        # cmake --preset debug && cmake --build (PRESET=release for -O2, no sanitizers)
make test         # build + ctest
make fmt          # clang-format in place;  make fmt-check is what CI runs
make precheck     # BEFORE EVERY PUSH: fmt, agents, libm, fragments, build, lint and the tests the change reaches (~40 s)
make lint         # clang-tidy's bug-finding checks, in parallel (BASE=origin/main: only what the change reaches)
make labs         # list the labs;  make lab LAB=level ARGS="--seeds=38" runs one
make lab LAB=synth # every voice the synth can play, metered, pictured and written as WAV
make lab LAB=audition # Fall Line's bank, the skier's beds and the bot's run, metered and written as WAV
make lab LAB=trace ARGS="--seed=38 --log=course=debug" # one run step by step: trace.tsv, charts, log.txt
make lab LAB=anim ARGS="--scenario=turns" # the skier's figure frame by frame: filmstrips, onion skins, joint faults
make lab LAB=world ARGS=--diff=before # the last run's pictures against the kept run's, per distance band
make sim SEEDS=38 # the bot down a seed: the run report table and its line on the map (the sim lab)
make sim ARGS=--rivals=3  # the same with a field of four: the field table, every line drawn
make play         # an agent at the controls: the play protocol on stdio (engine/play/session.hpp)
build/release/play bench --seeds=38 --rivals=3   # Fall Line's step timed per zone (ge bench's engine half)
make labs-smoke   # every lab at its smoke flags, twice, digests against tests/labs_digests.txt
make agents-check # symlinks, skills ↔ this router, the 1000-line cap, ge: tags (writing docs/index.md), lesson fragments
make index        # write docs/index.md from every file's ge: tags (gitignored: never committed)
make find Q="noise sample_field"  # files, skills, lessons and the porting map matching any word
make lessons ARGS="port --scope=engine/core"   # read a skill's lessons, filtered (no ARGS: the inventory)
make ge ARGS="new ../snow-racer"  # the ge command line (ge help): scaffold a game repo, then grow it with ge alone
make golden       # regenerate the golden tables (working area: porting/porting.mk, porting/README.md)
make golden-libm  # the golden tables that reach a transcendental (same place)
make libm-check   # no <cmath> transcendental in simulation code
make release-check # the changeset fragments valid + tools/release.py's tests; prints the next version
make shaders-check # every embedded WGSL shader compiled by Tint offline (needs GE_GPU)
```

**A cloud session starts the build itself.** `.claude/settings.json`'s SessionStart hook runs
`tools/session-start.sh`, which installs CI's Linux packages the container lacks and runs `make
build` (the debug preset, reusing `build/debug/`) in the background while you read. Before you
rely on the build, `tools/session-start.sh --wait` blocks until it is done and exits with its
result; the output is `build/session-start.log`, the one-line state `build/session-start.status`.

CI (`.github/workflows/ci.yml`) runs fmt-check, agents-check, libm-check, lint and shaders-check, plus build and
test on Linux x64 (GCC debug+ASan, GCC release, Clang release), Linux arm64 (GCC),
macOS (Apple Clang) and Windows (MSVC). A golden failure on one row points at that
row's compiler or libm. The Linux x64 rows install Mesa's lavapipe and run the tests under a
virtual X display with `GE_GPU_REQUIRE=1`, so the GPU and window tests run there and a skip
fails; elsewhere they run when the runner has an adapter and skip (CTest's exit 77) when not. `.github/workflows/packaging.yml` holds every PR to what a
release needs: a `.changes/unreleased/` fragment for a shipped change (or the
`no-changelog` label), and the installed package building a consumer on Linux, macOS
and Windows. Releases are dispatched, never hand-made (`release` skill,
[`docs/publishing.md`](docs/publishing.md)).

## Where things live

| Path | What | Skill |
| --- | --- | --- |
| `engine/core/` | The deterministic pool: `prng.hpp` (mulberry32), `math.hpp` (hypot family, clamp, smoothstep), `libm/` (every transcendental the simulation calls, with the same bits on every platform; vendored, with `LICENSE.md`), `num.hpp` (the simulation's number semantics: rounding, integer wrap, NaN-aware max/min, sign, remainder), `num_text.hpp` (numbers as text, for diagnostic lines), `noise.hpp` (hash2, value noise, the cached field), `heightfield.hpp` (the baked grid and its bilinear reads), `quat.hpp` (orientation; the Euler sign flip), `solar.hpp` (the sun and the moon, by geometry and on a real date), `output.hpp` (the one diagnostic channel: named log channels, the log spec, the step on every line; module state), `parallel.hpp` (a bake's independent rows in bands on threads) | `determinism`, `debug` |
| `engine/loop/` | `run_clock.hpp`: the fixed-step accumulator between the display's frames and the 120 Hz step | `sim` |
| `engine/lab/` | The lab contract and runner: `lab.hpp` (`LabSpec`, `GE_LAB`, the `Lab` context: metrics, tables, captioned shots, attached files, failures, the digest), `sound.hpp` (a rendered sound metered, pictured and attached as WAV), `runner.hpp` (the `labs` command line: list, run, tag, compare, smoke, repeat, digest baseline), `report.hpp` (report.json, metrics.tsv, the contact sheet), `raster.hpp` (the CPU picture and 5x7 font), `png.hpp` (written and read back), `diff.hpp` (the picture diff: PSNR, SSIM and the share changed per distance band, each band's floor, the four-panel sheet; `--compare`, `--diff`, `ge lab diff`), `series.hpp` (the series and spans text files every trace, bench and profile writes), `stats.hpp` (percentiles, outliers, spikes), `plot.hpp` (a drawing rendered to PNG or SVG), `chart.hpp` (line, scatter, histogram, box, heatmap and flame charts, outliers ringed), `anim.hpp` (a figure's pose track: filmstrips, onion skins, joint angles, jolt, and the pop, snap, stretch and clip faults); `labs/` holds the engine's own labs. Published as `ge::lab`, so a game's labs use the same contract | `labs`, `debug` |
| `engine/play/` | The play contract (`ge::play`): `game.hpp` (`GameSpec`, the axes, options and views, `Run`: step, over, digest, observe, state, report, pilot, shoot; `GE_GAME`), `session.hpp` (the snapped, recorded step, the line protocol, `pilot_run`, `replay`, `state_at`, `diverge`), `inputs.hpp` (the plain-text input log), `json_read.hpp` (the flat JSON reader), `shot_at` (a view drawn at a step and framed: `ge shot`), `record.hpp` (a run recorded: the view at a frame rate, encoded by ffmpeg, and a contact sheet: `ge record`), `bench.hpp` (the pilot's run replayed and timed per step and per zone, with its allocations), `main.hpp` (the `play` executable's command line). A game's `play` target is driven by `ge play`, `ge sim`, `ge replay`, `ge state`, `ge diverge`, `ge shot`, `ge record`, `ge bench`, `ge profile`, `ge mem`, `ge leaks` | `agent-interface` |
| `engine/prof/` | The profiler (`ge::prof`): `zones.hpp` (`GE_ZONE`: a span with its depth, time and allocations while a recording runs, one load when none does; the totals), `memory.hpp` (the allocation counts, the peak resident set), `alloc_hook.cpp` (the counting `operator new`, its own target `ge::prof_alloc`). Feeds no digest | `perf` |
| `engine/cli/` | The `ge` command line for game repos (`cmake/Cli.cmake`): `commands.hpp` (the catalog: every command, its flags, example and skill; the parser and `ge help`), `outcome.hpp` (the one answer shape, text or `--json`), `json.hpp`, `index.hpp` (`ge find`, `ge explain`: the repo's and the engine's tags), `build.hpp` (`ge build`, `ge test`, `ge check`, `ge lab`, `ge lab diff`: the tools' output read back as findings), `golden.hpp` (`ge digest`: a lab's digest against the committed baseline, one row moved alone; `ge golden diff`, `ge golden write`: a golden file's moves column by column), `ci.hpp` (`ge precheck`: the repo's CI gates run here, both presets; `ge ci`: a failed CI job's log as findings, over `gh api`), `graph.hpp` (`ge graph`: series and spans files charted, outliers as findings), `play.hpp` (`ge play`, `ge sim`, `ge replay`, `ge state`, `ge diverge`, `ge shot`, `ge record`: the game's `play` executable run), `perf.hpp` (`ge bench`, `ge profile`, `ge mem`, `ge leaks`: the step's cost, its zones, its allocations, its leaks), `add.hpp` (`ge use`, `ge add`), `scaffold.hpp` (`ge new`), `skills.hpp` (`ge skills sync`, the lock, plugin skills, `ge lessons`), `doctor.hpp` (`ge doctor`: a game repo held to the engine's rules), `diagnostic.hpp` (the diagnostics convention as a check: `tests/cli_diagnostics_test.cpp` holds the whole tree to it), `finding.hpp` (what, where, next), `embedded.hpp` (what ge carries) | `agent-interface` |
| `consumer-skills/` | The skills a game repo gets from `ge new` and `ge skills sync`, one `<name>/SKILL.md` each, compiled into `ge`; written for a game repo, never for this one | `agent-interface` |
| `scaffold/` | The game repo `ge new` writes: every file with a `.in` suffix and `{{name}}`-style placeholders, compiled into `ge` | `agent-interface` |
| `templates/` | What `ge add` writes: `system/`, `test/` and `lab-<kind>/`, each file with a `.in` suffix and `{{snake}}`, `{{kebab}}`, `{{Type}}` placeholders; and the starting games `ge new --game=<name>` writes over the scaffold, `game-<name>/` (`game-racer/`: a car round a loop of gates); compiled into `ge` | `agent-interface` |
| `*/labs/` | Labs beside their subject, compiled into the one `labs` executable: `engine/lab/labs/`, `plugins/<name>/labs/`, `games/<name>/labs/` | `labs` |
| `engine/platform/` | SDL3 (`cmake/SDL.cmake`, `GE_PLATFORM`): `window.hpp` (the window, its native handles, the input snapshot, the frame clock), `audio.hpp` (the output stream, pulled on SDL's audio thread; SDL's `dummy` driver headless); gamepad, touch and files as their milestones need them. The only place SDL is included | `platform` |
| `engine/gpu/` | The native GPU device over Dawn's prebuilt release (`cmake/Dawn.cmake`, `GE_GPU`): `device.hpp` (adapter, feature tiers, error lines), `pipeline.hpp` (shaders, the manifest and its creation counter), `surface.hpp`, `target.hpp` (offscreen targets, readback), `timer.hpp` (GPU ms per pass), `test_scene.hpp`; WGSL embedded by `cmake/Wgsl.cmake`. The only place webgpu.h is included | `gpu` |
| `engine/render/` | Generic rendering mechanisms (M7–M8; `docs/rendering.md`): `view.hpp` (the `FrameView` a game extracts, the camera, reversed-z), `frame.hpp` (the frame block, the named targets, the pass table and its check, the recorder and stats, layouts), `sky.hpp` (the atmosphere's tables, aerial perspective, the sky pass, group 0), `terrain.hpp` (the GPU-culled heightfield clipmap; its colour pass a material's shader over `wgsl/terrain_draw.wgsl`, `terrain.wgsl` the default), `horizon.hpp` (the ground's own shadow: the horizon maps), `hiz.hpp` (the depth pyramid), `taa.hpp` (TAA-U: the jitter, the history, the resolve), `bloom.hpp` (the glow's mip chain), `post.hpp` (the bloom mixed in, exposure, the tone curve, the grade's table), `mesh.hpp` (CPU meshes: shape builders, the packed vertex), `instances.hpp` (instance families: two-phase Hi-Z cull, LOD buckets, indirect draws, caster slots), `skinning.hpp` (a skinned mesh posed by a compute pass into its family's vertices), `shadows.hpp` (the sun's cached cascades and the figures' atlas), `particles.hpp` (the particle pool: a ring of births, the flight in compute, streaks softened by the depth); WGSL beside each, the shared library in `render/wgsl/` | `render` |
| `engine/audio/` | The synth (`ge::audio`, M9a): `voice.hpp` (the vocabulary: `Tone`, `Noise`, `LayerSpec`, `LayerTarget`), `synth.hpp` (the DSP graph: one-shots, steered layers, buses, echo, limiter; the producer-to-render ring), `dsp.hpp` (tables, polyBLEP oscillators, the SVF), `sound.hpp` (`SoundDef`, `PlayShape`, `play_def`), `rack.hpp` (a bed's layers), `meter.hpp` (level, bands, centroid, onset, length), `wav.hpp`. Deterministic, no platform; `make libm-check` holds it | `audio` |
| `engine/assets/` | Authored models (M8e, `cmake/Cgltf.cmake`): `model.hpp` (the `Model`: skinned meshes, the skeleton, materials, clips baked at 60 Hz, in the world's frame), `gltf.hpp` (the glTF 2.0 loader over cgltf: the mirror from glTF's frame, rigid parts bound to their node, what it skips), `gltf_write.hpp` (a Model as a .glb, for the tests and labs), `pose.hpp` (clip sampling, the bone palette, CPU skinning, the reach box), `test_model.hpp` (a skinned limb with a rigid fin and two clips). No GPU; `render/skinning.hpp` poses on the GPU | `assets` |
| `engine/ui/` | Immediate-mode 2D and text, plus the dev UI (M10) | `ui` |
| `plugins/` | Plugins: one directory each, one `ge_add_plugin` call in its `CMakeLists.txt`, built as `ge_<name>`; the build half is `cmake/Plugins.cmake`, the check `tools/check-plugins.mjs` | `plugins` |
| `plugins/racing/` | The racing primitives (`ge::racing`): `tape.hpp` (the control tape a ghost and a replay are kept on: the axis grid, the RLE + base64 byte format, the recorder, the reader, the map fingerprint), `records.hpp` (the record book's policy: `beats`, `split_gap`, `note_record`), `standings.hpp` (the field's order: `leg_progress`, `field_order`, `place_among`) | `racing` |
| `plugins/mountain/mapgen/` | The world generator (`ge::mountain::mapgen`): `generate.hpp` (`generate_level`, the attempts, the compile), `digest.hpp` (`level_digest`), `rules.hpp` (the rule book's numbers), `types.hpp` (the `Level`), and every step it calls. v1 (the single piste, M4a), and the ski area (M4b) stage by stage: `resort_build.hpp` (the attempt's state and its stages), `massif.hpp`, `resort.hpp` (the plan), `station_pad.hpp`, `lift_chain.hpp`, `lift_line.hpp`, `resort_rules.hpp` (the ski area's rule book's numbers), the walk (`network.hpp`, `network_build.hpp`, `net_index.hpp`, `run_start.hpp`, `summit_ramps.hpp`) and the lanes and drag lifts that link it (`lanes.hpp`, `access_build.hpp`, `drags.hpp`, `links.hpp`); the real faces (M4d): `real_face_crops.hpp` (each face's place and crop), `real_face.hpp` (a face's heights decoded and sampled), `real_faces/` (the heights, GENERATED by the `real-faces` lab's bake), `real_hints.hpp` (the real ski area's lifts, pistes and woods by height), `real_hints/` (GENERATED by the `real-hints` lab's bake off the open map data), `packed.hpp` (the varint and base64 codec both share); `GenerateOptions::face` builds a ski area on one, the massif laid off it 1:1 and the rule book bent where it asks (`resort_rules::massif::real`), leaning its stations, lifts and runs on the hints (`real_lean.hpp`); `plugins/mountain/picture/map_picture.hpp`: the map drawn from above on the CPU, whole or a window (the labs' maps, a game's `ge shot` view) | `mountain` |
| `plugins/mountain/lifts/` | The lifts run (`ge::mountain::lifts`): `lift_loop.hpp` (the lift plan, every carrier round its loop as a pure function of the clock, a detachable's terminals, the load zone, the let-go point) | `mountain` |
| `plugins/mountain/app/` | The mountain as drawn (`ge_mountain_app`, links `render`; built with the GPU): `snow.hpp` (the snow's material: the ground map, the relief, the corduroy and lanes, the wrap, the glitter, the glare, the tracks; `snow.wgsl` over the terrain's draw), `trails.hpp` (the trail map: the coarse map and the fine window, stamped with max blending, redrawn from the stamps' history), `trees.hpp` (the trees as drawn: five forms, four variants, three LODs, the snow by each face's facing, every kind's colours; the `forest` lab) | `mountain`, `render` |
| `plugins/mountain/analysis/` | The rule book re-checked on a finished map (`ge::mountain::analysis`): `analyze_level` | `mountain` |
| `plugins/skier/` | The skier on the snow (`ge::skier`): `skier.hpp` (`step_skier`, the model's order stated once), `snow.hpp` (sink, friction, grip, brake), `limits.hpp` (what a skier can do; a pilot plans on it), `skis.hpp`, `tuning.hpp`, `trees.hpp` (the trunks' grid), `synthetic.hpp` (the hand-built maps), `scenarios.hpp` (the ride scenarios the tests and the `ride` lab share) | `skier` |
| `plugins/snowmobile/` | The snowmobile on the snow (`ge::snowmobile`): `spec.hpp` (every number: the class, the probes, the grip, the drive, the crash), `sled.hpp` (`step_sled`, the step's order stated once; `SledState`, `SledControls`, `SledEvents`, `stand_sled`), `body.hpp` (the rigid body: springs, grip, carve, roll, yaw, air, hull), `drive.hpp` (the engine, the CVT, the belt), `pilot.hpp` (the bot's hands to an aim), `scenarios.hpp` (the sled scenarios the tests and the `sled` lab share) | `snowmobile` |
| `plugins/skier/app/` | The skier as seen and heard (`ge_skier_app`, links `audio`): `figure.hpp` (the placeholder figure: rigid pieces posed from a `SkierState`, and its joints for a pose track), `wind_bed.hpp` (the air on the body: rush, roar, whistle, flutter, buffet), `snow_bed.hpp` (the skis on the snow: hiss, powder hush, edge, chatter, skid), `ride_bed.hpp` (a `SkierState` turned into both and steered on two racks), `tracks.hpp` (the prints the skis lay each frame: two capsules, the outside ski deeper, the carve, the skid), `spray.hpp` (what the skis throw each frame: the skid's and the brake's sheet, the carve's curl, powder fines, the landing's burst) | `skier`, `audio` |
| `games/fall_line/sim/` | Fall Line's own simulation (`fall_line::sim`, the `fall_line_sim` target): `state.hpp` (`GameState`, the events), `game.hpp` (`create_game`, `step`: the step's order stated once), `course.hpp` (gates, a miss, the finish, the reset), `free.hpp` (the free ride: the runs as lines, the run under the skis, the foot, the reset, the chain a bot skis), `lift_ride.hpp` (the lift ride: boarding in the load zone, waiting, carried, let go at the top), `outing.hpp` (the bot's outing: a piste, the lift at its foot, another run), `sled_ride.hpp` (the free ride's snowmobile: its spot, the machine press, ridden, hopped off, thrown and put back on), `sled_outing.hpp` (the bot's outing on it: up a run, down on skis), `controls.hpp` (the tape's schema, `snap_controls`), `bot.hpp` (the speed plan and the pursuit), `field.hpp` (the rivals' deal, skier against skier, the standings), `simulate.hpp` (`Driver`: the one taped, hashed step; the headless run, `RunReport`, replay), `wanted.hpp` (the wanted level: the stars, the climb, the sight line, the search, the arrest; its numbers `wanted_rules.hpp`), `army.hpp` (the army on foot: the lifts' squads, the ground waves, the chase, the stand-off, the arrest; each man's way over the snow `army_troop.hpp`, its numbers `army_rules.hpp`), `play.hpp` (the game on the play contract: its axes, options, observation, state, pilot; `games/fall_line/play/` is its `play` executable) | `sim` |

| `games/fall_line/sound/` | Fall Line's sound (`fall_line_sound`, no GPU): `bank.hpp` (every one-shot as `SoundDef` data), `route.hpp` (the player's events to the bank per step, the ride bed per frame), `listener.hpp` (one row per camera seat), `mix.hpp` (the faders, `mix_of`, the two buses) | `audio`, `sim` |
| `games/fall_line/labs/` | Fall Line's labs: `sim` (the bot over seeds, `--rivals` for a field; the headless sim CLI, `make sim`), `free` (the bot on the free ride down every piste), `play` (an agent at the controls over stdio), `audition` (the bank, the beds and the bot's run, metered), `wanted` (the wanted level against scripted pursuers, the sight sheds), `army` (the army on foot after a still man: the table and every soldier's path), and the GPU's: `world` (the frame's pictures), `snow` (the snow's material up close, plain and with it), `trails` (the tracks through a turn, and another racer's still there), `spray` (the spray off a carve, a hockey stop, a landing and powder turns), `taa`, `shadows`, `profile` (GPU ms per pass and the CPU half over a run) | `sim`, `labs`, `audio` |
| `games/fall_line/app/` | Fall Line's presentation (`fall_line_app`, spelled `fall_line/app/<file>.hpp`): `frame.hpp` (the game's pass table over engine/render), `look.hpp` (the ground's albedo), `scene.hpp` (the forest's and the racers' instance families), `views.hpp` (the named views and the map's day), `main.cpp` (the `fall_line` window: the frame loop on the run clock, the bot's run on its steps, the sound on the device); then the HUD | `platform`, `gpu`, `render`, `ui`, `audio` |
| `tests/` | `<topic>_test.cpp`, one executable each; `check.hpp` (harness), `golden.hpp`, `golden/` | `testing` |
| `porting/` | The WORKING AREA (above): the porting guide, the porting map, the golden-table generators. Deleted when porting ends | `port` |
| `tools/check-agents.sh`, `tools/agents-index.mjs` | The structure check; the `ge:` tag check, `docs/index.md` and `make find` | `write-code` |
| `tools/skill-lessons.mjs` | Reads, filters and checks the skills' lesson fragments (`make lessons`) | `skill-reflection` |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `Makefile`, `.github/workflows/` | The build and CI | `build` |

### Finding things

Agents find code by grep, so the tree is built to be grepped:

- **Every source file ends its leading comment with a `ge:` tag block**: `ge:summary` (one
  line), `ge:keywords` (lower-case concepts in the engine's words, kebab or snake case),
  `ge:skill`, and `ge:test` where it applies. The format is the `write-code` skill's §
  Discovery tags. `make agents-check` fails an untagged file, an unknown skill, a missing
  test path, a keyword with a capital, and the retired tag that named a file's reference.
- **`make find Q="word word"`** searches every file's tags, every skill's description,
  every lesson's title, scope and concepts, and the porting map's rows at once (so a
  reference identifier still finds the engine file while porting lasts). `grep -rn "ge:keywords.*<word>"` is the
  same search for files alone. A search that misses is fixed by adding the word you tried to
  the right file's `ge:keywords` (`skill-reflection`, question 6).
- **`docs/index.md`** is generated from the tags by `make index` (and by `make agents-check`) in
  your own checkout. It is gitignored, so it is never committed and never conflicts: run
  `make index` to read it, or `make find` to search without it.
- **One name, everywhere**: a module `engine/loop/run_clock.hpp` has its test
  `tests/loop_run_clock_test.cpp` and its golden `tests/golden/loop_run_clock.txt`; a plugin's
  test is `tests/<plugin>_<file>_test.cpp`. Names are `snake_case`.

### Stated once

- **The FP flags**: `ge_fp` in the root `CMakeLists.txt`, linked by `ge_core` (so by every
  target) and published with it as `ge::fp`. **The instruction-set baseline** (AVX2 on
  x86-64): `ge_isa` beside it, published as `ge::isa`. `ge_flags` is the tree's own warnings and
  sanitizers, never published.
- **The plugin contract** (homes, layout, naming, dependencies, registration):
  `docs/architecture.md` § Plugins; built by `cmake/Plugins.cmake`, checked by
  `tools/check-plugins.mjs`.
- **World conventions**: the plan is x/z with y up; heading 0 = +z,
  clockwise from above; pitch tips-up positive; roll right-side-down positive; `steer`
  and `edge` positive to the right. Stated in `engine/core/math.hpp`'s header.
- **The PRNG**: `engine/core/prng.hpp`. Nothing else makes random numbers.
- **The transcendentals**: `engine/core/libm/libm.hpp` says which vendored upstream each
  function is (fdlibm, and glibc's pow). Nothing in simulation code calls `<cmath>`'s.
- **Number semantics** (rounding, `to_int32`/`to_uint32` wrap, `imul`, NaN-aware
  `max`/`min`, `sign`, `mod`): `engine/core/num.hpp`; numbers as text (`to_fixed`,
  `to_string`): `engine/core/num_text.hpp`.
- **The test harness**: `tests/check.hpp`. Bit comparison is `CHECK_BITS`.
- **The audio rate and format**: `kRate` (48 kHz) in `engine/audio/voice.hpp`, interleaved stereo
  float; the device converts to the hardware. The faders are the synth's buses (`bus_gain`).

## Commit and PR conventions

- Conventional commits (`feat(core): …`, `fix(render): …`, `docs: …`, `ci: …`), scoped by
  directory. PRs are squash-merged, and the PR title is itself a conventional subject.
- A PR states which milestone row it serves and includes its golden or perf evidence.

## Skills

Skills live in `.agents/skills/`, each one a `SKILL.md` with `name` and `description`
front matter and a `.lessons/` directory (`skill-reflection`). `.agents/skills/README.md`
lists them by group, and `tools/check-agents.sh` checks that this table, that README
and the directories agree.

| Skill | Load it when |
| --- | --- |
| `start-work` | Before any task: a clean tree, synced with `origin/main`, the milestone row named |
| `write-code` | On every code change: file shape, naming, comments, the edit loop, the line cap |
| `commit` | To end every session: the gates, the commit, the push, the PR, driving it to green, merging |
| `skill-reflection` | At the start and end of a session: read and record a skill's lessons |
| `determinism` | Anything in `engine/core`, any floating-point or RNG question, a golden failure |
| `port` | Carrying a module over from its reference source (the working area's skill: `porting/`) |
| `testing` | Writing a test, a golden table or a golden generator |
| `build` | CMake, presets, dependencies (`FetchContent`), compiler flags, CI workflows |
| `release` | A changeset fragment, `cmake/Packaging.cmake`, a public target, cutting or dry-running a release |
| `plugins` | Deciding where code lives (engine, plugin or game), adding a plugin, moving code between homes |
| `racing` | `plugins/racing`: the control tape, the record book's policy, the standings |
| `mountain` | `plugins/mountain`: the generated ski mountain (mapgen, analysis, `level_digest`) |
| `skier` | `plugins/skier`: the skier on the snow (`step_skier`, the snow, the limits), the hand-built maps, the ride scenarios |
| `snowmobile` | `plugins/snowmobile`: the snowmobile on the snow (`step_sled`, the body, the drive), what throws its rider, the bot's hands on it, the sled scenarios |
| `sim` | `games/fall_line/sim`: `create_game`, the step, the bot, the headless sim CLI, the run clock |
| `platform` | `engine/platform`: SDL3, the window, input, the gamepad, the audio device, files |
| `gpu` | `engine/gpu`: the native GPU device over Dawn (Dawn's `webgpu.h`), feature tiers, buffers, pipelines and the manifest, WGSL and `make shaders-check` |
| `render` | A draw pass, the pass table, GPU-driven instances, shadows, sky, TAA-U, post, lab captures, extraction from `GameState` |
| `assets` | `engine/assets`: an authored model (glTF), posing and skinning, the `models` lab, exporting from Blender |
| `audio` | The synth, a voice, a bed, the audio thread |
| `ui` | The 2D layer, text, the HUD, a card, the dev UI |
| `labs` | Writing, running or reading a lab: a measurement, a captioned screenshot, a scenario, a drive, a bench; the before-and-after a PR owes |
| `debug` | Finding out why a run behaves as it does: log channels and `--log` dumps, series traces and spans, the outlier rule, the charts (PNG + SVG), `ge graph` |
| `perf` | Any optimisation, a benchmark, a profile, a cost claim |
| `agent-interface` | Diagnostics wording, consumer skills, the `ge` CLI (M12) |
