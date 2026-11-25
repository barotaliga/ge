---
title: A question about a map's ways is answered off the BSP before the maintainer is asked
date: 2026-11-19
scope: plugins/shooterbots/map_nav.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [map-reading, brush-ents, targets, buttons, doors, bridges, slices, nav-is-not-the-map, maintainer-time]
---

The handoff carried "ask the maintainer how one gets from button 104 to the red armour's bridge"
for two sessions because se-reach's routes showed no such way. The nav is a reading of the map, not
the map: everything was in the BSP. `brush ents dm2 --class=func_*` gives every button's `target`
and every door's `targetname`, `angle`, `lip`, `wait` and `spawnflags` (button 136 on the ledge ->
door t11, up 5 s, out to a side room; button 104 there -> the bridge t10, out 20 s, start open), and
the slices (calm/py/heights.py, drawgoal.py) show the rooms, the window and the floor heights. Read
those first; ask only what the map cannot say (how people use it, which way is easiest), and when a
route is missing in the nav, say which link is missing, not that the way is unknown.
