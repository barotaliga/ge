---
title: A pass that bends the frame copies it first, and an animated surface wraps its phase where it is unseen
date: 2026-11-05
scope: engine/render/haze.cpp, engine/render/haze_copy.wgsl, engine/render/liquid.wgsl, engine/lab/labs/gpu_lava_lab.cpp
concepts: [heat-haze, refraction, scene-copy, depth-read-only, lava, flow-map, crossfade, fract, pop, frame-to-frame, dpdx]
---

- Heat haze cannot sample the target it draws into: copy the colour first (`haze_copy.wgsl`,
  rebound on resize), then draw into HDR with depth `depthReadOnly` so the same depth is also
  bound as a texture. Prove a few-milliradian bend by a diff against the run with its encode
  skipped (pixels over 3 levels), from the side camera: straight away from the eye shows nothing.
- Lava's stills looked fine while the game swapped the surface every 4 s: a plate's age
  `fract(hash + t * rate)` jumped cold to hot, and the flow map's tent weights were inverted (a
  phase's weight is `1 - abs(1 - 2 ph)`). `gpu-lava`'s `steady` view charts the share of pixels
  that jump between frames 0.2 s apart; `se-client --film=60 --film-step=0.2` films a real map.
- A normal from `dpdx`/`dpdy` of the height and the plan point costs one evaluation where two
  more evaluations tripled the surface (lava 35.5 to 18.9 ms in software).
