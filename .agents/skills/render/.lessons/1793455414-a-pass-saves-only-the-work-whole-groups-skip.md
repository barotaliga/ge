---
title: Skipping pixels inside one pass saves nothing, and a resolution change is priced from a view its subject fills
date: 2026-10-23
scope: engine/render/clouds.wgsl
concepts: [clouds, temporal, reprojection, divergence, lanes, checkerboard, quarter-resolution, upsample, perf, price-table]
---

- Marching one pixel in each two by two per frame inside the half-size pass cost as much as
  marching all: a SIMD group runs as long as its slowest lane, and every group held a marcher. Put
  the pixels whose turn it is in a pass of their own over a quarter-size target and let the
  half-size pass only gather (fresh pixel or history). Keep the divergent fallback march rare.
- From the chase camera a quarter-resolution march priced the same as full, because the sky is a
  sliver and the row's cost was the cloud shadow every surface reads. Looking up from the start it
  halved (72 to 36 ms). Price a pass from a view where it fills the frame, by the whole frame's
  median.
