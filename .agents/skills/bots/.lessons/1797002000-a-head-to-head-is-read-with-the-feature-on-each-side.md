---
title: A head to head is read with the feature on each side in turn; one side's draw read -6 frags for a change that was even
date: 2026-11-10
scope: games/shooters_edge/labs/se_team_lab.cpp, plugins/shooter/features.cpp, plugins/shooter/look_ahead.cpp
concepts: [head-to-head, a-b, sides, skew, noise, features-red, features-blue, se-team, verdict, bodies, ahead, presets, all]
---

- `ahead` was held off the presets on one 12-seed set with it on blue: -7.4 frags a match. Run
  with it on RED on the same seeds it read -0.3, and on seeds 13-24 +1.5 (red) and +5.2 (blue).
  Over both sides and 24 seeds: +0.4, even. The sides are not even and the skew itself moves
  with the build and the seeds (+2.6 on 1-12, -2.4 on 13-24), so one side's result is a draw.
- The verdict set is FOUR variants: `base:--sides=1` (the skew), `h2h:--features-red=+x
  --sides=1`, `h2hb:--features-blue=+x --sides=1`, and the feature on both; the feature's
  effect is the mean of (red-blue in h2h - skew) and (skew - red-blue in h2hb). 12 seeds of 10
  minutes with three variants take ~10 minutes on the Mac (16 threads).
- Read each side's movement apart: se-team's `team.side.<side>.speed-avg` and
  `bumps.by-wall-per-bot-min`/`by-player-per-bot-min` (under `--sides=1`). The team-wide mean
  mixes both sides and moves with how fights go (a side's speed swung 162-183 between sets
  with nothing changed on it).
- A NEW FEATURE MUST BE LEFT OUT OF `features_all` UNTIL JUDGED: `kAll` is every bit under
  `trial`, so a switch added before `trial` is on in every preset at once, and `--features=+x`
  against `+y` reads the same digest. `bodies` was A/B'd against itself that way: bit-identical
  sets are the sign (check the digests before reading the numbers).
- The client-fed world (`net/classic_mirror.cpp`) puts every other player as `kSolidNot`, with
  health 0 or 100 and the velocity sent: filter players by `free` and `health`, never `solid`.
