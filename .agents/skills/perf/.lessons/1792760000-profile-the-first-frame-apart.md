---
title: Report the first frame after a cut apart from the steady frames
date: 2026-10-22
scope: games/fall_line/labs/gpu_profile_lab.cpp
concepts: [profile, cpu-half, budget, first-frame, cut, cascades, median, p95]
---

The first frame after a cut redraws both shadow cascades, decides the horizon's bakes and
uploads afresh, and costs several times a steady frame on the CPU (3.6 ms against a 0.24 ms
median on seed 38). Counted with the rest, it alone breaks a 1 ms budget and hides what a
steady frame costs. The `profile` lab reports it as `cpu-ms.first` and judges the budget on the
frames after it. Judge the CPU half on a release build with validation off: the debug and
sanitizer rows, and Dawn's validation, are several times slower.
