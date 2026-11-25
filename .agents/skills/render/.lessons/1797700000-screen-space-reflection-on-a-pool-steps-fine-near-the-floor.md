---
title: A pool's reflection steps finely near the floor and accepts what stands on it
date: 2026-11-21
scope: engine/render/beams.wgsl
concepts: [ssr, reflection, blood, pool, paint, mirror, thickness, streaks, boots]
---

The shared scene reflection (`mirror.wgsl`'s `scene_reflection`) is tuned for a whole glossy floor:
its first step is about 0.2 m and it takes a hit up to 0.5 m behind the depth. Under a pool a
player stands IN, that smears his boots into vertical orange streaks and misses the boots
themselves. The paint's own march (`paint_mirror`) steps quadratically from 1 cm, takes a hit only
within `kPaintThick` (0.3 m) of the depth or 1.5 steps, and fades its share from `kPaintSure`
(0.08 m) behind, so the boots mirror under themselves and the streaks go. A thin thickness (0.04)
was worse: the rays under a boot then go "hidden" (debug colour green) and show nothing. Debug a
reflection by colouring the three outcomes (hit, hidden, off screen) before tuning a number.
