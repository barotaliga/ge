---
title: Find a map's likely rocket jumps by calculating their arcs, not by flying the move
date: 2026-11-15
scope: plugins/shooter/jump_finder.cpp, games/shooters_edge/labs/gpu_map_overview_lab.cpp
concepts: [jump-finder, rocket-jump, arc, calculated, ride, teleporter, walk-filter, dm3]
---

Searching run-ups, pitches and timings with the full move (`prove_rocket_jump`) cost minutes a
map and still missed jumps; the person asked for calculated jumps. The finder now takes off from
the jump, the run, a ramp's rise or a rising lift, bursts the rocket where its line meets the
world, adds the blast's push and sweeps the arc's box through the map in 1/72 s steps (sliding on
walls, steering toward the ledge in the air). `rocket_reach` comes out 257 against the move's
measured 256. For the walk filter to mean anything the graph must carry the map's lifts
(`func_plat` as `NavMover`s with a travel) and its teleporters (`nav_add_links`; a trigger in an
alcove is touched from the nodes 48 units off its box). A ledge no walk reaches (dm3's roofs)
keeps `walk -1`: that is a true trick, not a bug.
