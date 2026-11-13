---
title: Carry QC to C++ side by side with the compiled QC, then pin the QC's numbers before deleting it
date: 2026-11-13
scope: games/shooters_edge/rules/, plugins/progs/native.hpp, tests/shooters_edge_arena_rules_test.cpp
concepts: [native-program, native-arena, parity, side-by-side, loop-digest, pin, progs-natives]
---

Run each match both ways (`run_mod_match` with `compiled` and with `native`) and compare every
frame's loop digest, not just the frags. The traps are QC semantics, not logic: `&&` and `||`
evaluate both sides (a call on the right runs and is counted even when the left decides), `-0` is
true in an `if`, `!x` compares as a float, and the QC's own quirks stay (the pack's rank test is
reversed relative to the weapon pickup's). Builtin counts differ only where the QC needed a text call to read a number (strlen,
substr): exclude those by number. Once it matched, the test's numbers were taken FROM THE QC RUN
(frames, frag lines, prints, an FNV over every loop digest) and pinned, so the parity outlives the
QC's deletion. To switch every caller at once, the progs carries its C++ (`Progs::natives`, bound
by `create_vm`): no server option to thread through. Lab digests that fold a think's function
number or a string's offset move with the layout; the play does not, and the pin says so.
