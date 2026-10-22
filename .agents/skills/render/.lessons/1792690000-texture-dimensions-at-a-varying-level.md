---
title: Never ask textureDimensions for a level that differs between threads; derive it
date: 2026-10-22
scope: engine/render/instances_cull.wgsl
concepts: [hi-z, mips, texture-dimensions, software-adapter, lavapipe, occlusion, cull]
---

The instances' cull read the pyramid at a mip picked per instance and sized its texel rectangle
with `textureDimensions(hiz, mip)`. On the software adapter that answer is wrong when the level
differs between the threads of one dispatch, so the rectangle landed on the wrong texels and a
few visible trees (one to five pixels a view) were culled. Every simpler suspect (the margin,
the mip choice, the reprojection) "fixed" it by accident because each moved the rectangle; a CPU
replica of `hidden()` over the read-back pyramid said visible, which pointed at the shader's
reads. The mip's size is now `max(size0 >> mip, 1)`, the pyramid's own halving rule
(`render/hiz.hpp`), and only level 0's size is asked for. When a GPU result disagrees with its
CPU replica over the same inputs, suspect a per-thread query before the arithmetic.
