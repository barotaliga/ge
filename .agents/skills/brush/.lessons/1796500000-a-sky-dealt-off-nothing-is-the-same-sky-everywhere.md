---
title: A classic sky's day was dealt off seed 0, so every map had the same clouds; deal it off the map's name
date: 2026-11-05
scope: plugins/brush/app/bsp_sky.cpp, games/shooters_edge/client/world_draw.cpp
concepts: [sky, clouds, pick-sky, night-over, seed, sv-hour, cloud-stack, warp, patch, cover]
---

The game draws `pick_sky`'s DAY at `sv_hour -1` (its default); only the labs and an hour after dusk
use `night_over`. The day's one deck took no seed (only `night_over` set one) and the texture's
cloud share as its cover, so every map with the same sky texture had the identical sky, a lumpy
deck over most of it, and standing still. `pick_sky(colours, map)` now deals the seed, the wind and
a stack of kinds off the name. Judge a classic sky by `gpu-bsp --night=0` (what the game shows),
not by the night the labs default to. Covers in `SkyLayer` are before the islands (`patch`) thin
them: the sky seen is about a half to two thirds of the number.
