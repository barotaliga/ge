---
title: The invariants hold while the snow still steps where runs meet
date: 2026-10-21
scope: plugins/mountain/mapgen/network_stamp.cpp
concepts: [junctions, stamp-run, packed, lab, invariant, merge]
---

Every invariant of `tests/mountain_resort_build_test.cpp` passes on the finished ski area, yet
the `junctions` lab finds one to ten steps a seed in the groomed snow: jagged walls one or two
cells wide where a run's corridor meets a parallel run's, and terraces where a run is graded
cell by cell under another. "Packed along its line" and "falls from start to end" say nothing
about the pitch a skier meets crossing from one run to the next. Before calling a change to the
stamp or the grading done, run `make lab LAB=junctions ARGS="--seeds=1,2,4,7,38"` before and
after and read the sheet, not only the counts.
