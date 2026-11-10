---
title: Draw a played demo from a copy between two frames, never by writing the reader's state
date: 2026-11-08
scope: games/shooters_edge/client/demo_lerp.cpp, games/shooters_edge/client/timedemo.cpp
concepts: [demo, lerp, interpolation, smooth, playdemo, demovideo, delta, multiview]
---

A played demo looked jerky because each picture showed the newest frame read: 77 recorded frames
a second fall unevenly on 60 or 30 pictures. Smoothing it means drawing between the last two
frames, and the tempting shortcut is to write the in-between origins into `Demo::state`. Don't:
the next frame's players and entities are deltas read against that state, so the reader would
drift. `DemoLerp` keeps the moving parts of the last two frames (`lerp_frame` after every read)
and `lerp_at` builds `drawn` as a copy with them replaced. In the video the picture's demo time is
`read.time - (vt - at)` (the timeline runs ahead of the picture by that much); the window uses the
paced clock. Measure with `se-demo-smooth --file=<demo>`: the judder (mean change of the eye's
step picture to picture) is the number that drops, not the mean step.
