---
title: A lamp is laid to its lighting class, the eye is adapted to it by a geometric mean, and nothing that glows at it casts
date: 2026-10-28
scope: engine/render/lights.hpp, engine/render/lamp_shadows.hpp, engine/render/eye.wgsl, engine/render/eye_adapt.wgsl, engine/render/wgsl/water_light.wgsl, plugins/mountain/app/night_lamps.hpp, games/fall_line/app/lamps.hpp
concepts: [lights, lamp-lux, eye-adaptation, auto-exposure, night-lamps, lamp-shadows, caster-slots, lamp-glitter, smith-masking]
---

- On a moonless night the sky keys the eye for 0.0015 lx, so correct torches burn white. Take
  `lamp_lux` as the geometric mean of a few points the eye sees (pools and the dark between);
  the arithmetic mean leaves all but the brightest pool black. Compare lit and unlit at the same
  `lamp_lux`, or "the lamps brighten it" measures the exposure.
- The adapted eye meters every tap at `sky_exposure()`, never its own output, or it chases itself;
  `exposure()` alone reads `SkyLight.eye`.
- Lay floodlights to the class with a test summing light across the piste (mean and least); one
  flood a mast by eye left 0.15 lx gaps. Pose a worn lamp off the rig's joint (`headlamp_of`).
- A lamp's own bulb or post fills its tiles at the near plane and shadows the scene: put glowing
  parts in a family with no caster slots, and hang the light clear of solids.
- A lamp low over water needs Smith masking on both rays (`smith_g1`).
- Labs that build `FrameOptions` field by field miss a new cvar until they copy it.
