---
title: A single-player program leaves a model file's size to setmodel; the multiplayer one sets it
date: 2026-11-09
scope: games/shooters_edge/server/bi_world.cpp, games/shooters_edge/qc/campaign/set_pieces.qc
concepts: [setmodel, setsize, explobox, model-bounds, single-player, brush-model-file, pointlike, traceline]
---

The e1m1 exploding box could not be shot: the pack's single-player program calls `setmodel` on
`maps/b_explob.bsp` and never `setsize`, trusting the engine to size it from the file, while our
`setmodel` (the multiplayer rule) sized only inline `*N` models. A zero-size box is a point to a
line trace, so every pellet missed, and the test that called `damage` directly still passed.
`setmodel` now asks `QcServerOptions::model_bounds` for a brush model file's bounds under a
single-player program only (one unit wider each side); a multiplayer program sets such sizes
itself and keeps the multiplayer rule. Test a shootable thing by SHOOTING it (a frame with the
attack button), never only by calling the program's `damage`.
