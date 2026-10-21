---
title: A texture's mip chain halves rounding down; a CPU pyramid uploaded into one must too
date: 2026-10-21
scope: engine/render/terrain.cpp
concepts: [pyramid, mips, min-max, texture, validation]
---

A 1500-wide min/max pyramid built by halving rounding UP has 12 levels, but a 1500 texture
allows only floor(log2(1500)) + 1 = 11 mips: Dawn refuses the texture and every bind group and
command buffer that touches it fails after it (one root error, dozens of follow-on lines, so read
the FIRST `gpu:` line). `min_max_pyramid` halves as the texture does (rounding down) and lets the
last texel of an odd row or column take the one left over, and the cull clamps its texel index
to the mip's size, so the far edge is still covered. `tests/render_terrain_test.cpp` holds the
chain's sizes and the bounds.
