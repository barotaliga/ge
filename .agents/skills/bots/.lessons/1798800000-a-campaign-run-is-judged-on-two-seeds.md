---
title: A campaign bot change is judged on two seeds per map, and a stall is read off the plan log
date: 2026-11-22
scope: plugins/shooterbots/way_through.cpp, plugins/shooterbots/tour.cpp, plugins/shooterbots/map_nav.cpp, plugins/shooterbots/nav.cpp
concepts: [campaign, tour, complete, seed, stood-still, hopeful, hatch-drop, dead-node, swim]
---

- One run of a map is chaotic: the same change moved e1m1 from 117 s to 3/23 kills on another
  build. Run `--seed=1` and `--seed=2` for each map you judge (four processes on four cores, each
  from its own copy of the labs binary) before calling a change better or worse.
- A tour that is worked out again every few seconds against moving monsters flips its first stop
  and walks him back and forth: work it out again only when its first stop is gone (or every 30 s).
- "explore to 0 0 0" in a complete run is a stall: nothing he reaches is left. Print the reached
  count, the `from` node with its links, and the barred doors once every 20 plans behind an
  environment variable; delete the print before the commit.
- A door that toggles (spawnflags 32) can be shut behind him by a trigger of its own name he fires
  later (e1m6's end button): the only way on is then a floor door it slides away, which the nav
  only knows as a drop link through the hole (`MapNav::hatch_drops`).
- A clock that restarts whenever the target changes never fires (a pack of fish each chosen for
  a second): the stood-still clock restarts only when he moves.
- A retry block that resets `idle_since` starves every later block that waits on it (the swim
  back out of a pool): give it its own clock.
- A node with no links at all (a corner cell, a dry spot the closed water entries cut off) taken
  as the nearest leaves the plan with nothing reached (`reached=1`): `nav_nearest` passes over it
  unless nothing with a way out is within 64 units.
