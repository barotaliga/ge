---
name: port
description: "WORKING skill, deleted with porting/ when porting ends. Use when porting any game5 or oss-game-framework TypeScript module to C++ in gameengine2. The bottom-up procedure: find the oracle file, map layout and types, write the golden generator, port, prove it correct (bit parity optional), record the milestone."
---

# Port a module

A WORKING skill: like `porting/`, it is deleted when porting ends, and it is the only skill
that may name the reference sources (AGENTS.md § A clean slate). Read `porting/README.md`
first. It holds the layout map, the type map and the 14 JavaScript traps. This skill is the
procedure.

**Bit parity with the oracle is OPTIONAL**: a port may improve on the
oracle (structure, speed, behaviour) where that serves the engine; when it diverges, its
goldens are rewritten from the engine's own reviewed output and the PR says why. Use the
oracle's goldens when matching them is the fastest way to a correct module, not as a rule.

**Everything the port leaves outside `porting/` reads as engine code written here first**:
no reference names in the file, its comments, its tags, its test or its commit (the
`write-code` skill, § A clean slate).

## Procedure

1. **Find the oracle file** in `../game5` or `../oss-game-framework` and list its
   imports. `porting/map.md` says which oracle files are ported already. Every import must already be ported with parity. If one is not, port it
   first (bottom-up).
2. **Read the oracle's tests** (`../game5/tests/<topic>_test.ts`). They say which
   behaviours matter and which inputs are interesting.
3. **Place the port** where the ENGINE's design puts it (the `plugins` skill and
   `docs/architecture.md`), not where the oracle had it: no oracle architecture carries over
   by default (AGENTS.md § C++ only, and no inherited architecture). `porting/README.md` §
   Layout mapping is a starting hint, never a rule. Its first comment says what the file owns in the engine's terms and ends
   with the `ge:` tag block (the `write-code` skill, § Discovery tags). Add its row to
   `porting/map.md`: the oracle path and the oracle's exported identifiers as TypeScript
   spells them, so `make find` with an oracle name lands on the port.
4. **Write the golden generator** in `porting/parity/golden-<module>.mjs`. It imports the
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
   `docs/goals.md`, and run `make index`.

## Rules

- Port the oracle's behaviour, including its bugs. The engine-side comment states the
  behaviour as a fact ("a cliff past the last row is still counted"); that it is the
  oracle's bug goes in `porting/` (a note in `porting/README.md` or the map row). Never
  change game5 itself.
- A port that "simplifies" the math (`x*x` for `x**2`, `sqrt(a*a+b*b)` for `hypot`)
  breaks parity. Every simplification needs a golden test that shows it holds.
- Place the port in its home first (the `plugins` skill): `engine/` names the mechanism
  (vehicle, checkpoint, track), a plugin may use its world's words (skier, piste, gate),
  and only `games/fall_line/` uses Fall Line's own (its modes, strings, pinned maps).
