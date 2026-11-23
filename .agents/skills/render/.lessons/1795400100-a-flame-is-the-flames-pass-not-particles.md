---
title: Real fire is render/flame.hpp's marched gas; a hot particle reads as a square speck
date: 2026-11-22
scope: engine/render/mark_fire.cpp, engine/render/flame.hpp, engine/render/particles.wgsl
concepts: [flame, fire, particles, blackbody, kelvin, mark-fire, smoke]
---

Small flames made of particles failed twice on hot marks. At 1300 to 1500 K the blackbody light
is too dim to see beside a glowing decal, and at 2600 K they show as square orange flecks:
particles.wgsl draws an unstreaked particle as a soft rectangle. For anything a player should
read as fire, use `render/flame.hpp`: a `Flame` with a small height and radius, `candle_flame`
as the starting preset, and `relative` set by the game to its fires' brightness. Add it to the
flames pass's list beside the map's torches (Shooter's Edge: world_extras' `burn_extras`). Keep
particles for smoke and sparks. Flames standing on a near-white glow cannot be seen, in reality
too, so let them outlast the brightest part of the glow (mark_fire's `fire_until` 0.7).
