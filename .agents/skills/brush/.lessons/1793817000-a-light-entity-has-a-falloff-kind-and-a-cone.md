---
title: A modern compiler's light has a fall-off kind and a cone; read as a straight omni light it whites out the rooms
date: 2026-11-04
scope: plugins/brush/lights.cpp, plugins/brush/app/bsp_draw.cpp, plugins/brush/app/bsp.wgsl
concepts: [light-entity, delay, falloff, inverse-square, spotlight, mangle, cone, color-key, overexposed, remastered]
---

The remastered packs' maps were lit by a modern compiler: `delay` picks the fall-off (1 inverse, 2 inverse square, 3 none, 4 a floor, 5 inverse square eased over 128 units), `mangle` or a `target` aims a spot (`angle` its whole cone, 40 when unset), `_color` colours it, `_surface` makes a template that is not a light itself. `start` has 27 lights of 1000 (`delay 5`, aimed), `dm6` 14 of 2500 (`delay 3`, aimed). Read as straight omni lights, the real-time path's strength (the straight light matched by an inverse square at half its reach) grows as the CUBE of the value: a 2500 is about 600 times a 300, and the modern look of those maps was white while dm3 (lights of 300) looked right.

Now `light_entities` matches a falling light at 128 / wait units (`gain`), eases `delay 5` over that distance (`soft_m`), skips floors and templates, and aims spots (`spot`, `dir`, `cone_cos`); the GPU light carries the cone (`GpuLight::spot`, `info.w`) and the bake takes `LightKind::spot`. A change here changes the rebake: the lightmaps stage's version moved with it.

To find a map's bright lights: read its entity lump (the pak's directory, then the BSP's first lump) and count `light` by (`light`, `wait`, `delay`). The classic look (lightmaps) of the same map is the reference.
