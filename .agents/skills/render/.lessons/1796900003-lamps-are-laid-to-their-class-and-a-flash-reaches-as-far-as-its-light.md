---
title: A lamp is laid to its lighting class, the eye adapted by a geometric mean, and a flash's reach follows its light
date: 2026-10-28
scope: engine/render/lights.hpp, engine/render/lamp_shadows.hpp, engine/render/eye.wgsl, engine/render/eye_adapt.wgsl, engine/render/wgsl/water_light.wgsl, engine/render/explosions.cpp, plugins/mountain/app/night_lamps.hpp, games/fall_line/app/lamps.hpp
concepts: [lights, lamp-lux, eye-adaptation, auto-exposure, night-lamps, lamp-shadows, caster-slots, flash, local-lights, range, smith-masking]
---

- `lamp_lux` is the geometric mean of a few points the eye sees (pools and the dark between); the
  arithmetic mean leaves all but the brightest pool black. Compare lit and unlit at the same
  `lamp_lux`. The adapted eye meters at `sky_exposure()`, never its own output.
- Lay floodlights with a test summing light across the piste (mean and least), not by eye.
- A lamp's glowing bulb or post goes in a family with no caster slots, hung clear of solids, or
  it shadows the scene from the near plane. A lamp low over water needs Smith masking both ways.
- A flash's reach follows its light (`sqrt(cd / 1 lx)`, between the look's range and four
  times it), and the scene feeds `lamp_lux` at the eye before `fill_uniforms`: a fixed 22 m cut
  the light with a hard rim and left the eye outside it, so the night's gain blew the frame out.
  A metric counting pixels a blast changes switches the flash off.
- Labs that build `FrameOptions` field by field miss a new cvar until they copy it.
