---
title: Smoke in a room is sunlit through the roof unless the game hands it the room's light
date: 2026-11-05
scope: engine/render/explosions.wgsl, engine/render/particles.wgsl, games/shooters_edge/client/world_draw.cpp
concepts: [smoke, blast-light, indoor, sun, white-luminance, shining, palette, pre-lit]
---

The explosions' march and the particle pool light smoke by `sun_light` and the sky with no shadow
of a roof, so in a brush world every blast's smoke, every dust puff and every muzzle's puff glowed
white in a dark corridor. A game of rooms sets `Blast::light` / `MuzzleFire::light` (the open sky's
share, the lightmap's light under it in whites) and pre-lights its smoke particles as shining specks
of their tint times that light (Shooter's Edge's `light_at` in world_draw.cpp: the lightmap probe,
and a march straight up the tree for a sky leaf). Units that are the picture's WHITE
(`white_luminance()`) and not cd/m^2 are what make a "fullbright palette colour" or "as bright as
the room" exposure-proof; a beam's sheath in cd/m^2 was invisible at 450 in a map keyed near
daylight and needed thousands.
