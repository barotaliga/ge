---
type: Added
title: loc_draw place markers, pruneloc and $cl_surface in every classic game
---

`loc_draw 1` stands a marker at every place point within `loc_dist` of the player (Quake's the player's head, Quake II's the ancient head), the nearest one bobbing under a white light, so a place file can be checked and edited by eye. `$cl_surface` gives the texture straight ahead and its surface flags. Both work in Quake and Quake II. `pruneloc` (also `loc prune`) shrinks a place file by taking out every point that names no spot differently, so a room marked by dozens of points of one name keeps only the few its edges need.
