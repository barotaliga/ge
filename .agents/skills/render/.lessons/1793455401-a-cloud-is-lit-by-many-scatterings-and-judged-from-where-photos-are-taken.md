---
title: A cloud is lit by many scatterings, a deck by diffusion, and each is judged from where its photograph is taken
date: 2026-10-23
scope: engine/render/clouds.wgsl, engine/render/wgsl/flash.wgsl, plugins/mountain/app/cloud_field.cpp, games/fall_line/labs/gpu_world_lab.cpp
concepts: [clouds, multiple-scattering, diffusion, deck, cover, lightning, reference-photos, lab-check]
---

- A heap under Beer's law and one lobe draws grey: toward the sun it crosses an optical depth of
  about 20. A cloud's light dies only at the reduced extinction sigma (1 - g), g about 0.85; a
  second lobe there, weighted for the orders past the first (3), lights the sunward side like the
  snow and leaves the base grey. From the sunward side a heap IS grey with a bright rim.
- A deck (stratus, nimbostratus) is lit by diffusion, falling as 1/(1 + σ′τ), not exp(-σ′τ):
  mix the two by the kind's dome, take the path to the slab's top, dim the sky's light by it,
  and blend a lid's sheet halfway toward the deck's own luminance so it agrees with the flat light.
- A lightning flash lights a patch a kilometre or two across where the channel leaves the base
  (`flash_spread`, exp(-r / 1200 m), one order of scattering), not the whole deck.
- A cover is a quantile of the noise (`cloud_cut`), not a threshold; measure the share over whole
  noise tiles, not one 2 km map.
- Compare a kind with photographs from the valley with the sun behind and to one side (the
  `clouds` view), never from the summit. Stamp the eye's height against each layer's base and top,
  or a shot inside a deck and one in a gap under it cannot be told apart; judge the in-cloud look
  at a picked `sv_clouddensity` (0.5 up), not a map's lean. To see motion, shoot `--at=` a sequence
  with `sv_winddir` across the view: wind along the line of sight looks still.
