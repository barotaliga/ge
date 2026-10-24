---
title: Fire a scripted lab event on a whole frame, never by comparing accumulated time
date: 2026-10-24
scope: engine/lab/labs/water_audition_lab.cpp
concepts: [lab, scenario, script, frames, float-time]
---

The swim scene fired a stroke when `t >= next && t - 1/60 < next` with `t = i / 60.0` and
`next = 0.6 + 1.1 * k`: at k = 2, `next` rounds to 2.8000000000000003 and `169/60 - 1/60` to
the same, so the stroke never fired and every later one was skipped with it. The picture showed
two strokes where five were scripted. Schedule a scripted event by frame index
(`(i - 36) % 66 == 0`), and look at the picture before trusting a count.
