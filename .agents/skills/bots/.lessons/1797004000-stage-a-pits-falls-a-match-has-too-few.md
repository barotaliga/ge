---
title: Stage a pit's falls with se-reach --walk; a match has too few to judge, and the keyboard is the suspect
date: 2026-11-13
scope: games/shooters_edge/labs/se_reach_lab.cpp, games/shooters_edge/bots/keyboard.cpp, games/shooters_edge/bots/pilot_run.cpp, plugins/shooter/look_ahead.cpp, plugins/shooter/nav.cpp
concepts: [lava, brink, falls, staging, se-reach, walk, keyboard, brinkkeys, fallseen, noise, floor-trap]
---

- A pit's lava entries are ~5 a match: nine matches carry ±6.5 of noise, so three traced seeds
  "halving" them (fallseen 21 -> 10) and nine more doubling them were both noise. Stage the place
  instead: `se-reach --walk='x y z > x y z' --walk-variants='base:;x:+feature'` flies 60 runs a
  speed in seconds. Pick the route off the falls' own starts and goals (the trace's position a
  second before leaving the ground, the `goal` column): a route the falls did not walk shows none.
- Stage under the keyboard AND the trailing view (`--walk-view-lag=0.3`): dm2's YA stair never
  drops the analog pilot (`-keys`: 0 of 240) but the keyboard's 45-degree sets and taps drop 26.
  When a fall needs the keyboard, guard the keys (`brinkkeys`), not the pilot's wish: a forecast
  of the pilot's keys (`fallseen`) picks ways the keyboard cannot press, and its choice flips each
  command while the key hand still holds the old set toward the pit.
- dm2's big-room lava entries come 3-6 within seconds: the floor trap opening, not the legs.
  Leave them out of a legs' count (the trace's time column shows the bursts).
- A trace with what a guard did each command (se-team's `brink` column: -1 not run, ways, +10 run
  in, +100 changed) showed the brink guard starting ~0.2 s before the edge: read a guard's timing
  before tuning its distances.
- A side sweep off a link on a stair is blocked by the next step: sweep a step and two higher too
  (`nav_falls_in_harm`); and a box stands on an edge until its centre is 16 past it, so a key's
  step must reach ~48 past a line 30 off the edge to see the fall (64, not 40).
