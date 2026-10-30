---
title: A disc drawn larger than it is needs its cloud glow read off the drawn limb
date: 2026-10-28
scope: engine/render/frame.cpp, engine/render/clouds.wgsl, engine/render/sky.wgsl
concepts: [sun-disc, moon-disc, disc-size, corona, diffraction, glow, aim-at-moon]
---

The sky draws the sun's and the moon's discs off one radius each (the sun's off `style[2].w`, the moon's off `moon_light.z`), so a larger disc is one factor at the upload in `fill_uniforms`. The sun's disc already shares its light over its solid angle, so it keeps its light when it grows. The moon's face keeps its brightness, and its light on the world comes from `Night`, not from the radius, so the snow is unchanged.

A cloud's glow is a point's diffraction pattern. Left alone, a moon drawn five times larger has its corona's bright core as a small spot in the middle of its face. Reading the pattern at the angle off the drawn limb (`theta - disc_swell()`) moves the ring out with the edge. A swell of zero (any disc no larger than the real one) keeps every picture bit for bit.

To aim the `world` lab at the sun or the moon, use `vista@sun:<lens>` or `vista@moon:<lens>` (the render skill's § Labs); no heading needs printing first.
