---
title: The demo game's perf ledger is the evidence for rendering choices; read it before picking a technique
date: 2026-10-21
scope: docs/rendering.md, porting/rendering-survey.md
concepts: [survey, gpu-driven, shadows, pipelines, terrain, budget]
---

The rendering plan was chosen against measured costs, not a list of popular techniques.
The original renderer's optimisation ledger (summarised in `porting/rendering-survey.md`)
showed four costs that dominate: pipelines compiled mid-run, a sun shadow pass that
redrew the scene, CPU instance selection for tens of thousands of trees, and the
ground's fragment shader on tile GPUs (with `discard`). Each technique in
`docs/rendering.md` answers one of those. Before adding or swapping a technique, find the
measured cost it removes; techniques that answer no measured cost (virtual shadow maps,
a visibility buffer, deferred shading) are listed as not taken, with the reason.
