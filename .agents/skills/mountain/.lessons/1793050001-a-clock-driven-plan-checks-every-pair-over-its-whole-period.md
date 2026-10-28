---
title: Traffic as a pure function of the clock is checked over its whole period, not at a few moments
date: 2026-10-24
scope: plugins/mountain/village/traffic.cpp
concepts: [traffic, period, phase, overlap, sample, pure-function, clock, test]
---

The village's vehicles are placed at phases whose whole period meets no vehicle placed before
them, so a test at a handful of moments proves little: a meeting at a corner lasts a second in a
600 s period. `tests/mountain_traffic_test.cpp` and the `traffic` lab sample every vehicle every
half second over the whole period and check each pair, and each against every parked car, by the
separating axes of their bodies. That costs some 30 s a seed in the debug build, so the test
builds each seed's map and plan once and runs the full sweep on two seeds only. A car and a
cyclist on one lane are left out of the plan's check because the car swings out round him
(`vehicle_at`), and the check on the drawn bodies is what proves the swing is wide enough.
