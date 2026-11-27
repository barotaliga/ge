---
title: The way through's idle clock runs only while he has nothing to do
date: 2026-11-27
scope: plugins/shooterbots/way_through.cpp, plugins/shooterbots/way_parts.cpp
concepts: [idle-since, hopeful, explore, city1, stale-clock]
---

- A plan line that says "(0 shut)" means the hopeful spell is on: every door is taken as open for
  kWayHopeful_s. If it starts the moment the goal becomes "explore to 0 0 0" (one plan with
  nothing reachable, such as a fall off the graph), the idle clock was stale. `idle_since` was set
  only in the explore branch, so a step, a monster or a gun never reset it.
- To see why a reachable step is not picked, print `left_until`, `ever`, `keyed`, `held_step`,
  `covers`, `worth`, `roam`, `prey` and `left_to_do` for that one step, around the time it is
  dropped. Pass 0 takes only steps that are `worth` something; pass 1 takes the rest.
- City1's bot still misses the running jump at (-904, -1784) onto the walkway at (-744, -1752)
  on its way to the `big_secret` button, and falls about 150 units each time.
