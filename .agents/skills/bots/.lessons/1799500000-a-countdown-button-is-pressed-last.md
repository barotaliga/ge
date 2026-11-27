---
title: A countdown button is pressed last, and a death can be what opens a door
date: 2026-11-27
scope: plugins/shooterbots/buttons.cpp, plugins/shooterbots/way_gates.cpp, plugins/shooterbots/way_through.cpp, games/shooters_edge/bots/q2_unit.cpp
concepts: [button-dooms, killbox, countdown, evacuate, deathtarget, gate-dead, triggered, biggun, god-mode]
---

- A Quake II unit bot that sits for minutes at one spot with its origin at the FLOOR's height (24
  under where a player stands) is dead, not stuck: a gib's box starts at its origin. God mode stops
  everything but a killbox (`func_killbox` and blasts of thousands kill through it). Check
  `deathtarget`, killboxes and `target_explosion` dmg in the map's entities (`secli map ents
  <map> --json`) before chasing collision.
- A trigger with spawnflags 4 waits for its name to be fired; that name may come from a monster's
  `deathtarget`, not a step. An entity's spawnflags 1792 (not easy, medium or hard) exists only in
  deathmatch, so a door it opens never opens in single player.
- The unit lab's `--track` times drift from the plan notes' times by several seconds; trust the
  plan notes' clock when timing a countdown.
- A long `--seconds` changes the run itself (the plan paces on it), so rerun at the same seconds to
  see the same run.
