---
type: Added
title: Snow level of detail and a memory budget
---

A snow layer keeps fine tiles only near the actors the game names (`set_focus`, `engine/snow/lod.hpp`). Imprints far from all of them land in a coarse layer (one cell per 1.6 m). `fold_stale` folds the tiles nobody came near into it, and a tile allocated again is refined from it, worn, with the mass kept to the unit both ways. Past `LayerSpec::max_tiles`, the stalest tile is folded instead of the press being refused. Snapshots are `GESNOW2`, which carries the coarse layer; `GESNOW1` files still load. New labs: `snow-lod` (a 768 m run in 2 MB, against 79 MB if every tile were kept fine) and `snow-cost`.
