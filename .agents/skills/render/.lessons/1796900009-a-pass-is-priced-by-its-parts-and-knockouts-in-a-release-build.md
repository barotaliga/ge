---
title: What the knockouts found in the brush world's shader and Shooter's Edge's quality stops
date: 2026-11-02
scope: plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_draw.cpp, plugins/brush/app/bsp_views.cpp, engine/render/clouds.wgsl, engine/render/water.cpp, engine/render/sky.cpp, games/shooters_edge/app/ladder.cpp, games/shooters_edge/labs/gpu_se_timedemo_lab.cpp
concepts: [gpu-cost, knockout, price-cvars, quality-ladder, light-loop, scissor, full-screen-pass, planar-mirror, pcss, capsules, clouds, shafts, register-pressure, metal]
---

- `se-timedemo --price=1` pointed at three switches; the biggest cost was the loop over up to 160
  lights near the eye (2.6 of 7.5 ms at 1440p). The parallax march run for one step still cost a
  third of the full one. Unused lightmap, rebake and weather paths cost 0.16 ms: `kLitOnly`
  (an `override`) took it back. Keep per-face paths (the teleporter's liquid) in both.
- `--price-cvars` found the casters' cost in capsules (0.43) and soft edges, not count; the
  mirror's in redrawing every face; the cloud's in its passes (a quarter of the pixels saved
  nothing); the water's wet band in a full-screen triangle a pool (0.26 ms for a waterline).
- A full-screen pass discarding outside a known volume is scissored to the volume's projected
  corners (whole screen when one is behind the eye); a second view's clip-plane test comes first.
- A quarter-resolution cloud march inside the half-size pass saved nothing until the marching
  pixels got a pass of their own; from the chase camera it priced as full, looking up it halved.
- The light shafts were six cloud-shadow reads a pixel (1.84 ms at 1440p); `sky.shafts`
  (128x72x32) reads once.
- Noise: one desktop spike adds 0.5-0.9 ms; a text cvar takes one word (quote a value with spaces).
- A visual trade is judged by `<lab> --diff=<tag>`: the flames' march at 14 or 20 steps failed
  only the `wind` shot; say which shot fails and why rather than loosening its floor.
