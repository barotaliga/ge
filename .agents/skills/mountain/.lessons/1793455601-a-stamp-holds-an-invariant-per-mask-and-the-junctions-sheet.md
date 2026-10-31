---
title: A run's stamp holds an invariant for each mask it writes, and is done only when the junctions sheet reads clean
date: 2026-10-21
scope: plugins/mountain/mapgen/network_stamp.cpp
concepts: [stamp-run, junction, junctions, corridor, packed, invariant, merge, lab]
---

A run that merges into another stops pressing past the plane through its last station. Applied to
every segment, that plane also cut away an earlier stretch's corridor wherever the run traversed
back across the fall line, and left it unpacked; the cut now applies only within two corridor
reaches of the end. The test's invariant ("packed along its line but in its drifts") found it, so
give every mask a stamp writes an invariant like it.

The invariants still pass while the snow steps where runs meet: jagged walls a cell or two wide
where corridors meet, terraces where one run is graded cell by cell under another. Before calling a
stamp or grading change done, run `make lab LAB=junctions ARGS="--seeds=1,2,4,7,38"` before and
after and read the sheet, not only the counts.
