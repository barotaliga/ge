---
title: A hop's wall look is in seconds; at 600+ it must be longer, and both arcs walled is no rule
date: 2026-11-18
scope: plugins/shooterbots/pilot.cpp, plugins/shooterbots/fighter_setup.cpp, games/shooters_edge/labs/hop_course.hpp
concepts: [bunny-hop, hopsee, hop-see-s, arc-clear, corners, doorway, room-exit, avghop, hop-simple, se-bhop]
---

The hop's side switch away from a wall (`arc_clear` over `hop_see_s`) and its aim (`hop_look_s`)
are both in seconds. At 650-700 units/s on the hop course the 0.12 s look saw a corner's outer
wall about 0.1 s before meeting it, too late for the air's ~190 degrees/s. 0.25 s (`hopsee`)
fixed the corners (expert walls 2.2 -> 0.5 a lap); 0.3 s weaved 3.4 reversals a second and 0.4 s
thrashed (9.9): a longer look calls a wall in both arcs and switches on every command. A longer
AIM (0.4-0.5 s) did nothing for the walls. When both arcs are walled (a room's side exit, the
walls beside the mouth), neither the clearer arc nor the side toward the aim (`doorway`, dropped:
seed 2 out 280 -> 443, seed 1 714 -> 320) is a rule: the weave was already too wide before it.
The average's simple hop (`hop_simple`) lost to walls because its view ran past the velocity
after each swap (the hand's turn-back at ease waits for 90 % of a stroke) and the air keys went
to none: flying the expert's hop instead (`avghop`) took its lap 35 -> 25 s. The lab's trace has
the hop's aim, side and air set (`aim`, `hop_side`, `air_set`) and its picture the nav's route:
read those before guessing a hop's fault.
