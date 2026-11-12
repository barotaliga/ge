---
title: Stage a rare move in a lab and read its trace before matches; a look-ahead that rolls the move must press through the bot's own hands
date: 2026-11-12
scope: games/shooters_edge/labs/se_reach_lab.cpp, plugins/shooter/pilot_gaps.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/pilot_run.cpp, tests/shooter_nav_gaps_test.cpp
concepts: [gap-tricks, gapbuild, gap-run, se-reach, gaps-fly, gap-trace, rollout, hands, look-ahead, run-to-the-lip, staged]
---

- A move a match shows once in ten minutes cannot be debugged in matches. `se-reach --gaps=fly`
  runs every gap trick of a map with an expert fighter's pilot (`fighter_pilot`, the same settings)
  from 160-640 units behind its lip on three seeds in 5 s, and `--gap-trace=<tsv>` writes every
  command with the pilot's `because`. It found five bugs in an afternoon that two match sets had
  not: the curve looked only 3 route links ahead (nodes are ~32 apart, its reach 160 units); a
  landing 161 units out was refused instead of run on; the hop's stretch ended at a gap's landing
  mid-flight and the walk then "lined up" for the next jump by pressing BACK in the air (the
  misses: into the water); the flight after the takeoff was steered by the hop's side key, not
  flown as the takeoff check (`fly_jump`) had flown it; and chained gaps broke the hop at the
  first. Staged: 1 jump and 0 landings -> 140 jumps and 137 landings.
- A look-ahead that rolls the move forward (`gap_run`: straight, a curve, side-to-side strafes)
  must press each command through a COPY of the pilot's hands (`ai::Hands` copies cheaply and
  replays the same wander). Rolled with the view and keys landing at once, it chose side-to-side
  strafes every 4 commands that practised hands (side key travel 30/s: 5 commands to swap) could
  not do: the speed swung 330-405 and never built. Through the hands it picks what the hands can
  do, and a straight approach honestly finds nothing; people curve in from the side.
- A rollout that is re-run every command must keep the beat of a patterned way (`gap_way`,
  `gap_way_i`), or a side-to-side way restarts at its first leg each command and never swaps.
