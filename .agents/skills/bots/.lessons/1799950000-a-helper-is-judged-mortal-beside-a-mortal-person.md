---
title: Judge co-op helpers with the stand-in mortal, and count what they take from him
date: 2026-11-28
scope: plugins/shooterbots/party.cpp, plugins/shooterbots/fighter_items.cpp, games/shooters_edge/bots/coop_episode.cpp, games/shooters_edge/bots/q2_unit.cpp
concepts: [party, helper, co-op, heal, health, armour, share, god, mortal, left-to-people, party-leave-items]
---

- With god on (the labs' default) nobody's health ever drops, so nothing a helper does about
  health shows: run `--beside` with `--god=0` (both labs). Mortal, the stand-in alone died on 6 of
  7 maps of e1 and Quake II's first unit; with two helpers he got out of 3. That gap is what
  helpers are worth.
- The sharing hook is item-level and inert without people: `Fighter::left_to_people` is empty for
  every bot not in a party beside people, so `best_item`/`on_the_way` read nothing new and no
  match golden moves. A goal item newly marked must be dropped (`goal_item = -1`,
  `next_think = 0`) or the fighter's `kept` goal walks on to it.
- Compare with a lab switch (`--share=0`) in the same binary, not two builds: one helper's choice
  changes the fight, so single runs move kills and times both ways; read the counted quantity
  (health taken in his need: 118 -> 52) and finishes, not kills.
- Debug-preset campaign runs take ~2.5 s of wall time per game second with a party of three;
  build `build/release` labs for any sweep.
