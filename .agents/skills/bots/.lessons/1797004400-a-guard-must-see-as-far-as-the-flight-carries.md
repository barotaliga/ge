---
title: A guard must see as far as the flight carries, and its model must slide along walls as the move does
date: 2026-11-14
scope: plugins/shooter/brink.cpp, games/shooters_edge/bots/fighter.cpp, plugins/shooter/features.cpp
concepts: [brink, brinkfall, fall-into-harm, lava, stair, air-brake, wall-slide, look-ahead, dm2, ya-stair, keyboard-lag]
---

- A guard gated on "near the danger" never runs during the flight that carries the bot into it: dm2's
  YA walk-ins flew 0.4 s off the stair's top step, too far across from the lava's nodes, and
  landed on a lower step's lip at 340 u/s, where the ground's brake is 10 units short. Gate the
  look on the reach of the flight (speed x 0.6 s), not on where the bot stands.
- The air brakes a run at the full acceleration (~20 units from 360), so a fall seen BEFORE the
  lip is a fall stopped; one seen over the lip is a death. Fly the box (gravity, landings, the run
  on after one) instead of looking a fixed distance along the floor.
- A predicted path that stops dead at a wall misses the real move's slide: a hop into the pillar
  beside the lava slid along its face off the step. A model of the move needs the move's clip
  along the plane, or it reads every wall as safety.
- The keyboard's hand lags the guard's wish by ~0.15 s (a person's): a guard under the keyboard
  must fire that much earlier, which a look along the flight gives for free.
- Read the first set's deaths row by row before tuning: the first build moved nothing (33 -> 35),
  and two traced rows named both holes (the slide, the lag) in minutes. Then judge on one build,
  base and feature side by side: the base reproduced the old set to the death, so 33 -> 3 is clean.
