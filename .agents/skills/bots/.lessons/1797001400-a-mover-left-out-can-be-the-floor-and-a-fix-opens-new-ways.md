---
title: A mover left out of the nav can be the floor; a fix that opens ways runs links never run before
date: 2026-11-09
scope: games/shooters_edge/bots/map_nav.cpp, plugins/shooter/pilot.cpp, plugins/shooter/nav.hpp
concepts: [door, floor, plat, teleporter, no-route, walk-still, mover-in-the-way, nav, dm2, stand]
---

The nav proves its ways with every mover LEFT OUT, which is right for a door in a doorway and
wrong for a door that IS a floor: dm2's big room stands on plates a button slides away, so the room
was a hole and one spawn an island (`no route` 20 %, a bot stood there all match). `se-reach
--route` between the spawn and anything said "no route" at once; that is the first check when a
bot idles with `goal_node -1`. A named plate door resting shut is now a floor at rest
(`floor_door`), its nodes shut while it is away. A door with no name moves with the named door
its box touches: read the chain, not the door's own keys.

Opening ways sends bots down links nobody walked before, and their faults show next: the new floor
made teleporter 38 usable, and its link (a walk across the map in 0.1 s) was "passed through" and
cut to like any walk, so bots steered at the destination and pinned themselves on a wall for a
minute. A link that is not walked needs its own flag (`NavLink::teleport`), not a guess by length.
After any nav fix, run `stand.py` again and read the NEW top spot before measuring the set.

A stall the trace cannot explain (speed 0, `walking the route`, forward held) wants the server's
state round it: a temporary `GE_TMP_*` fprintf in `pilot_step`, filtered by a box, on a 1-minute
seed. It showed plat 206 UP and the bot standing inside its shaft, its field holding the plat up for
good: momentum had carried the bot onto a footprint node while "stepping clear", and footprint
links were "stepped off any time". A ridden mover over the player's feet is now in the way.
