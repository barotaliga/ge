---
title: A cloud is lit by many scatterings, judged from where photographs are taken, and a sun or moon behind it shows every cut
date: 2026-10-23
scope: engine/render/clouds.wgsl, engine/render/wgsl/flash.wgsl, engine/render/wgsl/frame.wgsl, engine/render/sky.wgsl, engine/render/frame.cpp, plugins/mountain/app/cloud_field.cpp, games/fall_line/labs/gpu_world_lab.cpp
concepts: [clouds, multiple-scattering, diffusion, deck, cover, lightning, glow, corona, transmittance, half-float, sun-disc, moon-disc, reference-photos, lab-check]
---

- A heap under Beer's law and one lobe draws grey (optical depth about 20 toward the sun). Its
  light dies at the reduced extinction σ(1 - g), g about 0.85, with a second lobe weighted for the
  later orders (3). A deck is lit by diffusion, 1/(1 + σ′τ), mixed with the heap's by the kind's
  dome. A lightning flash lights a patch 1-2 km across (`flash_spread`, exp(-r / 1200 m)).
- A cover is a quantile of the noise (`cloud_cut`), measured over whole noise tiles.
- Compare a kind with photographs from the valley, sun behind and to one side (the `clouds`
  view), never from the summit; stamp the eye's height against each layer, judge the in-cloud
  look at `sv_clouddensity` 0.5 up, and shoot wind across the view (`sv_winddir`), not along it.
- The sun's disc (up to 60 000 against white near 1) shows through transmittance over 1e-5: keep
  T itself (not 1 - T) in half-float targets, and take the rest of a stopped march as thick as the
  ray so far. A kind that must hide the sun needs about 1.7 times its column opacity.
- The diffraction lobe is hundreds of whites; ease the glow toward a few whites and blend it to
  white as it brightens, or per-channel clipping bands it into a rainbow. Round the moon the glow
  is lifted as seen (`glow_seen`), so an early `break` in any layer draws a line across the disc:
  find it by removing one stage at a time. A disc drawn larger reads its glow off the drawn limb
  (`theta - disc_swell()`). Look at the sun from the `veil` view.
