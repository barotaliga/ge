---
title: A new look lands off by default until its owner likes it; the fold's first order is what the whitecap law's threshold assumes
date: 2026-11-05
scope: engine/render/water.hpp, engine/render/water.cpp, engine/render/water.wgsl, engine/lab/labs/gpu_water_lab.cpp
concepts: [water, foam, whitecaps, default-off, look, sign-off, jacobian, fold, whitecap-cover, lace]
---

The foam landed on by default and the owner did not find it nice looking: every game's water
changed at once. `WaterDraw::foam` is now false and the `gpu-water` lab turns it on for its foam
views. A look a game sees by default (foam, a new sky, a grade) lands switched off and shown in
its lab, and is turned on only once its owner has seen it.

Two things a parallel attempt at the same foam measured (a history texture round the eye instead
of the waves' past per pixel), for whoever reworks the look: a threshold set from the slopes'
Gaussian statistics holds the whitecap law only on the fold's FIRST order (the sum of
k a sin(phi)); with the full Jacobian a 10 m/s sea drew half the law's cover, since the
Sxx Szz - Sxz^2 term pulls the tail in, the more the further out the threshold. And foam lace cut
from value noise turns to bars when drawn out along the wind; a jittered grid's F2 - F1 (bubble
walls), warped and roughened by a finer net, reads as foam.
