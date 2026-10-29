---
title: The pre-exposure must count the lamps, or a bright emitter in a dark room renders black
date: 2026-10-28
scope: engine/render/night.cpp, engine/render/beams.wgsl
concepts: [pre-exposure, half-float, overflow, nan, tone-curve, emissive, local-lights, black-pixels]
---

The HDR target is RGBA16F and its ceiling is 65504. `pre_exposure` scales stored light to the
scene's expected brightness; when it read only the sun and the sky, a room lit by torches at
night got a scale near 1e9, so a lightning core's white was stored past the ceiling, the target
held infinity and the tone curve turned it into NaN, drawn black (the beam and the lit gun both
came out as black shapes). `pre_exposure` now adds `FrameView::lamp_lux`, and an emissive
fragment clamps what it writes to 60000. When something bright draws black, read the HDR
target's raw values before touching the shader's colours.
