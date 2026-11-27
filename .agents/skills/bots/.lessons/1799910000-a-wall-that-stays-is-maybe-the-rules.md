---
title: When a door or wall never opens for the bot, check the rules loaded the key that opens it
date: 2026-11-27
scope: games/shooters_edge/server/q2_targets.cpp, plugins/shooterbots/nav.cpp, plugins/shooterbots/pilot.cpp
concepts: [killtarget, trigger-counter, rules, boss1, moving-platform, lift, rim, pinned, dock]
---

- On boss1 a long run of nav and pilot work got the bot onto both button ledges, and the wall still
  stood: the rules never loaded `killtarget` for target-class entities (`targets_spawn`), only
  `spawn_one` did. A temporary print in the counter's use (count, killtarget, the wall's `gone`)
  found it in one run. Before more bot work on "a way that never opens", print the entity that
  should open it the moment it fires.
- `brush ents maps/<map>.bsp --root=<paks> --game=baseq2 --near=x,y,z` and following the
  `target`/`targetname` chain answer "what opens this" fast; check `spawnflags` 1792 (not in
  single player) before planning around an entity: boss1's teleporter is deathmatch only and
  the real way up is a train that rises and falls.
- A bot standing still in mid air 56 units over the floor beside a lift is pinned: a rising
  platform lifted the edge of his box into a lintel and stalled on him (god mode keeps both there
  for good). The rim node of a lift's shaft overlaps the lift's footprint by a few units; wait
  clear of the whole way (`rises`), and once aboard at the boarding stop skip the rim node.
- A temporary per-0.2 s pilot trace (mode, link, `because`) in the unit run, printed only for a
  window of seconds, explained each stall in one rerun; strip it before the commit.
