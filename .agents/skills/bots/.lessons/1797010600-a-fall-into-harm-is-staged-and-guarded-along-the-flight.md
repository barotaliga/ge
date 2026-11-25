---
title: A fall into harm is staged by place and guarded along the flight; a trap is judged by who falls
date: 2026-11-10
scope: plugins/shooterbots/brink.cpp, plugins/shooterbots/look_ahead.cpp, plugins/shooterbots/floor_trap.cpp, plugins/shooterbots/keyboard.cpp, plugins/shooterbots/fighter.cpp, games/shooters_edge/labs/se_reach_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [brink, brinkfall, brinkkeys, lava, falls, staging, walk, air-brake, wall-slide, keyboard-lag, floor-trap, side-metrics, client-fed, dm2]
---

- A run down a stair is airborne step to step, so a guard on the ground met the bot sliding over the
  last lip. In the air a wish against the run brakes at full acceleration (`air_accelerate` caps
  speed gained, not lost): gate the look on the flight's reach (speed x 0.6 s) and fly the box with
  the move's slide along walls (a path that stops at a wall reads it as safety). dm2's YA stair: 33
  -> 3 deaths a set.
- A pit's lava entries are ~5 a match (±6.5 over nine): stage the place. `se-reach --walk='x y z > x
  y z' --walk-variants=` flies 60 runs a speed in seconds, on a route taken off the falls' own
  starts and goals, under the keyboard and a trailing view (`--walk-view-lag=0.3`): the analog pilot
  fell 0 of 240, the keyboard 26. Guard the keys (`brinkkeys`), not the pilot's wish.
- A side sweep on a stair is blocked by the next step: sweep one and two steps higher
  (`nav_falls_in_harm`).
- Count lava deaths by the contents at the feet (trace `liquid` -5), not height; dm2's big-room
  entries come in bursts (the floor trap): leave them out of a fall's count.
- A floor trap is judged per side (`team.side.<side>.deaths-in-lava`): the first killed its own side
  as often as the enemy while head to head read even. Trace each firing and who fell in the next
  seconds. An unseen mate is not sent under `--client-fed=1`: use `Mind::mates`. A shut route is not
  shut feet: guard the keys near a trap while it is dangerous.
