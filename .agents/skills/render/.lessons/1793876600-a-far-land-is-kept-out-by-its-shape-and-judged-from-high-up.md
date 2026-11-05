---
title: A far land is kept out of the world by its shape, aimed at its highest ranges and judged from high up in a map
date: 2026-11-05
scope: engine/render/far_land.cpp, engine/render/far_land.wgsl, engine/render/far_land_kinds.cpp, engine/lab/labs/gpu_far_land_lab.cpp, plugins/brush/labs/gpu_bsp_lab.cpp
concepts: [far-land, skybox, keep-out, sky-mask, horizon-shadow, lapse-rate, lab-view, glade, placeholder-digest]
---

- Backdrop geometry behind a closed world needs no mask or stencil: keep the land under the
  world's lowest point inside the circle round its box, and after the sky faces' fix-up (their
  pixels back to the cleared depth) an ordinary `greater` depth test draws it only through the sky.
  The classic veil multiplies those pixels afterwards, so land and sky dim together.
- A lab that aims at "the highest horizon" from the site picks the nearest big hill; aim at the
  bearing whose ranges stand highest between 1.5 and 6 times the first range's distance.
- Ridged noise squared and scaled alone makes lone spikes; a range reads as a range when a broad
  body (a coarse fbm) carries the ridges. The bare-rock band belongs to the TREE LINE, not the
  current snow line, or a cold kind loses its forest under the snow.
- A polar grid with a constant ring ratio is coarse radially (ratio - 1) against 2 pi / bearings
  across: 640 bearings by 384 rings keeps a far peak from reading as a faceted pyramid.
- Check the land where it is seen: the `gpu-bsp` `<map>-high` view (just under the largest sky
  ceiling) on the user's pack, and in the game window by `se-client --cvars="r_farland <kind>"`;
  the id maps' skies are violet or red-brown, so `auto` gives them only alien kinds (`--sky=earth
  --night=0 --farland=<kind>` shows an earthly one).
- A glade on far water is the GGX lobe with the sub-pixel ripples as roughness, not a sharp
  `pow` glint (a dot far off). Every surface the key light reaches needs its horizon, the water's
  too, or the moon's path shines on water the ridge hides it from.
- Plan a GPU lab's shots once (a list of ids) and walk that list in both the device branch and
  the placeholders': two loops that list them separately drift apart, and Windows (no adapter)
  is the first to see the digest move.
