---
title: A campaign step no route reaches is read off the map, one entity at a time
date: 2026-11-21
scope: games/shooters_edge/bots/way_through.cpp, games/shooters_edge/bots/map_nav.cpp, games/shooters_edge/labs/gpu_se_q1_campaign_lab.cpp
concepts: [campaign, way-through, complete, push-trigger, teleporter, bridge, pillar, node-in, plan-log, each]
---

- Run one map alone with `--each=1 --plan=1` (four at once on four cores, each from its own copy
  of the labs binary): "explore to 0 0 0" means nothing he reaches is left, so the next door is
  shut to a step whose node is unreached. Print the steps (kind, node, its origin, mover,
  reached, doors) and the barred list once every 20 plans behind an environment variable, then
  delete the print before the commit.
- A list piped to `sort | head` hides the step you are after: grep the node by number.
- `search.reached` on a plan's first call reads an empty search: a debug print there crashes.
- A step's node on the mover the step opens (e2m6's trigger round the pillar it lowers) is
  barred: take an off-mover node a step outside and walk him in (`way_touch`).
- A `trigger_push` up is a link to the floor at its TOP: measure the landing in three dimensions,
  not across alone (e3m5's air tube took the roof 900 units over it). Its bottom over head
  height is a standing jump's link from under it.
- A start-open door that sinks to open (angle -2) and has room on its drawn top is a bridge out
  of a pit, as a slid plate is (e2m6).
- Key doors: spawnflag 8 = gold (`item_key2`), 16 = silver (`item_key1`).
