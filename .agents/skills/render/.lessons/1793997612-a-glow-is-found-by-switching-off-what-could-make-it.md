---
title: A glow too strong is found by switching off each thing that could make it, never guessed
date: 2026-11-06
scope: plugins/brush/app/bsp_flames.cpp, plugins/brush/app/bsp_draw.cpp, engine/render/emitters.cpp, games/shooters_edge/app/look.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [glow, bloom, sparks, blackbody, near-field, inverse-square, point-light, fire, lava, knockout, film]
---

- The brazier's white disc in Shooter's Edge was not the flame (its brightness cut tenfold changed
  nothing) but its sparks: 100 of them at 2900-3400 K, a filament's heat, summed by an
  unthresholded bloom. The torch's white oval was not the torch's light (a wider near field
  changed nothing) but a map `light` entity beside it, its inverse square softened over 0.2 m:
  dozens of times its half-reach light on the wall, where the classic's straight fall-off peaks
  at twice. Both were found in minutes by `se-client --game=<packs> --cvars="<one thing off>"` at
  one view, after an hour spent guessing at the obvious knob.
- A point light that stands in for a classic light: soften it over a share of its half reach
  (`Options::near_soft`, 0.4: about six times the half reach's at the light).
- A blackbody particle's light is steep in its heat: a wood fire's sparks are 1900-2400 K here; a
  room-adapted eye makes 3000 K a lamp.
- Motion is judged from `se-client --film=<frames>`'s GIF, as a strip and as every frame lightened
  over the others (ffmpeg `blend=all_mode=lighten`): the trail shows straight lines at once.
