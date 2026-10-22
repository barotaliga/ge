---
title: A run's foot is not a lift's load zone, so an outing needs a line across the snow
date: 2026-10-22
scope: games/fall_line/sim/outing.cpp, games/fall_line/sim/lift_ride.cpp, games/fall_line/sim/game.cpp
concepts: [outing, lift-ride, load-zone, run-chain, foot, state-hash, golden]
---

On seed 38 (generator v10), the runs end in the valley 40 to 500 m from the nearest bottom
station, and no run carries a `to` link to a lift. A gondola's load zone is behind its station
(`entry_at` is negative). So the bot cannot ski a run's chain into a lift. Its way to the lift
(`to_lift_line`) runs on from the foot across the snow: first to a point `kLineUp` m short of
the zone on the line up, then up through the zone. It is held under `kWalkSpeed` past the foot,
so he comes in slow enough and facing up the line. Two traps:

- `bot_controls` brakes once `arrived`. On a line laid to a lift (`BotPlan::to_lift`) it must
  ski on past the foot, and `arrived` is cleared when a lift lets him go, so the next foot
  counts.
- Folding a new field into `state_hash` moves every free-ride digest, even where the trajectory
  is the same. Before rewriting `fall_line_free.txt`, check that the time and metres bits did
  not move. Only the digest column should change.
