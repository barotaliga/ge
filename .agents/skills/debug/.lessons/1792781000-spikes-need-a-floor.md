---
title: A robust z-score alone calls every step of a smooth signal a spike
date: 2026-10-22
scope: engine/lab/stats.cpp, engine/lab/chart.cpp
concepts: [spike, outlier, mad, rolling-median, smooth-signal, noise-floor, ranking]
---

The first `trace` run flagged 9385 of 28763 steps of the skier's speed as spikes. A 120 Hz
signal is so smooth that its deviations from a rolling median are all near zero, so their MAD is
tiny and any wobble is many robust sigmas out. `spikes` now floors the scale at 1% of the
series' p1..p99 range, which left the speed with none and the edge with its real snaps. A
noisy signal (the sideways load chatters every step) still flags thousands, so findings are
sorted strongest first and a chart rings the first 60: rank, never take the first N by index,
or every ring lands in the run's first seconds. `make lab LAB=charts` holds the rule to its
planted outliers after any change.
