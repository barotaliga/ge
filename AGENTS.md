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

A C++20 game engine, built to run **Fall Line** (`niclaslindstedt/game5`, a downhill
skiing game in TypeScript) natively and fast. Fall Line is ported here first and stays in
the tree as the demo game (`games/fall_line/`). The generic half comes from porting
`niclaslindstedt/oss-game-framework`. **The goal spec and the roadmap are
[`docs/goals.md`](docs/goals.md).** Every task serves one of its milestone rows and
updates that row's Status in its PR.

| Doc | What it holds |
| --- | --- |
| [`docs/goals.md`](docs/goals.md) | The long-term goals (G1–G7), the non-goals and the milestone roadmap |
| [`docs/research.md`](docs/research.md) | What game5 needs, how established engines solve each part, the libm measurement, and the decisions |
| [`docs/architecture.md`](docs/architecture.md) | The layers and their dependency direction, the frame, the code conventions, diagnostics, the determinism contract and the build |
| [`docs/porting.md`](docs/porting.md) | How a TS module becomes C++: the oracle, layout and type mapping, the JS-semantics traps, parity levels |
| [`docs/index.md`](docs/index.md) | GENERATED from the code's `ge:` tags: every file's one-line summary, owning skill, oracle and test; oracle file to port; keyword to files |
| [`docs/publishing.md`](docs/publishing.md) | What a release is (GitHub Releases + GitHub Packages), how a game consumes it, changeset fragments, cutting a release, the credentials |

## Rules for every task

- **WRITE ONLY TO THIS REPOSITORY.** `game5` and `oss-game-framework` are the read-only
  **oracle**. Clone them beside this checkout (`../game5`, `../oss-game-framework`),
  read them and run them, but never commit, push, open issues or comment there. If the
  oracle has a bug, the port reproduces it and you tell the user.
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
- **ONLY WHAT GAME5 NEEDS** (G4). Before you build a component, name the game5 feature
  it serves. A general-purpose feature with no game5 user is out of scope.
- **FAST, AND PROVEN FAST** (G2). Hot paths do not allocate, branch on virtuals or chase
  pointers. A performance claim carries a before-and-after measurement in the PR
  (`perf` skill).
- **BITS, NOT CLOSENESS** (G1). The simulation must reproduce the oracle's IEEE 754
  doubles. No `<cmath>` transcendentals in simulation code, no FMA contraction and no
  fast-math. Randomness only from `core::Rng`. Every port lands with a golden parity
  test (`determinism` and `port` skills).
- **AGENT-ACTIONABLE OUTPUT** (G6). Every error, assert and log line says what failed,
  where, and the next action to take (`docs/architecture.md` § Diagnostics).
- **GREEN, THEN MERGE** (G7). CI must pass on every row. A PR you open is yours to drive
  to green, and you merge it (squash) once it is green. Never skip or weaken a test to
  get there.
- **WHEN A TASK IS DONE, NAME THE NEXT STEP.** Update the roadmap row in
  `docs/goals.md`, then name the next open milestone row as the follow-up.
- **SKILLS LEARN.** Read each loaded skill's lessons before the first edit, and before the
  commit record what the session learned, fix what the skill got wrong, and promote what
  is always true (the `skill-reflection` skill). Lessons ship in the same PR as the work.
- **Every work session ends with the `commit` skill.**

## Build and test

```sh
make build        # cmake --preset debug && cmake --build (PRESET=release for -O2, no sanitizers)
make test         # build + ctest
make fmt          # clang-format in place;  make fmt-check is what CI runs
make lint         # clang-tidy over the sources (bug-finding checks only)
make agents-check # symlinks, skills ↔ this router, the 1000-line cap, ge: tags, docs/index.md fresh, lesson fragments
make index        # regenerate docs/index.md from every file's ge: tags
make find Q="noise sampleField"   # files, skills and lessons matching any word
make lessons ARGS="port --scope=engine/core"   # read a skill's lessons, filtered (no ARGS: the inventory)
make golden       # regenerate tests/golden/core_*, loop_*, racing_* from the TS oracle (needs Node 22+, ../oss-game-framework and ../game5)
make golden-libm NODE=<node 24>  # regenerate tests/golden/libm_*, core_quat, core_solar, mapgen_generate under game5's pinned Node, on Linux x86-64
make libm-check   # no <cmath> transcendental in simulation code
make release-check # the changeset fragments valid + tools/release.py's tests; prints the next version
```

