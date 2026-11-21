---
title: A cleared session may inherit a set still running, and a split keeps a report's metric order
date: 2026-11-21
scope: games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/team_items.cpp, plugins/shooterbots/fighter_goto.cpp, plugins/shooterbots/knowledge.cpp, tests/shooters_edge_fight_test.cpp
concepts: [set, background-run, handoff, line-cap, split, metric-order, digest, ab, own-pickup, belief, fight-test]
---

- After `/clear`, a set the last session left in the background can still be running into the
  directory the handoff names as pending (`p2/ce/dm3` was empty, then filled minutes later). Check
  `ps aux | grep 'labs se-team'` before starting it again: two runs into one `--out` race, and the
  duplicate takes every core from the one already half done.
- Rebuild `build/release/labs` only when no set is running on it: build the gates' targets on
  `build/debug` meanwhile.
- A file over the 2000-line cap is split by subject (`fighter_goto.cpp`: the goal handed to the
  pilot; `labs/team_items.cpp`: se-team's item metrics). A lab's block moved to a function is
  called where the block stood, one call per block, so the report keeps its metric order and the
  smoke digest does not move for a refactor.
- A preset switch that moves the body (`airend`: flights to the landing) can break a BELIEF test,
  not a golden: the fight test's own pickups against the server's. Bisect by `features` with
  `-<switch>` per preset addition, then print the bar's rises beside the server's takes for the
  one bot and seed: a miss read 74 off an item taken in the air; an extra was a backpack's rise
  put on an item believed gone. Fix the belief, never the tolerance.
- A preset change moves labs that `make precheck` never reaches: #1251 put `airend` in the preset with only the tests'
  goldens rewritten and left main red on 25 lab digests.
  Before the PR, run every lab's smoke digests, ONE SHARD AT A TIME with its `--out` deleted after
  each: eight shards at once wrote ~2.6 GB of pictures and filled the disk. A GPU lab whose
  digest hashes a picture file (`se-fly`'s fly.gif) differs on the Mac from CI: build main beside
  (`git worktree add`) and run it there before blaming the change.
