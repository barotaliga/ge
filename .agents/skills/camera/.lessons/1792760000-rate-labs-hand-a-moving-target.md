---
title: A frame-rate lab hands a follower a moving target, never a step
date: 2026-10-23
scope: engine/lab/labs/camera_follow_lab.cpp, engine/camera/follow.cpp
concepts: [camera-follow, frame-rate, follower, step, sampling, exact]
---

Comparing a follower at 30 Hz against 240 Hz with a STEP measures the step's sampling (where
the jump lands inside a frame, up to a frame of delay), not the integration: the first runs
failed every case for that reason alone. Count time in whole frames (`tick / hz`, never `t +=
dt`), and hand a smooth rise and a ramp, which is what a camera's target does (a teleport snaps
instead). With that, the lab's real finding showed: a semi-implicit step with a clamped mass
strayed by a fifth of a step at 30 Hz; the exact per-frame solution with the target taken as a
line across the frame is within 1.3 %.
