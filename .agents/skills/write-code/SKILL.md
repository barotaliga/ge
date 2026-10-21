---
name: write-code
description: "Use on every code change in gameengine2. How C++ is written here so the next agent can find and change it: file shape and naming, the ge: discovery tags and docs/index.md, data-first design, comments, the edit loop, the 1000-line cap, and the structure check."
---

# Write code

The reader is always an agent with no context. Write so that a grep finds the answer
and the file explains itself.

## File shape

- **One subject per file.** The first comment says what the file owns, which oracle
  file it ports (if any), and where its test is, then ends with the file's `ge:` tag
  block (§ Discovery tags).
- **Under 1000 lines.** `tools/check-agents.sh` fails a longer file unless it carries
  `agents:allow-large-file: <reason>`. Split by subject, not by line count.
- **Headers declare the whole API with comments.** Small hot math is `inline` in the
  header. Everything else is declared in `.hpp` and defined in `.cpp`.
- **Names**: `snake_case` for files, functions and variables; `PascalCase` for types;
  `kConstant`. Mirror the oracle's names (`createGame` becomes `create_game`). Units go
  in names or comments (`speed_mps`, `cell_m`, `dt_s`).
- **Namespaces**: `ge::<dir>` for the engine (`ge::core`), `ge::<plugin>::<dir>` for a
  plugin (`ge::mountain::mapgen`), `fall_line::<dir>` for the demo game.
- **Paths mirror the oracle.** A port of `<oracle>/src/<dir>/<name>.ts` is
  `engine/<dir>/<name>.hpp` (a game5 `engine/<dir>/<name>.ts` is
  `plugins/<plugin>/<dir>/<name>.hpp` or `games/fall_line/sim/<dir>/<name>.hpp`, by the
  `plugins` skill's test), with kebab-case turned to snake_case
  (`run-clock.ts` is `run_clock.hpp`). Its test is `tests/<dir>_<name>_test.cpp`, its
  golden table `tests/golden/<dir>_<name>.txt`, and the script that writes it
  `tools/parity/golden-<dir>.mjs`. Knowing one of the five names gives the other four.

## Discovery tags

Agents find code by grep. Every source file (`engine/`, `plugins/`, `games/`, `tests/`, `tools/`) ends its
leading comment with a block of `ge:` lines, which `make index` turns into
[`docs/index.md`](../../../docs/index.md) and `make agents-check` holds:

```cpp
// THE SEEDED STREAM. Port of oss-game-framework `src/core/prng.ts` ... (the prose header)
//
// ge:summary The seeded stream (mulberry32): every random draw in a simulation
// ge:keywords prng rng random seed mulberry32 next range chance pick pickIndex createRng
// ge:oracle oss-game-framework/src/core/prng.ts
// ge:skill determinism
// ge:test tests/core_prng_test.cpp
#pragma once
```

| Tag | Required | What |
| --- | --- | --- |
| `ge:summary` | yes | One line, under about 100 characters: what the file owns. It is the file's row in the index. |
| `ge:keywords` | yes | Space-separated. Lower-case kebab CONCEPTS (`value-noise`, `fixed-step`) and the oracle's IDENTIFIERS as TypeScript spells them (`sampleField`, `MAX_FRAME_SECONDS`), so a grep for the oracle's name lands on the port. Include the synonyms an agent would type (`rng random prng`). |
| `ge:skill` | yes | The skill(s) that own the subject, space-separated; each must exist. A file in `plugins/<name>/` names `<name>` first (`tools/check-plugins.mjs`). |
| `ge:oracle` | ports | `game5/<path>` or `oss-game-framework/<path>`: the file it ports. Checked against `../<repo>` when cloned there. |
| `ge:test` | when one exists | The test file(s) that hold it; each must exist. |

- Use `#` instead of `//` in shell scripts. Tags must sit in the first 60 lines.
- A tag line stays within 100 columns (clang-format reflows a longer comment, and the wrapped
  half loses its `ge:` prefix). Wrap by repeating the tag: repeated tags' values are joined.
- Keywords name what the file HAS, not what it skipped: a symbol left unported is not a keyword.
- After adding a file or changing a tag, run `make index` and commit `docs/index.md`. On a
  merge conflict in `docs/index.md`, take either side and run `make index` again.
- A `.cpp` beside its `.hpp` carries its own block; its summary says what the definitions add
  (the module state, the codec) rather than repeating the header's.

To search: `make find Q="word word"` lists every file, skill and lesson matching any word
(case-insensitive, substring). `grep -rn "ge:keywords.*<word>" engine games` does the same for
files alone. When a search misses, add the word you tried to the right file's `ge:keywords`
(the `skill-reflection` skill, question 6).

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
4. Before committing: `make index`, `make fmt`, `make test`, `make agents-check` (the
   `commit` skill).

`make lint` (clang-tidy) runs in CI. Run it locally when you touch many files.
