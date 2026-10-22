---
title: A ring walk over a sparse grid should visit only the part of each ring inside the occupied box
date: 2026-10-22
scope: plugins/mountain/mapgen/query.cpp, plugins/mountain/mapgen/net_index.hpp
concepts: [spatial-hash, ring-walk, nearest, mapgen, perf]
---

`nearest_within` and `NetIndex::nearest` walk rings of 24 m cells outward until nothing further
could be nearer, up to 90 rings when nothing is close. Most of those cells are empty, and each
was a hash-map miss or an out-of-line call. Keeping the box every segment lies in and clamping
each ring's columns and rows to it visits the occupied cells in exactly the old order (so the
tie rule, the first found, is untouched) and skips the rest: a v10 build went from 2.16 s to
1.54 s with every digest the same. Any new spatial index here should keep its occupied box too.
