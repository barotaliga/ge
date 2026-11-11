---
title: A GPU cost lab waits for each frame, warms 30 frames and times each half in the same process
date: 2026-11-10
scope: games/shooters_edge/labs/gpu_se_burn_lab.cpp
concepts: [gpu-cost, timestamps, on-submitted-work-done, warm-up, a-b, cvars, se-burn]
---

`cl::record_span` reads nothing until the frame's work has landed: wait on
`queue.OnSubmittedWorkDone` after each `draw_world`, and draw about 30 frames before the first
timed one (the eye's adaptation and the timers settle). Toggle the feature's cvars between timed
runs in the same process (off, on, each half alone) rather than comparing two runs: the container's
load moves a whole frame by 10 % between runs. A lab arg named `--out` collides with the runner's
output folder: name it something else (`--after`).
