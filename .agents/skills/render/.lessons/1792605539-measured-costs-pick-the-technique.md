---
title: Measured costs pick a rendering technique; read the cost survey before choosing one
date: 2026-10-21
scope: docs/rendering.md
concepts: [survey, gpu-driven, shadows, pipelines, terrain, budget]
---

The rendering plan was chosen against measured costs of the scene, not a list of popular
techniques. Four costs dominate: pipelines compiled mid-run, a sun shadow pass that
redraws the scene, CPU instance selection for tens of thousands of trees, and the
ground's fragment shader on tile GPUs (worse with `discard`). Each technique in
`docs/rendering.md` answers one of those. Before adding or swapping a technique, name the
measured cost it removes; techniques that answer no measured cost (virtual shadow maps,
a visibility buffer, deferred shading) are listed as not taken, with the reason. While
porting, the measurements behind the four costs are in the `porting/` working area.
