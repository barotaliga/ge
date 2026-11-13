---
title: A trace that starts inside a wall reports a clear line; check start_solid
date: 2026-11-13
scope: games/shooters_edge/labs/gpu_se_sequel_monster_fx_lab.cpp
concepts: [camera-placement, line-trace, start-solid, film-a-monster]
---

A lab that places its camera by testing lines of sight with `ge::brush::BrushTracer` took points
inside a wall as clear: a trace whose start is in solid can come back with fraction 1 and
`start_solid` or `all_solid` set. The films showed nothing but a close wall. Treat a line as
clear only when `fraction >= 1 && !start_solid && !all_solid`, and also ask for room round the
camera (a short trace along each axis) so it does not sit against a face.
