---
title: A swimmer at the top of the water is in a cell with no swim node; plan from the one under him
date: 2026-11-26
scope: plugins/shooterbots/nav.cpp, tests/shooterbots_swim_test.cpp, games/shooters_edge/bots/coop_episode.cpp
concepts: [swim, nav_nearest, surface, no-route, reach_from, pool, swim-node]
---

- A swim node stands where its cell's centre is in the water, so a swimmer floating at the top
  is often in a cell with none, and the nodes below lie past nav_nearest's 64 units: he plans
  from nowhere ("pilot idle 0/0 no route", `reach -1 0` in the campaign lab's frames) and floats
  for the rest of the map. nav_nearest now looks two cells down.
- The campaign lab's frames line ends with `reach <from> <n> swim <0/1> goal <node>`: a `-1`
  reach is a bot with no node under it, the first thing to read when it stops with no route.
