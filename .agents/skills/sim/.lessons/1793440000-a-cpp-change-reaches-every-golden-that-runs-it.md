---
title: A change inside a sim .cpp moves goldens precheck does not run
date: 2026-10-29
scope: games/fall_line/sim/village.cpp
concepts: [precheck, affected, golden, tree-index, felled, army-steal]
---

The buildings' placement (M13l) felled the trees under each footprint, which changed the free
ride's tree index and so the state digest every 5 s of `fall_line_army_steal_test`'s helicopter
ram, which flies over the village. Precheck stayed green, because `ge test --affected` follows the
include graph: a change inside `village.cpp` that edits no header reaches only the tests that
include its header, not every test that links `fall_line_sim` and runs it. Main went red on the
next full suite. When a sim change alters what the world holds (trees, walls, bodies), run the
whole `fall_line_` suite in release (`ctest -R fall_line_ -j 8`, about a minute) before the push.
