---
title: After a depth prepass, an equal-tested pass shades coincident surfaces twice; count with a tolerance
date: 2026-10-21
scope: engine/render/terrain.cpp
concepts: [depth-prepass, equal-test, overdraw, clipmap, invariant, coincident-geometry]
---

`@invariant` on the position makes the prepass and the colour pass compute the same depth bits,
so every covered pixel passes `equal` at least once. It does not make it pass only once: where
two clipmap rings overlap at a ring's edge, two triangles land on the very same depth and both
pass. The `world` lab's overdraw probe measured 1.0002 shaded a covered pixel on seed 38's
lookback and vista views (exact 1 on the small synthetic hills of the tests), so its check is
"at least 1, at most 1.001". A count UNDER 1 is the real fault: a pixel the colour pass missed
(a vertex stage that differs between the two pipelines).
