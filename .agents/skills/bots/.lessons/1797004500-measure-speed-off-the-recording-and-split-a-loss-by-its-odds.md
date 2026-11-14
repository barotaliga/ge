---
title: Measure speed off the recording by people's rule, and rule a cause out by its odds before building a fix for it
date: 2026-11-14
scope: games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_moves_lab.cpp, plugins/shooter/bot.cpp, plugins/shooter/pilot.cpp
concepts: [speed, hop, se-moves, blast-cut, yardstick, crash, wall, air-keys, view-lag, odds, experts, dm2]
---

- se-team's trace speed is not the yardstick: it counts the speed a blast gave. On dm2 the experts'
  trace read 36-39 % of their time at 320+, people's level, while se-moves on the same matches'
  recordings (`--save`, then `se-moves --dir`) read 24 % against people's 38 %: both lose about
  21 % of their time to the blast cut. Compare speed only off recordings, both sides by one rule,
  experts against people (`--skills=expert`).
- A trace shows plenty that looks wrong: 43 % of the hop's fast air commands pressed no key, the
  view 2-6 degrees behind the velocity. Before building a fix, split the BAD OUTCOME's odds by
  that state (here a wall hit per command: 1.0-2.5 % with keys, without, the other side, after a
  swap) and read the same state off people's recordings (their view trails the turn as much).
  Two features built on the look of it (`hopclear`, `airhold`) and a longer wall look each cost
  a set and moved nothing; the odds script would have ruled all three out in a minute.
- `se-moves --track=<tsv>` writes every frame (phase, speed, heading, view, blast) for people and
  bots alike: flights' losses split into one-frame drops (a wall) and gradual ones answer "wall or
  air" at once (bots 91 % walls).
