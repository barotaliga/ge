---
title: Every mode change of a kinematic actor starts where the last mode left him
date: 2026-10-23
scope: games/fall_line/sim/crowd.cpp, games/fall_line/sim/crowd_lift.cpp, games/fall_line/labs/crowd_lab.cpp
concepts: [teleport, jump, handoff, kinematic, crowd, lift, queue, merge, lab-metric]
---

An actor kept as (run, s, d) or on a carrier is placed by each mode's own rule, and a handoff
between two modes is where he jumps: the queue's spot against the carrier's seat (20 m), a run's
end against the next run's nearest point (1 m), the let-off point against where the carrier
really is (2 m), the ramp's foot against the run's snow (11 m). None of it shows in a table of
counts or speeds.

Give the lab a `jumps` metric (the largest move in one step beyond what the speed allows) and
fail on it before tuning anything else. Then fix each handoff at its source: hang the queue off
the load point the carriers really pass, merge at `nearest_track_point` and ease the lateral in,
let a rider off where he is, and add a last skating leg onto the run's snow.
