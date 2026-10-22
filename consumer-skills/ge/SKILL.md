---
name: ge
description: "Use whenever you build, test, run, grow or check this game: ge is the one command line for all of it (find, explain, new, use, add, build, test, check, lab, doctor, skills, lessons). Its commands, the --json answer every one gives, where each piece it writes goes, and the loop to work in."
---

# ge, the game's command line

Everything this repo does goes through `ge`: there is no editor and nothing to click. Each
command answers in one shape, and every problem it reports says what failed, where, and the
next action. `ge help` lists the commands; `ge help --json` is the whole catalog (every
command's arguments, flags, an example and its skill), generated from the table the parser
uses, so it is always true for the `ge` you run.

**Which ge.** The pinned release builds its own `ge` into `build/<preset>/ge`. Before the first
build, use a `ge` from a release package (or `make ge-build`); a `ge` of another release run in
this repo hands the command to the pinned one once it is built, so the rules you are held to
are always the pinned release's.

## The loop

```sh
ge find <words>                  # what exists: this repo's and the engine's files, skills, lessons
ge explain <file|name>           # one of them: its header comment, declaration, skill, test
ge add system <name>             # sim/<name>.hpp, .cpp and its replay test, to start from
ge add lab <name> --kind=<k>     # a lab (sim, scenario, picture, bench), in the smoke pass
ge add test <name>               # a test file on the harness
ge use <plugin>                  # link racing, mountain or skier, and install its skill
ge build                         # compiler and CMake errors come back as findings
ge test [--filter=<regex>]       # every test and lab smoke; each failure a finding
ge lab <id> [--tag=before | --compare=before] [-- <its flags>]   # measure and LOOK
ge check                         # the whole gate: format, doctor, build, tests (what CI runs)
```

Around it: `ge new <dir>` scaffolds a game repo; `ge doctor` is the rules alone; `ge skills
sync` rewrites the engine's skills after an upgrade; `ge lessons <skill>` reads a skill's
lessons before you start.

Build, test and check write the tools' full output to `build/<preset>/ge-*.log` and answer
with findings instead, so a clean run is one line. Read the log only when a finding says to.

## The answer

Every command takes `--json` and then prints exactly one object:

```json
{"command": "ge test", "ok": false, "exit": 1,
 "findings": [{"severity": "error", "what": "level 4.1 after 20 s, want inside (-1, 1)",
               "where": "tests/sim_wind_test.cpp:36", "next": "fix the code or the test at ..."}],
 "written": [], "result": {"preset": "debug", "total": 4, "failed": 1, "log": "..."},
 "summary": "...", "next": ""}
```

- **`exit`**: 0 done, 1 the work found a problem (some finding is an error), 2 a usage error
  (an unknown flag, a missing argument: `ge help <command>` says what it takes).
- **`findings`**: act on each error's `next`. A `note` is information, a `warn` worth a look.
- **`written`**: every file the command changed, so you know what to review and commit.
- **`result`**: the command's own answer (a lab's whole `report.json`, the files `ge find`
  ranked, the catalog for `ge help`).
- **`next`**: on success, the command most likely wanted after this one.

## Rules

- **Use the command, not the tool under it.** `ge test`, not `ctest`; `ge lab`, not the
  `labs` executable; `ge use`, not an edit to `target_link_libraries`. The command knows the
  repo's layout and turns failures into findings.
- **Start from `ge add`.** A template is a small working example with its `ge:` tags filled,
  so the repo builds and passes `ge check` the moment it lands. Rewrite its summary, keywords
  and question for what you are making.
- **`ge check` is the gate.** Green before the first edit (skill: `start-work`) and before
  the commit (skill: `commit`).
- **A search that misses is a tag to add.** When `ge find` does not find what you were looking
  for, add the word you tried to the right file's `ge:keywords` (skill: `skill-reflection`).
