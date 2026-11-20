---
type: Added
title: Taking an item flashes the view gold, recordings can keep the weapon that last fired, and the crosshair moves and takes Quake II's colours
---

Picking up an item now flashes the view gold for half a second, on a server you join, in a recording and in your own game, as the classic games do; `cl_bonusflash 0` turns it off, and an `f_bonusflash` alias runs on each pickup. `r_viewmodellastfired 1` makes a recording keep showing the weapon that last fired while the player looks through others.

The crosshair can be moved off the middle with `cl_crossx` and `cl_crossy`. In Quake II, a config's `ch_x`, `ch_y`, `ch_scale` and `ch_alpha` now place, size and fade the crosshair, and `ch_red`, `ch_green`, `ch_blue` and `ch_health` colour the crosshair, `ch_health 1` by your health.
