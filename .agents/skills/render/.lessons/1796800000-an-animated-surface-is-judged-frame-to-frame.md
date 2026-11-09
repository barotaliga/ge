---
title: An animated surface is judged frame to frame; a wrapped phase pops where it is seen
date: 2026-11-09
scope: engine/render/liquid.wgsl, engine/lab/labs/gpu_lava_lab.cpp
concepts: [lava, flow-map, crossfade, fract, pop, frame-to-frame, steady, shader-cost, dpdx]
---

Stills of lava looked fine for weeks while the game showed plates vanishing and the whole surface
swapping every 4 s. Two wraps did it: a plate's age `fract(hash + t * rate)` jumped from cold to
hot in one frame, and the two-phase flow map's tent weights were inverted (`abs(1 - 2 ph)` is 1
where the phase wraps; a phase's weight is `1 - abs(1 - 2 ph)`). Any `fract` or `floor` of time
in a shader must land where its result is unseen: a weight of 0, or a state equal on both sides
(the plate is all melt at age 0 and at age 1). Prove it with motion, not stills: `gpu-lava`'s
`steady` view charts the share of pixels that jump (> 40/255) between frames 0.2 s apart and
fails on a spike over the trace's median; `se-client --film=60 --film-step=0.2` films a real map.

Cost: a normal from two more full evaluations a footprint away triples the surface's price;
`dpdx`/`dpdy` of the height and of the plan point, solved for the plan gradient, is the same at
that scale for one evaluation (lava 35.5 -> 18.9 ms in software). Time a pass by drawing the
same frames without it.

A file a GPU lab attaches is in its digest by name, even when measured, and in order: the no-GPU
path (`ls::placeholders`) must attach the same name at the same place, so attach after the last
shot in both paths. Check `GE_GPU_OFF=1 labs <id> --smoke` gives the GPU run's digest.
