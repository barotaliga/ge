---
title: A line takes no depth bias, so push a wireframe's fragment depth toward the eye
date: 2026-10-22
scope: engine/render/wgsl/terrain_draw.wgsl, engine/render/instances.wgsl
concepts: [wireframe, wire-probe, depth-bias, line-list, frag-depth, reversed-z, hidden-line]
---

WebGPU refuses a non-zero depth bias on a line or point topology, so a wireframe drawn over the
frame's own depth flickers in and out of the triangles it outlines. Write `frag_depth` as the
fragment's depth times a little over 1 (reversed-z: larger is nearer; 1.002 is about 2 m at a
kilometre) and test `greater-equal` against the read-only depth: the ground still hides the
lines behind a ridge. Reuse the draws' own vertex stage and copy only the instance counts from
the cull's indirect arguments into the lines' (CopyBufferToBuffer, 4 bytes at offset 4 of each
draw): the lines then draw exactly what the frame culled.
