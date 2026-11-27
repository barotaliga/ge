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
- The guard reads the killing boxes themselves (`NavGraph::hurt_boxes`), never the node marks:
  the marks carry margins, so down in a pit every way round was "harm" and he stood still for
  600 s. Switching the guard off on marked nodes switched it off on the lips too. Keep the
  margins on the plan (no way leads there) and the exact box on the keys.
- "out no" at 900 s with every objective done can be the plan or the runner: rboss's exit
  reaches the end through a `trigger_key` and a relay (`level_change_via`), and the game's last
  exit asks for no map (`Q2Server::game_over`), which the unit lab only watched `pending_map` for.
- One rboss rerun is ~3 min; small guard changes flipped the end (finish, freeze, death). Judge
  a rule by what it reads (box or margin), not by one lucky run.
