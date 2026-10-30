---
title: A test's note on a field the loop mirrors is overwritten
date: 2026-10-30
scope: games/shooters_edge/sim/qc_program.cpp, games/shooters_edge/server/test_mod.cpp
concepts: [mirror, fields, test-program, worldspawn, campaign]
---

The campaign stub's worldspawn stored the `registered` setting in `world.health` for the test to
read, and the test read 0. `health` is one of the fields the loop owns: it is mirrored from the
loop's entity into the VM on every enter, so the program's write lasted only until the next
call. A program that leaves a value for a test or a lab must put it in a field the mirror does
not own. `grep -o "q.layout->f\.[a-z_0-9]*" games/shooters_edge/sim/qc_program.cpp` lists the
owned fields (`max_health`, `armorvalue`, `items` and the ammo are free).

A program compiled from QC by `compile_qc` must also carry its form (`Progs::layout`), or
everything that reads the layout falls back to the multiplayer table and reads the wrong
offsets. The compiler now stamps the form it matched; check `progs.layout` when a compiled
single-player program seems to have lost its classnames.
