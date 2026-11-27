---
title: A trigger that kills through god mode is harm like lava, and the brink guard must see it
date: 2026-11-27
scope: plugins/shooterbots/map_nav.cpp, plugins/shooterbots/brink.cpp
concepts: [harm, trigger-hurt, god-mode, rboss, nav, brink, nav-hurt-at, trigger-push, func-rotating]
---

- "He died in god mode" is almost always a `trigger_hurt` with a huge `dmg` and the flag that
  ignores protection. Read the entities near the death point first (`secli map ents <map>
  --json`, boxes in `mins`/`maxs`), then `--track=0.2` to see the last seconds: on rboss a
  `trigger_push` blew him 700 units into a pit, and the last death was a walk off a lip.
- Marking nodes is not enough. Four fixes in a row (nodes in the box, its margin, links across
  it, drops into it) left the death bit-identical: the pilot's brink guard reads only lava and
  slime contents, and a walk off the lip into a trigger pit was never braked. A pit no contents
  show needs `NavGraph::hurt` and `nav_hurt_at` in the guard. When a nav fix leaves a run
  bit-identical, the fault is not on the graph: stop marking and find what drives the keys.
- Print the graph around the spot once (node origins, harm, link counts) before guessing: it
  showed every pit node marked and the walkway's links into it cut.
