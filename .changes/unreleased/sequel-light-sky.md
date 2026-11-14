---
type: Fixed
title: The sequel's maps lit by their own coloured light, under their own turning sky
---

Shooter's Edge's modern look lights a version 38 map by its own lightmaps, so its rooms keep the colour and brightness they were built with instead of the darker, greyer light the point lights gave. Its sky box is the one the server sends (the world's `sky`, else `unit1_`), drawn in the modern look as in the classic, and turned by the map's `skyrotate` about its `skyaxis` on the level's clock (`sky_box_turn` in `plugins/brush/app/sky_box.hpp`).
