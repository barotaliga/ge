---
title: Bots wet where people are dry lack a dry way, not a price on water; a swimmer needs its own exits, clocks and view rules
date: 2026-11-01
scope: games/shooters_edge/bots/water.cpp, games/shooters_edge/bots/fighter.cpp, plugins/shooter/pilot.cpp, plugins/shooter/swim.cpp, games/shooters_edge/labs/se_reach_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [water, wet, swim, dive, drowning, commit, give-up, route-wet, gap-tricks, se-people, samples, onground, swimway]
---

- Count deaths by cause before tuning the aim: 13 of 90 e1m2 deaths were drownings on routes along a
  pool's floor; turning a swimmer up after six seconds under lifted frags more than any aim change.
- A plan that commits a bot past its other rules (a dive) needs a way out on its own clock, then a
  cool-down on that goal: a dive failing at its ledge bobbed bots up to 821 s a match. TIME in the
  committed state found it, not counts of dives begun. A pickup made by a floor walk had hidden the
  broken dive.
- A swimmer is never `onground`: the stuck and lost checks never ran in water. Swim level just off a
  floor (ground friction adds to the water's); dive only where a box trace at that height is
  blocked.
- A bearing to a point nearly overhead swings 40 degrees a unit of drift: the swim's way aiming the
  view at the next cell was most of the view's jitter. Do not turn toward a point nearer than 48
  units across.
- e1m2's bots were wet 27-36 % of their time, people 9 %. Pricing water 2-4x moved nothing: by place
  (`se-people --samples`, 128-unit cells) people stood in the same cells dry, on walkways above the
  flooded floor. `se-reach --route=... --route-wet=1000`: a route that still swims has no dry way.
  People cross by gap jumps (`se-rj --dir=` writes `gap_tricks.tsv`; e1m2 had 113, the nav none).
  se-people's `wet` is the cheap first tell on a map with water.
