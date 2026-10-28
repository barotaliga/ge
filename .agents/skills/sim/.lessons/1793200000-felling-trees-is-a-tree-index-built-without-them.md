---
title: A tree taken off the map is an index built without it, never a copy of the level
date: 2026-10-28
scope: games/fall_line/sim/village.cpp
concepts: [trees, tree-index, felled, village, level, digest, free-ride]
---

The village fells hundreds of trees, but the `Level` is shared and immutable, and every system
that meets a trunk (the skier, the snowmobile, the helicopter, the crowd, the army, a thrown
body) asks the run's `TreeIndex` rather than walking `level.trees`. So felling is
`build_tree_index(level, felled)` once at create, after the machines are placed: no copy of the
map, no map digest moved, and every system sees the trees gone at once. Only a run whose bodies
pass where a felled tree stood moves (the scripted hunt on seed 38's soldiers did), and the
drawing, which reads `level.trees` whole, still has to be told which are gone.
