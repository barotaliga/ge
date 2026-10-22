---
title: A series that never moves calls its own rounding a spike
date: 2026-10-22
scope: engine/lab/stats.cpp, engine/lab/anim.cpp
concepts: [spike, flat-series, rounding, noise-floor, joint-angle]
---

The first `anim` run charted the knees' angles through the turns scenario: they never move
(the placeholder figure bends only with the tuck), yet the chart rang a hundred spikes. The
angle is read off the posed pieces to about 1e-13, so the series has no p1..p99 range, the 1%
floor is zero and the rounding is many robust sigmas out. `spikes` now also floors the scale at
a millionth of the series' largest value. Any measure computed off a pose (an angle, a bone's
length) has this rounding; test a new measure on a figure that holds still.
