---
title: A real-time lab bounds a timing check by the pace it measured, not the nominal one
date: 2026-10-29
scope: engine/lab/labs/gpu_capture_live_lab.cpp
concepts: [lab, real-time, flaky, frame-pace, sync, flash, smoke, load]
---

`gpu-capture-live` held each click's flash to "within one frame" using 1000/fps. Alone it passed
every time; under `make precheck`, which runs many labs at once, a frame slipped a few ms and a
flash landed 37 ms after its click at 30 fps, one ms over the bound. The flash is drawn on the
first frame stamped after the click, so the honest bound is the longest gap the frame loop
actually had (`frame-interval.max`), never less than one nominal frame. Bound every wall-clock
check in a lab by what the run measured, and run the smoke flags six at a time before pushing.
