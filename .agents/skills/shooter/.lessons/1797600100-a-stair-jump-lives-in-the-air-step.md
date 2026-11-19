---
title: The stair's double jump lives in the air's step slide, not in the jump
date: 2026-11-14
scope: plugins/shooter/pmove.cpp, plugins/classic/q2_move.cpp, games/shooters_edge/labs/se_remix_physics_lab.cpp
concepts: [classic-remix, cr-physics, stair-jump, double-jump, air-step, pm-airclimb, step-slide]
---

Both moves ADD the jump's 270 to the speed up they carry, so the jump is not where the double
jump differs. Quake II's air move runs the step slide: a ledge met while rising is climbed by a
whole step (18) at once, and below 180 up the player then stands on it with the rise kept, so the
next jump stacks (436 up, about 120 over a 32 ledge). The classic air move only slides (its
`pm_airstep` climbs only within 18 of the ground below), so it must rise past the edge and lands
with little left. `pm_airclimb` runs the ground's step slide in the air; `Q2Move::air_step`
false runs the plain slide. Measure it with a sweep of take-off points (the lab's stair table):
one take-off misses the window of a few units where the climb happens.
