---
title: Halfway up a ladder no node is near; a climber is never lost, as a lift rider is not
date: 2026-11-26
scope: plugins/shooterbots/fighter.cpp, games/shooters_edge/bots/q2_unit.cpp
concepts: [ladder, climbing, lost-to, nav-nearest, off-the-graph, fighter-think, keys-by]
---

- The fighter's "off the graph" step (`lost_to`) walks a bot with no node near it straight at the
  nearest one. Up a ladder's shaft no node is near, so it pressed back and aside off the rungs at
  the same height every climb, fell, and climbed again (xsewer1, four minutes). The pilot was
  still holding forward on the ladder; the fighter's command overrode it.
- To find who changed the keys after the pilot, read `Fighter::keys_by` (set by `keys_mark` at
  each step of `fighter_command`), not `Bot::set_by`. Here it said `lost`.
- A bot that loses hold of a ladder halfway up: print the move's ladder trace (fraction,
  contents) beside the command's keys. Contents with the ladder bit and a fraction under 1 mean
  he is still on the ladder.
