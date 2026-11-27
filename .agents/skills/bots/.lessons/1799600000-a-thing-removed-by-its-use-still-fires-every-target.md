---
title: A breakable removed by its own shot still fires every target
date: 2026-11-27
scope: games/shooters_edge/server/q2_map.cpp, plugins/shooterbots/way_bosses.cpp, plugins/shooterbots/way_through.cpp
concepts: [fire-targets, killtarget, func-explosive, trigger-push, current, shot-door, hangar1, gone]
---

- A Quake II bot pinned for minutes with the same `vel` in every "given up" line is held by a
  `trigger_push` (velocity ten times its `speed`), not by a wall. Find who `killtarget`s the push
  (`secli map ents <map> --json`) and check that the killer really fires: a temporary print in
  `blow()` and in the push's touch showed the box blown and the push still there.
- `fire_targets` returns when its user is `gone` after a use, to catch a user removed by its own
  target. A breakable, an item taken and a monster dying are already `gone` before they use their
  targets, so that check cut them off after the first target. It now stops only when the use
  itself removed the user.
- A plain `func_door` with `health` and no name opens to a shot and shuts again after `wait`. The
  way through added it as a shot step but never locked it, and `door_open` calls any door it does
  not know open, so the step was done from the first frame. Any door with a step must be in
  `w.locked`, or the step counts as done.
- A hover monster sitting on the lip of a pool can hold a bot waist deep in the water under it
  while the fighter shoots at a tank across the map. Seen once on hangar1 and not fixed yet.
