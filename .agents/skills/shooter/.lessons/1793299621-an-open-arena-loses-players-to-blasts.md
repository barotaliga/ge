---
title: An open-topped arena loses players to blasts; roof a map a crowd plays on
date: 2026-10-29
scope: games/shooters_edge/sim/arena.cpp, games/shooters_edge/server/host.cpp, games/shooters_edge/labs/se_server_lab.cpp
concepts: [arena, roof, rocket, knockback, stand-in, se-server, trails, airborne]
---

The test arena's walls stop at z = 256 and nothing is over them. In the scripted match no one
leaves, but with eight bots firing rockets for a minute a blast throws a player over a wall,
he falls past the floor's edge forever, and the run's numbers (90% of frames airborne, trails
2000 units outside the map) look like a movement bug. The trails picture showed it at once.
The server builds the arena ROOFED (`build_arena(a, true)`, the roof the last brush so the
match's arena and its golden are unchanged). Look at the trails before reading the tables, and
a share of frames in the air far over what the jumps explain is a player off the map.
