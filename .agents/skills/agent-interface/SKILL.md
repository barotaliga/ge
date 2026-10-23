---
name: agent-interface
description: "Use for how the engine talks to agents in gameengine2 and in the game repos that consume it: the diagnostics convention and its test, the consumer skills (consumer-skills/), the game scaffold (scaffold/), the ge CLI in engine/cli (its catalog, the --json answer, find, explain, new, use, add, build, test, check, lab, play, sim, replay, state, diverge, doctor, skills sync, lessons), the play contract in engine/play and the templates ge add writes (templates/)."
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

`ge` is the whole interface an agent has to a game repo: there is no editor. `commands.cpp`'s
`kCommands` is the catalog (each command's arguments, flags, stage of the loop, summary,
example and consumer skill); the parser, `ge help` and `ge help --json` all read it, and
`tests/cli_commands_test.cpp` fails a command without its help, example or skill, and one the
`ge` consumer skill does not mention.

| Command | What | Code |
| --- | --- | --- |
| `ge help [<command>]`, `ge version` | The catalog, as text or `--json` | `commands.hpp` |
| `ge find <words>`, `ge explain <file\|name>` | The repo's and the engine's files, skills and lessons by their `ge:` tags; one file's header, a symbol's declaration | `index.hpp` |
| `ge new <dir> [--game=racer]` | A game repo from `scaffold/`, its links, its skills; a starting game other than the demo writes its files from `templates/game-<name>/` over it | `scaffold.hpp` |
| `ge use <plugin>` | Link a plugin into the game's sim, sync its skill (and the skills of the plugins it brings) | `add.hpp` |
| `ge add system\|lab\|test <name>` | A working piece from `templates/`, tags filled, formatted, refusing to write over a file | `add.hpp` |
| `ge build`, `ge test`, `ge check`, `ge lab` | Configure, build, test, the whole gate, a lab: the tool's output in `build/<preset>/ge-*.log`, the answer as findings | `build.hpp` |
| `ge test --affected`, `ge lint` | What a change reaches (git's changed files, Ninja's deps, through the headers of reached libraries' directories): its tests run, its sources linted | `reach.hpp`, `lint.hpp` |
| `ge play`, `ge sim`, `ge replay`, `ge state`, `ge diverge`, `ge shot` | The game's `play` executable built and run: the protocol on the terminal, the pilot's runs and their input logs, a log replayed to its digest, the state at a step, where two runs part, a captioned still of a view at a step (`--gpu --mode=<m>`: drawn by the game's registered renderer, engine/play/game.hpp § THE GPU'S PICTURES) | `play.hpp` |
| `ge doctor`, `ge skills sync`, `ge lessons` | The repo against the rules; the engine's skills and the lock; a skill's lessons | `doctor.hpp`, `skills.hpp` |

- **One answer shape.** Every command builds an `Outcome` (`outcome.hpp`: findings, written,
  result, summary, next) and `emit` prints it as text or, with `--json`, one object
  `{command, ok, exit, findings, written, result, summary, next}`. A failure's `next` is
  blank: its findings carry the next actions. Exit codes: 0 done, 1 a finding is an error, 2 a
  usage error.
- **Output read back, not passed through.** `compiler_findings` and `test_findings`
  (`build.cpp`) turn GCC/Clang, MSVC, CMake, linker, clang-format, CTest and the harness's
  `FAIL` lines into findings at a repo-relative `file:line`, deduplicated and capped. A tool
  whose output it does not recognise still fails with a finding that names the log. A new
  shape is a case in `tests/cli_build_test.cpp` first.
- **The play contract** (`engine/play`, `ge::play`) is the engine's half of `ge play` and its
  siblings: a game fills `GameSpec` and `Run` (`game.hpp`) and registers it with `GE_GAME` in its
  `play` executable, whose `main` is `play_main` (`main.hpp`). Everything else is the engine's,
  once for every game: `Session` snaps every input onto its axis's grid, records it on the input
  log and steps the run (`session.hpp`), so an agent's run, the pilot's and a replay are written
  down the same way. The play executable answers each batch command with one JSON line;
  `play_answer` (`cli/play.cpp`) turns it into an outcome. The protocol and the log format are
  stated in `session.hpp` and `inputs.hpp` alone; `docs/labs.md` § Drive labs and the
  `play-test` consumer skill describe them, so a change to either moves all three.
  `tests/play_session_test.cpp` holds the session on a toy game, `tests/fall_line_play_test.cpp`
  on Fall Line, and the `scaffold` job plays a fresh repo.
- **Every command is budgeted.** The `cli-loop` lab runs the loop on a scaffolded repo and
  fails a text answer over its line budget: an agent pays for every line.
- **ge carries what it writes.** `cmake/EmbedFiles.cmake` compiles every
  `consumer-skills/*/SKILL.md`, every `scaffold/**` and `templates/**` file, every
  `plugins/*/CMakeLists.txt` (so `ge use` knows each plugin's links) and `VERSION` into it. A
  game repo builds the ge of the engine it pins; a ge of another release run in a repo hands
  the command to `build/<preset>/ge` once that is built (`main.cpp`'s `hand_to_pinned`), so
  the rules are always the pinned release's.
- **Templates** (`templates/<kind>/`, `.in` files) use `{{name}}`, `{{ident}}`, `{{snake}}`,
  `{{kebab}}` and `{{Type}}`. Like the scaffold, each must pass a game's format and `ge doctor`:
  `tests/cli_add_test.cpp` checks every piece is filled and doctor-clean, and the `scaffold`
  job builds and tests every one.
- **Plugin skills.** A consumer skill with `plugin: <name>` in its front matter is synced only
  into a repo that links that plugin, directly or through another (`skills_for`).
- **Every message is a `Finding`** (`finding.hpp`: severity, what, where, next), printed by
  `format_finding`.
- **The engine skills a game owns only by lock.** Sync writes an engine skill only while its
  hash matches `.agents/skills/ge-skills.lock`; a hand-edited one, or a repo skill of the same
  name, is left and reported until `--force`. `.lessons/` are never touched. A skill the engine
  drops is removed with its lessons kept.
- **Scaffold with a released ge** (a package's `bin/ge`). `VERSION` on `main` is the last
  release, so a ge built from `main` pins that release, which lacks whatever `main` added since:
  its scaffold downloads and verifies, then fails to configure. Against this tree, pass
  `--engine=<this tree>` to `ge build`, `ge test`, `ge check` or `ge lab`.
- **Try it here:** `make ge ARGS="new ../snow-racer"`, then in that repo `<this tree>/build/debug/ge
  use skier`, `ge add system wind` and `ge check --engine=<this tree>`. Packaging's `scaffold`
  job does exactly that on every PR.

## Consumer skills (`consumer-skills/`)

The skills a game repo needs, written for a game repo (its paths, its `make` targets, the
engine as a dependency), never for this one: `ge` (the command line and its loop), `play-test` (the play contract,
`ge play` and its siblings), `multiplayer` (`ge::net`, the simulated network and `ge net`),
`start-work`, `write-code`, `engine` (the pin, determinism, upgrading, reporting an engine
bug), `sim-system`, `labs`, `skill-reflection` and `commit`, plus one per plugin (`racing`,
`mountain`, `skier`) with `plugin:` front matter.
Each is a `SKILL.md` alone: lessons accumulate in each game
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
