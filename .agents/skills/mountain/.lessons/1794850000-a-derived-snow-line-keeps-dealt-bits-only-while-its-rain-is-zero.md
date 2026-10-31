---
title: A snow line read off the air keeps a dealt map's fall only while its rain share is exactly 0
date: 2026-10-31
scope: plugins/mountain/air/air.cpp
concepts: [snowline-at, snow-level, fall-at, rain-share, golden, digest]
---

`fall_at` now reads every map's snow line off its day of air (`snowline_at`) instead of a line
only a picked sky set. A dealt winter map keeps its bits because the line lies far under its base,
so `rain_share_at` is smooth's exact 0 and `see_through_rain` returns before touching the visual
range. A change that warms the day (the climate's mean, its swing, a lapse) can put a dealt map's
base under the line and move the `precip` and fog labs' digests and `mountain_air`'s golden: read
`<sky>.snowline` (m over the eye) in the `precip` lab before and after.
