---
title: A real-time lab's timing checks hold on a starved machine, or they flake
date: 2026-10-29
scope: engine/lab/labs/gpu_capture_live_lab.cpp
concepts: [lab, real-time, flaky, frame-pace, sync, flash, stall, smoke, load, runner]
---

`gpu-capture-live` passed alone and failed in three ways once the machine was busy: under `make
precheck` (many labs at once) a frame slipped and a flash landed past one nominal frame; on the
macOS runner's paravirtual GPU the loop starved to 174 ms gaps, a click's flash frames were
dropped, and "the first bright frame after the click" matched the next click's flash 2.5 s
later; eight copies at once preempted the odd capture call past the 25 ms stall bound. The
checks now state the claim, not the timing: every bright frame lies inside some click's flash
window (the sync), each click's flash is looked for only inside its own window, a missing flash
is allowed only when a frame was dropped or the loop slipped by more than a flash, the lag is
bounded by the longest gap the loop measured, and a stall is more than 5% of frames over the
bound, not the single worst. Before pushing a real-time lab, run its smoke flags eight at a
time.
