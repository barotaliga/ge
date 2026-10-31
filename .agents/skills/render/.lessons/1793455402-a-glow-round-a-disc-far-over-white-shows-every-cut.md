---
title: A sun or moon far over white shows every rounding, threshold and colour clip in the cloud in front of it
date: 2026-10-24
scope: engine/render/clouds.wgsl, engine/render/wgsl/frame.wgsl, engine/render/sky.wgsl, engine/render/frame.cpp
concepts: [clouds, glow, corona, diffraction, transmittance, half-float, march-stop, sun-disc, moon-disc, tonemap]
---

- The sun's disc is drawn at up to 60 000 against a white near 1, so it shows through any cloud
  transmittance over about 1e-5. Keep the march's stop at 0.01 but take the rest of the path as
  thick as the ray's so far, and keep the transmittance itself (not 1 - T) in half-float targets.
  If only the quarter-resolution frame shows the disc, it is precision.
- A kind's `opacity` is the full column's; a sheet's lens and eaten edge leave about 0.6 on the
  ray, so a kind that must hide the sun needs about 1.7 times that depth. Light a glow by the
  ray's own transmittance.
- The diffraction lobe is hundreds of times white for degrees; per-channel clipping bands it
  into a rainbow. Ease the glow toward a few whites and blend its colour to white as it
  brightens. A thin layer's sunset colour comes through its edges (the kind's `deck` knob).
- Round the moon the glow is lifted as seen (`glow_seen`, ratio^0.35), so a thousandth is visible
  and any `break`/`continue` dropping more draws a line across the disc. Find it by removing one
  stage at a time (`kGlowSeen = 1`, then one layer's glow), then read that layer's early exits.
  The longer march is paid only within 0.1 to 0.3 rad of the key light.
- A disc drawn larger reads its glow at the angle off the drawn limb (`theta - disc_swell()`).
- The post pass keeps the moon's disc for the cones, so a cloud in front must be handed to the
  rods in the cloud pass. Look at the sun from the `veil` view; the `clouds` view keeps it behind
  the eye. `sv_clouddensity 0 .. 1` with `--day=172` shows the thickness ladder in one strip.
