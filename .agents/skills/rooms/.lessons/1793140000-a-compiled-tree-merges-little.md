---
title: A compiled map's tree is already pruned; pairwise brush merging buys 6 to 10 percent
date: 2026-10-24
scope: plugins/rooms/brush_merge.cpp, plugins/rooms/clone.cpp
concepts: [clone, merge, brush, tree, subtree, convex, union, dent]
---

Collapsing a subtree whose leaves all hold one contents into one cell changed nothing on any
shareware map: the compiler already prunes such subtrees. Neighbouring solid cells are cut by
planes that extend faces elsewhere, so most pairs have a dent where they meet and only 6 to 10
percent merge pairwise. Two traps on the way: projections solved from different triangles of
one surface differ in their last bits, so compare them to a tolerance before calling two
coplanar faces the same; and a test that writes `-0` into the text form reads back a float that
prints as `0`, so its bytes no longer round-trip (write `0.0 - x`, not `-x`).
