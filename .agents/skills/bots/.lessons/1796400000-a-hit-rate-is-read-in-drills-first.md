---
title: Read a bot's hit rate in a weapon drill and per player before tuning its aim
date: 2026-11-05
scope: games/shooters_edge/net/classic_hits.cpp, games/shooters_edge/net/classic_team.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/qc/arena/world.qc
concepts: [hit-rate, accuracy, drill, respawn, telefrag, trigger]
---

The first per-skill hit rates were far under the bands, and none of the reasons was the aim. A
respawn empties the ammunition, which a counter of ammunition spent reads as a burst of shots;
the trigger compared the bot's wanted and pressed view while a swim or a button had sent another;
and a teleporter that did not telefrag left players stacked inside each other, firing for whole
matches at a box their traces started in. Each showed only per player (a few players at
thousands of shots and 0%) and in a drill (`se-team --drill=<weapon>`, one weapon for everyone
on a map whose every item a route reaches), never in the mixed per-skill table, where weapon
mixes and maps move the rates more than skill does. Log the trigger's real miss (the
perpendicular distance from the shot line to the target) before turning a knob.
