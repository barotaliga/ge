---
title: A fast flight's walls are split by how it took off before the air's steering is blamed
date: 2026-11-14
scope: plugins/shooterbots/pilot.cpp, plugins/shooterbots/pilot_hop.cpp, plugins/shooterbots/pilot_recipes.cpp, plugins/shooterbots/bot.cpp
concepts: [hop, wall, takeoff, recipes, aimland, hopside, trace, flights, line, crash]
---

- Before changing how the hop steers in the air, split the trace's fast flights (380+ at takeoff,
  a wall = 80+ lost in one command, the health unchanged) by their TAKEOFF: who wrote the view
  (`view`), a copied start in the 0.4 s before (`because` "copied"), and the takeoff's heading off
  the hop's aim (`way`). On dm2 the hop's own takeoffs on its aim met walls 28 % of the time (people
  27 %); takeoffs 30+ degrees off met them 52-91 %, and they were half of all fast takeoffs. A
  better line in the air (`hopline`, a string pulled through the route's corners) moved nothing.
- Under `hopside` the air's side key follows the mouse's turn: in a fight the enemy steers the
  velocity, and only letting go is left. A takeoff there is the last steering the hop has
  (`aimland`: no jump into a wall straight ahead).
- A copied start's speed comes from its curve: refusing the ones that take off off the way
  (`startline`) or jumping early when the curve passes the way (`startcut`) both cost the fast
  flights they were meant to save.
- A trace column for a new decision's outcome (here why the line found no point: no samples, out
  of the turn, walled) found two bugs in an hour: a step-up trace in the air meets the lintel the
  flight passes under, and side traces started beside a box hugging a wall start in solid.
