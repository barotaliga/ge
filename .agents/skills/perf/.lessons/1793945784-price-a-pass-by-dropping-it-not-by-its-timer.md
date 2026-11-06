---
title: On a tiled GPU, price a pass by dropping it from the frame, not by its own timer
date: 2026-11-06
scope: engine/render/clouds.cpp
concepts: [tiled-gpu, pass-timer, gpu-span, experiment, clouds, shafts, overlap]
---

The `weather` lab's per-pass times said the full-size cloud lay-over cost 1.98 ms; dropping the
pass from the frame for one `ladder --price` run moved the top's span by 0.02. On the M3 Max a
pass's own timer counts the passes it overlaps. To find where a frame's time goes, change one
thing in a throwaway edit (return early, skip the pass, return 1 from a helper), price the frame's
span, and restore it: that is how the per-pixel light shafts (1.84 ms of the chase frame's top at
1440p) and the cloud's reprojection (0.4 ms) were found. A row's cost that barely changes between
its stops (the clouds' low and high stops both near 2.9 ms from the chase camera) is a fixed cost
the row switches on, not the stops' own work.
