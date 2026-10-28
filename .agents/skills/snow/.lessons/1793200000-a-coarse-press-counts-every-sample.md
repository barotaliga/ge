---
title: A coarse press must count every sample it covers; a probe grid misses a boot
date: 2026-10-28
scope: engine/snow/lod.cpp
concepts: [coarse, press-coarse, share, outline, lod, fold, refine, tile-size, digest]
---

The first `press_coarse` estimated each coarse cell's covered share from a 4×4 grid of probes. A
boot (about 30×10 cm) fell between the probes and left no mark at all. Count the share over every
sample of the cell the outline covers, and read the outline at least three quarters of a coarse
cell wide, as the fine press does for a sample. Also: a field added to `Tile` grows every tile by
its size, which moves the memory metric of `snow-field` and with it the lab's digest, though no
snow moved. Say so in the PR, or keep per-tile bookkeeping in a parallel vector.
