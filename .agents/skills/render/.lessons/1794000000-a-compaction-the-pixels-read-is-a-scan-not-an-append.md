---
title: Compact what a draw reads with a scan, not an atomic append, and hold the cull to drawing everything
date: 2026-10-30
scope: plugins/brush/app/bsp_cull.wgsl
concepts: [cull, compaction, scan, atomic-append, determinism, hi-z, two-phase, indirect-draw]
---

An atomic append puts the drawn faces' triangles in whatever order the threads ran in. Two
coplanar faces then win the depth test in a different order from frame to frame, so the
picture's pixels, and a lab's digest, depend on scheduling. The world's cull scans the faces'
sizes in one workgroup instead, so each face lands at the place its order gives it. Two checks
caught real bugs here. First, read the cull's counters: a late phase that tested every face not
marked 0 drew the early phase's faces twice and doubled the "drawn" count, with no change in the
picture. Second, draw each view culled and again drawing everything, and compare the pixels with
the visible set off. The set is the map's data. The arena's rooms see only two doorways deep, so
a face seen through three aligned doorways is lost by the data, not by the GPU.
