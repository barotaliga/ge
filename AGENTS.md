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
  `permissions:` they need. Write every comment, doc and commit message for a stranger
  reading it.
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
- **Every work session ends with the `commit` skill.**

## Build and test

```sh
make build        # cmake --preset debug && cmake --build (PRESET=release for -O2, no sanitizers)
make test         # build + ctest
make fmt          # clang-format in place;  make fmt-check is what CI runs
make lint         # clang-tidy over the sources (bug-finding checks only)
make agents-check # symlinks, skills ↔ this router, the 1000-line cap
make golden       # regenerate tests/golden/core_* from the TS oracle (needs Node 22+ and ../oss-game-framework)
make golden-libm NODE=<node 24>  # regenerate tests/golden/libm_* under game5's pinned Node, on Linux x86-64
make libm-check   # no <cmath> transcendental in simulation code
```

CI (`.github/workflows/ci.yml`) runs fmt-check, agents-check, libm-check and lint, plus build and
test on Linux x64 (GCC debug+ASan, GCC release, Clang release), Linux arm64 (GCC),
macOS (Apple Clang) and Windows (MSVC). A parity failure on one row points at that
row's compiler or libm.

## Where things live

| Path | What | Skill |
| --- | --- | --- |
| `engine/core/` | The deterministic pool: `prng.hpp` (mulberry32), `math.hpp` (hypot family, clamp, smoothstep), `libm/` (every transcendental game5 calls, at Node 24's bits; vendored, with `LICENSE.md`), `js.hpp` (JS number semantics). Next: noise, heightfield, quat, solar, output | `determinism` |
| `engine/racing/` | Tape, records, standings (M3) | `port` |
| `engine/loop/` | The fixed-step run clock (M2) | `sim` |
| `engine/platform/` | SDL3: window, input, gamepad, audio device, files (M6); the only place SDL is included | `platform` |
| `engine/gpu/` | The webgpu.h wrapper (M6); the only place webgpu.h is included | `gpu` |
| `engine/render/` | Generic draw passes Fall Line needs (M7–M8) | `render` |
| `engine/audio/` | The synth's DSP graph (M9) | `audio` |
| `engine/ui/` | Immediate-mode 2D and text, plus the dev UI (M10) | `ui` |
| `games/fall_line/sim/` | game5's `engine/` ported: mapgen, the step, the bot (M4–M5) | `sim` |
| `games/fall_line/app/` | game5's `pwa/` ported: the frame, the look, the HUD, the beds | `render`, `ui`, `audio` |
| `tests/` | `<topic>_test.cpp`, one executable each; `check.hpp` (harness), `golden.hpp`, `golden/` | `testing` |
| `tools/parity/` | Node scripts that run the TS oracle and write `tests/golden/` | `testing` |
| `tools/check-agents.sh` | The structure check | `write-code` |
| `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `.github/workflows/` | The build and CI | `build` |

### Stated once

- **The FP flags**: `ge_flags` in the root `CMakeLists.txt`. Every target links it.
- **World conventions** (from game5): the plan is x/z with y up; heading 0 = +z,
  clockwise from above; pitch tips-up positive; roll right-side-down positive; `steer`
  and `edge` positive to the right. Stated in `engine/core/math.hpp`'s header.
- **The PRNG**: `engine/core/prng.hpp`. Nothing else makes random numbers.
- **The transcendentals**: `engine/core/libm/libm.hpp` says which upstream each function
  is and why (V8's fdlibm, and glibc's pow for `Math.pow`). Nothing in simulation code
  calls `<cmath>`'s.
- **JS number semantics** (`Math.round`, `|0`, `>>>0`, `Math.imul`, `Math.max`/`min`,
  `Math.sign`, `%`): `engine/core/js.hpp`.
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
| `sim` | `games/fall_line/sim`: mapgen, the step, the bot, the headless sim CLI, the run clock |
| `platform` | `engine/platform`: SDL3, the window, input, the gamepad, the audio device, files |
| `gpu` | `engine/gpu`: the webgpu.h device, surfaces, buffers, pipelines, WGSL |
| `render` | A draw pass, the frame's pass order, a shader, extraction from `GameState` |
| `audio` | The synth, a voice, a bed, the audio thread |
| `ui` | The 2D layer, text, the HUD, a card, the dev UI |
| `perf` | Any optimisation, a benchmark, a profile, a cost claim |
| `agent-interface` | Diagnostics wording, consumer skills, the `ge` CLI (M12) |
