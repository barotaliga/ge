---
name: write-code
description: "Use on every code change in gameengine2. How C++ is written here so the next agent can find and change it: the clean-slate rule (no legacy references), file shape and naming, the ge: discovery tags and docs/index.md, data-first design, comments, the edit loop, the 1000-line cap, and the structure check."
---

# Write code

The reader is always an agent with no context. Write so that a grep finds the answer
and the file explains itself.

## A clean slate: no legacy references

The engine and its games stand on their own. Code that was carried over from a reference
source is written, commented and tagged as if it had been written here first:

- **Never** name, outside `porting/`, the reference repositories or kit, the reference
  language or its files and identifiers, the reference as an authority, or a file's
  provenance; never justify a choice by what a reference runtime does. This holds for
  comments, `ge:` tags, docs, skills, lessons, the Makefile, CMake, CI, commit messages and
  PR titles. `make agents-check` fails on the words (`tools/agents-index.mjs`, `LEGACY`).
- **Say what the code does and why, in the engine's terms.** Not "rounds like the
  reference's rounding" but "rounds halves toward +inf (round(-2.5) == -2)". Not "the
  reference's message text" but "the finding's text is part of the golden". Not "matches
  the reference bit for bit" but "held bit-exact by `tests/golden/<name>.txt`".
- **The link to the reference goes in `porting/map.md`**: one row per engine file, sorted
  by path, with the reference module and the reference identifiers it covers. `make find`
  searches it, so a reference name still finds the engine file while porting lasts. Add the
  row in the PR that adds the file; move it when the file moves.
- **A porting note goes in `porting/`**: a semantics trap of the reference language, why a statement order is kept,
  a difference measured against the reference. If the code needs the reason to be safe to
  change, state the reason as a fact about the engine (an invariant, a golden that holds it).

## File shape

- **One subject per file.** The first comment says what the file owns and where its test
  is, then ends with the file's `ge:` tag block (§ Discovery tags).
- **Under 1000 lines.** `tools/check-agents.sh` fails a longer file unless it carries
  `agents:allow-large-file: <reason>`. Split by subject, not by line count.
- **Headers declare the whole API with comments.** Small hot math is `inline` in the
  header. Everything else is declared in `.hpp` and defined in `.cpp`.
- **Names**: `snake_case` for files, functions and variables; `PascalCase` for types;
  `kConstant`. Units go in names or comments (`speed_mps`, `cell_m`, `dt_s`).
- **Namespaces**: `ge::<dir>` for the engine (`ge::core`), `ge::<plugin>::<dir>` for a
  plugin (`ge::mountain::mapgen`), `fall_line::<dir>` for the demo game.
- **One name, four places.** A module `engine/<dir>/<name>.hpp` (or
  `plugins/<plugin>/<dir>/<name>.hpp`, `games/fall_line/sim/<dir>/<name>.hpp`, by the
  `plugins` skill's test) has its test `tests/<dir>_<name>_test.cpp` and its golden table
  `tests/golden/<dir>_<name>.txt` (a plugin's: `tests/<plugin>_<name>…`). Knowing one name
  gives the others.

## Discovery tags

Agents find code by grep. Every source file (`engine/`, `plugins/`, `games/`, `tests/`, `tools/`) ends its
leading comment with a block of `ge:` lines, which `make index` turns into
`docs/index.md` (generated in each checkout, never committed) and `make agents-check` holds:

```cpp
// THE SEEDED STREAM. Every random draw in a simulation ... (the prose header)
//
// ge:summary The seeded stream (mulberry32): every random draw in a simulation
// ge:keywords prng rng random seed mulberry32 next range chance pick pick_index create_rng
// ge:skill determinism
// ge:test tests/core_prng_test.cpp
#pragma once
```

| Tag | Required | What |
| --- | --- | --- |
| `ge:summary` | yes | One line, under about 100 characters: what the file owns. It is the file's row in the index. |
| `ge:keywords` | yes | Space-separated, lower case: CONCEPTS in kebab case (`value-noise`, `fixed-step`) and the file's own identifiers (`sample_field`). Include the synonyms an agent would type (`rng random prng`). A capital fails the check: a reference identifier goes in `porting/map.md`. |
| `ge:skill` | yes | The skill(s) that own the subject, space-separated; each must exist. A file in `plugins/<name>/` names `<name>` first (`tools/check-plugins.mjs`). |
| `ge:test` | when one exists | The test file(s) that hold it; each must exist. |

- Use `#` instead of `//` in shell scripts. Tags must sit in the first 60 lines.
- A tag line stays within 100 columns (clang-format reflows a longer comment, and the wrapped
  half loses its `ge:` prefix). Wrap by repeating the tag: repeated tags' values are joined.
- Keywords name what the file HAS, not what it skipped.
- `docs/index.md` is gitignored: `make index` (or `make agents-check`) rewrites it in your
  checkout after a tag changes, and nothing commits it.
- A `.cpp` beside its `.hpp` carries its own block; its summary says what the definitions add
  (the module state, the codec) rather than repeating the header's.

To search: `make find Q="word word"` lists every file, skill, lesson and porting-map row matching any word
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
- **Only what the game needs.** No speculative parameters, options or abstractions.
- **Errors are prompts.** A message names what failed, where, and the next action
  (`docs/architecture.md` § Diagnostics).

## Comments

They explain WHY and the contract, not what the next line does, in the engine's own terms
(§ A clean slate). When a carried-over module had a reasoning comment that explains a
non-obvious choice, restate the reason as a fact about this code. Delete any comment the
code now makes obvious.

## The edit loop

1. Write the test first when the behaviour is defined by a golden table (`testing` skill).
2. Make the change.
3. Build and run only the affected test executables:
   `cmake --build build/debug && ./build/debug/tests/<topic>_test`.
4. Before committing: `make fmt`, `make test`, `make agents-check` (the
   `commit` skill).

`make lint` (clang-tidy) runs in CI. Run it locally when you touch many files.
