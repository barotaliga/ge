---
title: A fog hides things in its own grey, a mist closes into the horizon's sky, and a far land is kept out of the world by its shape
date: 2026-10-23
scope: engine/render/wgsl/frame.wgsl, engine/render/fog.wgsl, engine/render/wgsl/valley_fog.wgsl, engine/render/sky_style.cpp, engine/render/wgsl/atmosphere.wgsl, engine/render/far_land.cpp, engine/render/far_land.wgsl, engine/render/far_land_kinds.cpp, engine/lab/labs/gpu_far_land_lab.cpp, plugins/brush/labs/gpu_bsp_lab.cpp
concepts: [fog, mist, haze, veil, overcast, exposure, valley-fog, dust, sky-type, far-land, skybox, keep-out, horizon-shadow, glade]
---

- Under a deck key a white face lower (0.6, not 0.78). Droplets scatter every colour alike, so
  what a fog or fall hides fades to grey and the sky is seen through the same veil.
- The draw distance's mist closes into the horizon's sky, not the sky behind the pixel; keep it
  out of `haze` (the clouds call it; `haze_air` is the air alone) and draw the same colour in the
  sky pass. A ground mist measures height from the terrain (`FogVolume::hug_ground`).
- A dust sky needs a scale height near 10 km and asymmetry 0.55-0.65; a type cannot be "dim",
  since the exposure keys to white: darkness is absorption in every channel.
- A medium's top lit off a noise takes the coarse octave's slope and a wrapped cosine.
- A far land behind a closed world needs no stencil: keep it under the world's lowest point
  outside its box, and a `greater` test draws it only through the sky. Aim a lab at the bearing
  whose ranges stand highest 1.5-6 times the first range out; a broad fbm body carries ridges;
  bare rock follows the tree line. A polar grid of 640 bearings by 384 rings keeps peaks smooth.
- Far water's glade is GGX with ripples as roughness. Every surface the key light reaches needs
  its horizon, water too.
- Judge the land from the `gpu-bsp` `<map>-high` view and in the window (`r_farland <kind>`).
