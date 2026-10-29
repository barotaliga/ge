---
type: Added
title: BSP maps rebaked at finer luxels with bounced, coloured light and drawn with it
---

`ge::brush::rebake_map` rebakes a map's light with the engine's baker (its lamps, its sky and its glowing liquids, two bounces of coloured light, the light's direction a luxel) and keeps it as a small cache (`write_rebaked` / `read_rebaked`). The v2 BSP renderer draws a map with that light (`Lighting::rebaked`), shading each texture's relief by where the light comes from. The `gpu-bsp` lab shows it beside the map's own lightmaps with `--rebake=<luxel m>`.
