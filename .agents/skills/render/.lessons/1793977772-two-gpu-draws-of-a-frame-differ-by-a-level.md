---
title: Two draws of one frame on a GPU differ by a level; compare them with a tolerance, and draw the reference twice before blaming a pass
date: 2026-11-06
scope: games/fall_line/labs/gpu_world_lab.cpp
concepts: [cull-check, hi-z, occlusion, metal, nondeterminism, tolerance, probe, world-lab]
---

The `world` lab's cull check compared the culled and the unculled still byte for byte and failed
at random on Metal for two weeks (#101); the suspects were the pyramid's mip chain and the
atomic appends. A throwaway probe settled it in minutes: draw the UNCULLED still twice and compare
those two. They differed in the same 3 pixels, by one level of one channel, so the cull was never
the cause. Before suspecting the pass a check is about, compare the reference with itself on the
same adapter; on a GPU, count a pixel only past a few levels (`kHardwareWobble`), which a dropped
instance always passes.
