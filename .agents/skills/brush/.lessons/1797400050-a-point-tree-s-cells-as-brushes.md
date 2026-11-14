---
title: A version 29 map is traced by the brush trace once its point tree's cells are brushes
date: 2026-11-14
scope: plugins/brush/cells.hpp, plugins/brush/cells.cpp, games/shooters_edge/client/remix.cpp
concepts: [cells, brushes, point-tree, bevel, brush-trace, classic-remix, solid-leaf]
---

A version 29 file sends all its solid space to ONE shared leaf, so a brush a leaf does not work:
walk each model's tree and make a brush a PATH (the path's planes faced so the cell lies behind
each), and point that node's child at a new leaf holding it. Faces cut in doubles from a big quad
on each plane drop the planes that bound nothing; axial bevels from the faces' box keep a swept box
from overreaching at edges. Every leaf's contents must become the 38 bits (0 for empty): the
classic numbers are negative and `-1 & mask` is never 0. Compare against the map's own clip hull
(hull 1) in a test, not against the 38 fixture, whose doorways differ.
