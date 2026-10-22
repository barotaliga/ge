---
name: write-code
description: "Use on every code change in a game repo built on the engine: file shape and naming, the ge: discovery tags, data-first design, diagnostics written as prompts for an agent, comments, the 1000-line cap, and the edit loop."
---

# Write code

The reader is always an agent with no context. Write so that a grep finds the answer and the
file explains itself. `ge doctor` (part of `ge check`) holds every rule here that a program can.

## File shape

- **One subject per file.** The leading comment says what the file owns and why, then ends with
  the file's `ge:` tag block.
- **Under 1000 lines.** Split by subject, not by line count. An honest exception carries an
  `agents:allow-large-file: <reason>` comment.
- **Headers declare the API with comments**; small hot math is `inline` there, the rest is in
  the `.cpp`.
- **Names**: `snake_case` files, functions and variables; `PascalCase` types; `kConstant`.
  Units in names or comments (`speed_mps`, `dt_s`, `cell_m`).
- **One name, everywhere**: `sim/<name>.hpp` is tested by `tests/sim_<name>_test.cpp`.

## Discovery tags

Every `.cpp`, `.hpp` and `.h` ends its leading comment with:

```cpp
// ge:summary One line: what the file owns (it is the file's answer to a search)
// ge:keywords lower-case concepts and the file's identifiers: rng random seed create_game
// ge:skill sim-system
// ge:test tests/sim_game_test.cpp
```

`ge:summary`, `ge:keywords` and `ge:skill` are required, inside the first 60 lines; keywords are
lower-case kebab or snake case; every skill and test named must exist. Wrap a long tag by
repeating it. When a search misses the file that had the answer, add the word you searched for
to its keywords (skill: `skill-reflection`, question 6).

## Design

- **Data first.** Plain structs, explicit arrays, free functions. No inheritance or virtuals on
  hot paths, no singletons, no hidden globals.
- **No allocation in a step or a frame.** Reserve at create time.
- **State each answer once.** Grep before writing a helper; the engine may already have it
  (skill: `engine`).
- **Only what the game needs.** No speculative options or abstractions.

## Diagnostics

Every error and warning is a prompt for an agent, in three parts:

1. **What** failed, in one line, with the values involved.
2. **Where**: the file and line, and the game's context (seed, step, system).
3. **Next**: the command to run, the skill to load or the doc to read.

```cpp
ge::core::output::error("sim: seed 38 step 4410: the body left the field at x = 212.4 m; "
                        "next: ge lab run -- --seeds=38, skill: sim-system");
```

Print through `core/output.hpp` (`status`, `info`, `warn`, `error`), never straight to
stdout. `ge doctor` reads every `output::error`/`output::warn`, every `GE_ASSERT`, every
lab's `fail` and every CMake `message(FATAL_ERROR|WARNING ...)` and fails a message that has
fewer than three words or no next-action marker (`next:`, `skill:`, `make `, `run `, `rerun`,
`see `, `docs/`).

## Comments

They explain WHY and the contract, not what the next line does. Delete a comment the code makes
obvious.

## The edit loop

1. Write the test first when the behaviour is defined (skill: `sim-system`).
2. Make the change.
3. Build and run only what it touches: `ge test --filter=<x>` (failures come back as findings;
   the full log is under `build/debug/`).
4. Before committing: the `commit` skill's gates.
