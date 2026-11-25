---
title: A gap trick is debugged staged, through the matches' keyboard and a copy of the hands, never in matches
date: 2026-11-10
scope: plugins/shooterbots/pilot_gaps.cpp, plugins/shooterbots/nav_gaps.cpp, plugins/shooterbots/gap_jumps.cpp, games/shooters_edge/bots/gap_tricks.cpp, plugins/shooterbots/keyboard.cpp, games/shooters_edge/bots/pilot_run.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [gap-tricks, gapbuild, gap-run, gaps-fly, gap-trace, rollout, hands, keyboard, curvekeys, underway, speed-jump, harm, staged]
---

- A gap trick in the nav is not a gap jumped: a hopper plans one only at 380 u/s, which bots rarely
  reach (`team.gap-jumps` 0). People curve in from ~150 units at ~180 u/s and leave at ~490: a gap
  needs its own run-up (`gapbuild`). Check the gaps' `harm` column (dm2's are all over lava). Read
  off demos, a walk on a brush entity is one long "hop": trace through the solid func_ entities.
- `se-reach --gaps=fly --gap-trace=<tsv>` found five bugs in an afternoon two match sets had not (a
  curve looking 3 links ahead, a landing 161 out refused, BACK pressed in the air, the flight
  steered by the hop's key instead of as `fly_jump` flew it, chained gaps): 1 jump -> 140.
- A rollout (`gap_run`) presses through a COPY of the pilot's hands: with keys landing at once it
  chose side swaps every 4 commands that hands needing 5 cannot do. Re-run every command, it keeps a
  patterned way's beat (`gap_way_i`).
- Staged analog, 137 of 140 landed; matches 45 %. Pressed through the keyboard (`keys` in
  `--gap-features`) the staging matched the match (81 of 101). Hand the keyboard the keys the pilot
  means (`Pilot::curve_keys`), not a slewed axis read back. The jump key goes down 1-2 commands
  late: jump only where asked now and one and two later all land (`gap_lands_late`), and hold keys
  and view until the feet leave (58 -> 93 % landed).
- On the ground under both ends of its link a walker has fallen under the way: replan at once
  (`underway`).
