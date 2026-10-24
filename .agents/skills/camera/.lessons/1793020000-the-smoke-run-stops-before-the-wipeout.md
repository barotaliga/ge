---
title: The smoke run stops before the wipeout; run the lab whole when the body changes
date: 2026-10-23
scope: games/fall_line/labs/gpu_camera_world_lab.cpp, games/fall_line/camera/subject.cpp
concepts: [camera-world, smoke-flags, wipeout, ragdoll, trunk, worn-lens, regression]
---

`camera-world`'s smoke flags run the woods for 3 s, and the skier meets the trunk at 4.3 s, so
`make labs-smoke` never sees him go down. When the thrown body landed, a wiped-out skier was
thrown into the trunk he met and the helmet lens followed his head 0.05 m into the bark; the
smoke digest stayed green and only the full run failed. After any change to how the skier falls
or where his body goes, run `make lab LAB=camera-world` with its default flags, not only the
smoke. The fix is the frame's: keep every lens `kBarkClear` off any trunk it stands beside, as
it is kept over the snow.
