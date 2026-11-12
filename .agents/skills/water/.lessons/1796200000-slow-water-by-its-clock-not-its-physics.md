---
title: Slow a water by its own clock (WaterDraw::pace), and keep the wakes on the real one
date: 2026-11-11
scope: engine/render/water.cpp, engine/render/wgsl/water_body.wgsl, plugins/brush/app/bsp_liquids.cpp
concepts: [water, pace, time, waves, stir, gusts, wakes, classic, pool, water-speed]
---

A pool a few metres across in a classic room read hurried even after its quick ripples were cut
(M41p): real waves at that scale look too fast. Slowing it means scaling every clock the surface
runs on alike, or it shears: the components' angular frequencies (scaled once as they are staged,
so height_at, the slope, the caustics, the fold and the foam all follow), the gusts' and the foam's
drift (m/s, in the uniform), and the stir's and the gusts' noise clocks (`sun.y` times t in
water_body.wgsl). The wakes and the rain's rings must NOT be scaled: they start at a moment the game
hands in on the real clock. A GPU test proves it as an identity: the sea at pace 0.7 at 10 s draws
the sea at pace 1 at 7 s, once the sky's own clock (measured with the water held at pace 0) is taken
out of both differences.
