---
name: port
description: "Use when porting any game5 or oss-game-framework TypeScript module to C++ in gameengine2. The bottom-up procedure: find the oracle file, map layout and types, write the golden generator, port, prove bit parity, record the milestone."
---

# Port a module

Read `docs/porting.md` first. It holds the layout map, the type map and the 14
JavaScript traps. This skill is the procedure.

## Procedure

1. **Find the oracle file** in `../game5` or `../oss-game-framework` and list its
   imports. Every import must already be ported with parity. If one is not, port it
   first (bottom-up).
2. **Read the oracle's tests** (`../game5/tests/<topic>_test.ts`). They say which
   behaviours matter and which inputs are interesting.
3. **Place the port**: the path that mirrors the oracle (`docs/porting.md` § Layout
   mapping). Its first comment names the oracle file.
4. **Write the golden generator** in `tools/parity/golden-<module>.mjs`. It imports the
   oracle `.ts` directly (Node `--experimental-strip-types`) and writes
   `tests/golden/<module>.txt` as hex words. Cover the edges: zeros, signed zeros,
   NaN and ∞ where the inputs allow them, huge and tiny magnitudes, and every branch.
   If the module reaches a transcendental, run it under game5's pinned Node and put
   the version in the file's first line. Add the script to `make golden`.
5. **Write the C++ test** `tests/<module>_test.cpp`: read the golden rows and
   `CHECK_BITS` every output.
6. **Port**: keep the oracle's structure and statement order. Optimise only after
   parity, and keep parity while you optimise (`perf` skill). Data goes into POD
   structs, closures into a struct plus free functions, and maps into
   insertion-ordered storage.
7. **Run** `make test` on both presets. Every row must be bit-exact.
8. **Update** `AGENTS.md`'s "Where things live" row and the milestone Status in
   `docs/goals.md`.

## Rules

- Port the oracle's behaviour, including its bugs, with a comment. Never change
  game5 itself.
- A port that "simplifies" the math (`x*x` for `x**2`, `sqrt(a*a+b*b)` for `hypot`)
  breaks parity. Every simplification needs a golden test that shows it holds.
- The oracle's words for the game (skier, piste, gate) stay in `games/fall_line/`.
  `engine/` names the mechanism (vehicle, checkpoint, track).
