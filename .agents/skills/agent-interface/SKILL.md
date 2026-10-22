---
name: agent-interface
description: "Use for how the engine talks to agents in gameengine2 and in the game repos that consume it: the diagnostics convention and its test, the consumer skills (consumer-skills/), the game scaffold (scaffold/), and the ge CLI in engine/cli (ge new, ge skills sync, ge doctor, ge lessons)."
---

# The agent interface

Agents build this engine, and agents build the games that use it (G6). The engine's
output and tooling are written for them.

## Diagnostics (applies to every message)

A message is a prompt with three parts:

1. **What** failed, in one line, with the values involved (hex for doubles when
   bits matter).
2. **Where**: file:line, and the game context (seed, step, system, pass).
3. **Next**: the command to run, the skill to load, or the doc to read.

Example: see `docs/architecture.md` § Diagnostics. Do not write bare error codes,
"something went wrong", or a stack trace without a sentence.

**It is a test, not a style.** `engine/cli/diagnostic.hpp` states the rule a program can hold
(three words or more, and a next-action marker: `next:`, `skill:`, `make `, `run `, `rerun`,
`see `, `docs/`) and finds the messages: `output::error`/`warn` (or through an `out` alias),
`GE_ASSERT`, a lab's `fail`, and CMake's `message(FATAL_ERROR|SEND_ERROR|WARNING ...)`.
`tests/cli_diagnostics_test.cpp` runs it over `engine/`, `plugins/`, `games/`, `scaffold/`,
`cmake/` and the root `CMakeLists.txt`; `ge doctor` runs it over a game repo. Only a call's
string literals are judged, so put the next action in a literal, not in a variable.

## The `ge` CLI (`engine/cli`, `cmake/Cli.cmake`)

| Command | What | Code |
| --- | --- | --- |
| `ge new <dir> [--name=] [--engine-sha256=]` | A game repo from `scaffold/`, its links, its skills | `scaffold.hpp` |
| `ge skills sync [--force]` | The engine's skills rewritten to this ge's version, the lock and the `AGENTS.md` block | `skills.hpp` |
| `ge doctor` | A game repo held to the engine's rules; a game's `make agents-check` | `doctor.hpp` |
| `ge lessons [<skill>] [--scope=]` | The skills' lessons read back; a game's `make lessons` | `skills.hpp` |

- **ge carries what it writes.** `cmake/EmbedFiles.cmake` compiles every
  `consumer-skills/*/SKILL.md`, every `scaffold/**` file and `VERSION` into it, so a ge writes
  exactly its release's skills and scaffold. A game repo builds the ge of the engine it pins
  (`cmake/Cli.cmake` runs in a FetchContent build too), and its Makefile runs that one, so the
  version always matches; `ge doctor` reports a mismatch.
- **Every message is a `Finding`** (`finding.hpp`: severity, what, where, next), printed by
  `format_finding`. Exit codes: 0 done, 1 the repo breaks a rule, 2 a usage error.
- **The engine skills a game owns only by lock.** Sync writes an engine skill only while its
  hash matches `.agents/skills/ge-skills.lock`; a hand-edited one, or a repo skill of the same
  name, is left and reported until `--force`. `.lessons/` are never touched. A skill the engine
  drops is removed with its lessons kept.
- **Scaffold with a released ge** (a package's `bin/ge`). `VERSION` on `main` is the last
  release, so a ge built from `main` pins that release, which lacks whatever `main` added since
  (v0.1.0 has no `ge::lab` and no `ge`): its scaffold downloads and verifies, then fails to
  configure. The release workflow writes the new `VERSION` before it builds, so a released ge
  pins itself.
- **Try it here:** `make ge ARGS="new ../snow-racer"`, then in that repo
  `cmake --preset debug -DFETCHCONTENT_SOURCE_DIR_GAMEENGINE2=<this tree>`, `make test` and
  `make agents-check`. Packaging's `scaffold` job does exactly that on every PR.

## Consumer skills (`consumer-skills/`)

The skills a game repo needs, written for a game repo (its paths, its `make` targets, the
engine as a dependency), never for this one: `start-work`, `write-code`, `engine` (the pin,
the targets, determinism, upgrading, reporting an engine bug), `sim-system`, `labs`,
`skill-reflection` and `commit`. Each is a `SKILL.md` alone: lessons accumulate in each game
repo, and a lesson that would hold for every game is promoted upstream into the skill's text.
A new consumer skill is a directory with its `SKILL.md`; `tests/cli_skills_test.cpp` checks
its front matter, and the scaffold's `AGENTS.md` routes it through the block sync writes.

## The scaffold (`scaffold/`)

Every file carries a `.in` suffix (so no tool here formats, lints or compiles it) and may use
`{{name}}`, `{{ident}}`, `{{engine_version}}`, `{{engine_sha256}}` and `{{engine_repo}}`
(`scaffold.hpp`). Its sources must pass the game's own `make fmt-check` and `ge doctor`:
after an edit, scaffold a repo, run `make fmt` there, and copy the formatted files back with
the placeholders restored. `tests/cli_scaffold_test.cpp` checks a fresh repo is complete and
passes `ge doctor`; the `scaffold` job builds and tests it.

## Labs

A lab's `fail` lines follow the diagnostics shape, and its report, `report.json` and
contact sheet are written for an agent to read first. `labs play --stdio` (M5c, the protocol
in `games/fall_line/sim/play.hpp` and `docs/labs.md` § Drive labs) is the engine's playtest
interface for agents. A game's labs run on the same contract through `ge::lab`: the scaffold's
`labs/` directory and the `labs` consumer skill.
