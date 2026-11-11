---
title: A pilot change moves every shooter lab, but precheck ran only two; the queue's macOS row found the other eleven and a broken one
date: 2026-11-11
scope: plugins/shooter/pilot.cpp, plugins/shooter/pilot_movers.cpp, tools/precheck.sh, tests/shooters_edge_fight_test.cpp, tests/shooter_senses_test.cpp
concepts: [precheck, digests, affected, labs-smoke, se-pilot, merge-queue, statistical-test, seeds, reaction, arena, plat]
---

- `make precheck`'s digest gate ran se-team and se-why for a change to the shooter pilot; the
  queue's macOS row then failed 11 other labs' digests (se-pilot, se-duel, se-coop, se-lobby,
  se-people...) and se-pilot's own check (human hands never rode the arena's cycling plat).
  After any change in `plugins/shooter` or `games/shooters_edge/bots`, run every lab's smoke
  digests before a push: `labs --all --smoke --shard=i/8 --quiet --check-digests=tests/digests`
  eight at once (about 10 minutes on the Mac). Rewrite moved ones ONE AT A TIME with
  `labs <id> --smoke --write-digests=tests/digests`: eight writers at once kept only one file.
- The tests that hold the bots by a few short matches (senses: the skills' aim order over three
  runs; fight: 3 frags a seed over four) flip when a path changes. Two answers were honest:
  a measure fixed to measure what it says (a "reaction" ended when a runner rode INTO the
  crosshair; now only when the view turned onto him), and more seeds with the per-seed floors
  unchanged (six). Never lower a floor; say in the commit which runs were outliers and why.
