---
type: Added
title: Teleporters drawn as magic ink, and filtered textures, in the v2 BSP renderer
---

The v2 BSP renderer draws a map's teleporter panels as their own liquid: black ink with grey smoke folded through it and rising, threads of silver liquid running up the panel in drops, and sparks rising in front, the panel breathing slowly. Each teleporter is also a pulsing silver light over its room (`Options::portal_lights`). The map's textures are now filtered (between texels, between mip levels, and with extra samples along a slanted surface's stretched direction) instead of read one texel at a time; the crisp look stays a setting (`Options::crisp`, `r_texfilter 0`). The `gpu-bsp` lab adds a view of each map's teleporter and the `--crisp` and `--portal-lights` flags.
