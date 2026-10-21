---
name: testing
description: "Use when writing or changing a test, a golden table, or a parity script in gameengine2. The zero-dependency harness (tests/check.hpp), one executable per topic, golden files from the TS oracle, and CTest."
---

# Testing

## The harness

`tests/check.hpp` is the whole framework. `TEST(name) { … }` registers a case.
`CHECK(cond)` records a failure and carries on. `CHECK_BITS(got, want)` compares two
doubles by bit pattern and prints both values in hex. `tests/main.cpp` runs every case
and prints `ok` or `FAIL` per test.

- One file per topic: `tests/<topic>_test.cpp`, with a `ge:` tag block like every source
  file (the `write-code` skill, § Discovery tags); the module it tests names it in `ge:test`. `tests/CMakeLists.txt` globs
  `*_test.cpp`, and each file becomes its own executable and CTest case, so nothing is
  registered by hand.
- `ge::test::root_path("tests/golden/x.txt")` opens files relative to the repo.
- `tests/golden.hpp`: `golden_rows(path)` returns rows of `uint64_t` hex words, and
  `as_double(u)` reinterprets them. A codec's strings (the tape's base64) go in a text file
  read with `golden_words(path)`, with arbitrary text hex-encoded (`unhex`).

## Golden files

- Written ONLY by `tools/parity/*.mjs` from the TypeScript oracle (`make golden`),
  never by hand and never by the C++ under test.
- Format: one row per line, whitespace-separated hex (`%016llx` doubles, plain hex
  integers). Inputs come first, then outputs. A leading `#` line may carry
  provenance (the Node version). Booleans are `0`/`1` doubles; an optional is the pair
  `has value` (`0 0` when absent). A module with no numbers (`core/output`) gets a text
  transcript the C++ test reproduces by running the same script.
- Generators load the framework's `.ts` sources through `tools/parity/oracle-import.mjs`
  (it resolves the extensionless relative imports). Anything that reaches a
  transcendental goes in a script that calls `requireOracleNode` (`oracle-node.mjs`) and
  runs under `make golden-libm`.
- Committed. Regenerating them is a reviewed change: say in the PR why the oracle
  moved.
- Keep a file under about 1 MB. Draw inputs from a fixed mulberry32 seed so the table
  is reproducible.

## What a test owes

- A port: bit parity on a golden table covering every branch.
- A behaviour the oracle tests (`../game5/tests/`): the same assertion, ported.
- A bug fix: the failing case first.

## Running

```sh
make test                                   # everything, debug + sanitizers
ctest --preset debug -R core_math           # one topic
./build/debug/tests/core_math_test          # direct, with full output
```

Slow tests (a full level build, a simulated run) are fine, but keep each executable
under about 30 s in debug so the CI matrix stays quick.