CI (`.github/workflows/ci.yml`) runs fmt-check, agents-check, libm-check and lint, plus build and
test on Linux x64 (GCC debug+ASan, GCC release, Clang release), Linux arm64 (GCC),
macOS (Apple Clang) and Windows (MSVC). A parity failure on one row points at that
row's compiler or libm. `.github/workflows/packaging.yml` holds every PR to what a
release needs: a `.changes/unreleased/` fragment for a shipped change (or the
`no-changelog` label), and the installed package building a consumer on Linux, macOS
and Windows. Releases are dispatched, never hand-made (`release` skill,
[`docs/publishing.md`](docs/publishing.md)).

## Where things live

| Path | What | Skill |
| --- | --- | --- |
| `engine/core/` | The deterministic pool: `prng.hpp` (mulberry32), `math.hpp` (hypot family, clamp, smoothstep), `libm/` (every transcendental game5 calls, at Node 24's bits; vendored, with `LICENSE.md`), `js.hpp` (JS number semantics), `js_text.hpp` (`toFixed` and `String(n)`, for log lines in the oracle's words), `noise.hpp` (hash2, value noise, the cached field), `heightfield.hpp` (the baked grid and its bilinear reads), `quat.hpp` (orientation; the Euler sign flip), `solar.hpp` (the sun and the moon, by geometry and on a real date), `output.hpp` (the one diagnostic channel; module state) | `determinism` |
| `engine/racing/` | The racing primitives: `tape.hpp` (the control tape a ghost and a replay are kept on: the axis grid, the RLE + base64 byte format, the recorder, the reader, the map fingerprint), `records.hpp` (the record book's policy: `beats`, `split_gap`, `note_record`), `standings.hpp` (the field's order: `leg_progress`, `field_order`, `place_among`) | `port` |
| `engine/loop/` | `run_clock.hpp`: the fixed-step accumulator between the display's frames and the 120 Hz step | `sim` |
| `engine/platform/` | SDL3: window, input, gamepad, audio device, files (M6); the only place SDL is included | `platform` |
| `engine/gpu/` | The webgpu.h wrapper (M6); the only place webgpu.h is included | `gpu` |
| `engine/render/` | Generic draw passes Fall Line needs (M7–M8) | `render` |
| `engine/audio/` | The synth's DSP graph (M9) | `audio` |
| `engine/ui/` | Immediate-mode 2D and text, plus the dev UI (M10) | `ui` |
| `games/fall_line/sim/mapgen/` | game5's `engine/mapgen/` ported, a file per oracle module: `generate.hpp` (`generate_level`, the attempts, the compile), `digest.hpp` (`level_digest`), `rules.hpp` (LEVEL_RULES), `types.hpp` (the `Level`), and every step it calls. v1 (the single piste) so far (M4a); the ski area is M4b | `sim` |
| `games/fall_line/sim/analysis/` | game5's `engine/analysis/` ported: `analyze_level`, the rule book re-checked on a finished map, in the oracle's own message text | `sim` |
| `games/fall_line/sim/` | game5's `engine/` ported: the step and the bot next (M5), library `fall_line_sim` | `sim` |
| `games/fall_line/app/` | game5's `pwa/` ported: the frame, the look, the HUD, the beds | `render`, `ui`, `audio` |
| `tests/` | `<topic>_test.cpp`, one executable each; `check.hpp` (harness), `golden.hpp`, `golden/` | `testing` |
| `tools/parity/` | Node scripts that run the TS oracle and write `tests/golden/` | `testing` |
| `tools/check-agents.sh`, `tools/agents-index.mjs` | The structure check; the `ge:` tag check, `docs/index.md` and `make find` | `write-code` |
| `tools/skill-lessons.mjs` | Reads, filters and checks the skills' lesson fragments (`make lessons`) | `skill-reflection` |
| `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `.github/workflows/` | The build and CI | `build` |

### Finding things

Agents find code by grep, so the tree is built to be grepped:

- **Every source file ends its leading comment with a `ge:` tag block**: `ge:summary` (one
  line), `ge:keywords` (kebab concepts plus the oracle's TypeScript identifiers),
  `ge:skill`, and `ge:oracle` / `ge:test` where they apply. The format is the `write-code`
  skill's § Discovery tags. `make agents-check` fails an untagged file, an unknown skill or a
  missing test path.
- **`make find Q="word word"`** searches every file's tags, every skill's description and
  every lesson's title, scope and concepts at once. `grep -rn "ge:keywords.*<word>"` is the
  same search for files alone. A search that misses is fixed by adding the word you tried to
  the right file's `ge:keywords` (`skill-reflection`, question 6).
- **[`docs/index.md`](docs/index.md)** is generated from the tags by `make index`; commit it
  with the change. On a merge conflict there, take either side and rerun `make index`.
- **Names mirror the oracle** (`run-clock.ts` → `engine/loop/run_clock.hpp`, its test
  `tests/loop_run_clock_test.cpp`, its golden `tests/golden/loop_run_clock.txt`; `createGame`
  → `create_game`), so the oracle's name is a search key for the port.

### Stated once

- **The FP flags**: `ge_fp` in the root `CMakeLists.txt`, linked by `ge_core` (so by every
  target) and published with it as `ge::fp`. `ge_flags` is the tree's own warnings and
  sanitizers, never published.
- **World conventions** (from game5): the plan is x/z with y up; heading 0 = +z,
  clockwise from above; pitch tips-up positive; roll right-side-down positive; `steer`
  and `edge` positive to the right. Stated in `engine/core/math.hpp`'s header.
- **The PRNG**: `engine/core/prng.hpp`. Nothing else makes random numbers.
- **The transcendentals**: `engine/core/libm/libm.hpp` says which upstream each function
  is and why (V8's fdlibm, and glibc's pow for `Math.pow`). Nothing in simulation code
  calls `<cmath>`'s.
- **JS number semantics** (`Math.round`, `|0`, `>>>0`, `Math.imul`, `Math.max`/`min`,
  `Math.sign`, `%`): `engine/core/js.hpp`; numbers as text (`toFixed`, `String(n)`):
  `engine/core/js_text.hpp`.
- **The test harness**: `tests/check.hpp`. Bit comparison is `CHECK_BITS`.

## Commit and PR conventions

- Conventional commits (`feat(core): …`, `fix(render): …`, `docs: …`, `ci: …`), scoped by
  directory. PRs are squash-merged, and the PR title is itself a conventional subject.
- A PR states which milestone row it serves and includes its parity or perf evidence.

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
| `determinism` | Anything in `engine/core`, any floating-point or RNG question, a parity failure |
| `port` | Porting any game5 or framework module to C++ |
| `testing` | Writing a test, a golden table or a parity script |
| `build` | CMake, presets, dependencies (`FetchContent`), compiler flags, CI workflows |
| `release` | A changeset fragment, `cmake/Packaging.cmake`, a public target, cutting or dry-running a release |
| `sim` | `games/fall_line/sim`: mapgen, the step, the bot, the headless sim CLI, the run clock |
| `platform` | `engine/platform`: SDL3, the window, input, the gamepad, the audio device, files |
| `gpu` | `engine/gpu`: the webgpu.h device, surfaces, buffers, pipelines, WGSL |
| `render` | A draw pass, the frame's pass order, a shader, extraction from `GameState` |
| `audio` | The synth, a voice, a bed, the audio thread |
| `ui` | The 2D layer, text, the HUD, a card, the dev UI |
| `perf` | Any optimisation, a benchmark, a profile, a cost claim |
| `agent-interface` | Diagnostics wording, consumer skills, the `ge` CLI (M12) |
