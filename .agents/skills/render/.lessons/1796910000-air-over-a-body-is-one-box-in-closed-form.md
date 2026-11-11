---
title: The air over a body is one box in closed form, its far faces drawn, and lavapipe's pass times lie
date: 2026-11-10
scope: engine/render/liquid.cpp, engine/render/liquid.wgsl, games/shooters_edge/labs/gpu_se_burn_lab.cpp
concepts: [heat-haze, fumes, closed-form, far-faces, cull-mode, premultiplied, lavapipe-timestamps, liquid-air]
---

A volume over a surface (heat haze, a gas, a plume) needs no march: a layer falling off as
exp(-h / s) has an exact weight along a straight ray, and a Gaussian column's is taken at the
ray's closest approach to its axis. Draw the box once, its far faces only (outward winding needs
`CullMode::Back` here; `Front` drew nothing with the eye inside the box), so a pixel is shaded once
from outside or inside. Two effects over the same box belong in one fragment: the gas laid over the
bent scene (opaque there) or alone (premultiplied), one blend state for both. On lavapipe a pass's
timestamps do not hold its raster work (the air pass read 0.3 ms while the whole frame grew by
17 ms): compare whole-frame spans, A/B toggled by cvars in one process, over many frames.
