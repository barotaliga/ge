---
name: write-code
description: "Use on every code change in gameengine2. How C++ is written here so the next agent can find and change it: file shape and naming, data-first design, comments, the edit loop, the 1000-line cap, and the structure check."
---

# Write code

The reader is always an agent with no context. Write so that a grep finds the answer
and the file explains itself.

## File shape

- **One subject per file.** The first comment says what the file owns, which oracle
  file it ports (if any), and where its test is.
- **Under 1000 lines.** `tools/check-agents.sh` fails a longer file unless it carries
  `agents:allow-large-file: <reason>`. Split by subject, not by line count.
- **Headers declare the whole API with comments.** Small hot math is `inline` in the
  header. Everything else is declared in `.hpp` and defined in `.cpp`.
- **Names**: `snake_case` for files, functions and variables; `PascalCase` for types;
  `kConstant`. Mirror the oracle's names (`createGame` becomes `create_game`). Units go
  in names or comments (`speed_mps`, `cell_m`, `dt_s`).
- **Namespaces**: `ge::<dir>` for the engine (`ge::core`), `fall_line::<dir>` for the
  demo game.

## Design

- **Data first.** POD structs, explicit arrays, free functions. No inheritance, no
  virtuals on hot paths, no singletons and no hidden globals. Module state, when
  there must be some, sits in one `.cpp` and its header says so.
- **No allocation in a step or a frame.** Reserve at create time.
- **State each answer once.** Before you write a helper, grep for one. If the same
  rule would live in two places, it belongs in one function that both call.
  `AGENTS.md` § Stated once lists the big ones.
- **Only what game5 needs.** No speculative parameters, options or abstractions.
- **Errors are prompts.** A message names what failed, where, and the next action
  (`docs/architecture.md` § Diagnostics).

## Comments

They explain WHY and the contract, not what the next line does. Port the oracle's
reasoning comments in your own words when they explain a non-obvious choice. Delete
any comment the code now makes obvious.

## The edit loop

1. Write the test first when the behaviour is defined by the oracle (`testing` skill).
2. Make the change.
3. Build and run only the affected test executables:
   `cmake --build build/debug && ./build/debug/tests/<topic>_test`.
4. Before committing: `make fmt`, `make test`, `make agents-check` (the `commit` skill).

`make lint` (clang-tidy) runs in CI. Run it locally when you touch many files.
