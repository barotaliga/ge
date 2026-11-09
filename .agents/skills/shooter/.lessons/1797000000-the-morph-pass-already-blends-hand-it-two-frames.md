---
title: Smooth model animation is a game-side job; the morph pass already blends two frames
date: 2026-11-09
scope: games/shooters_edge/client/model_lerp.cpp, games/shooters_edge/client/world_models.cpp, games/shooters_edge/client/world_fx.cpp
concepts: [model-lerp, r-lerpmodels, r-lerpmove, frame-lerp, step-lerp, morph, jittery, monster-steps]
---

"Models move jittery" was never the renderer: `MorphPose` has `frame_a`, `frame_b` and `blend`,
and the world's models always set both frames to the same one. The fix lives in one place every
path passes (`step_fx`, after `out.models = scene.models`), so the listen server, a played demo and a
joined server all get it, and the shadows, wounds and warps read the same pose. Two pitfalls: a
72 Hz server's "tenth of a second" is 7 or 8 server frames (0.097 or 0.111 s), so a blend capped at
0.1 s holds or jumps once a stride (`kMostFrameSpan` 0.15, a longer gap reuses the last pace); and a
helper that writes the drawn place into the same track it reads (`drawn_place(tr, ..., tr.step_origin)`)
jumps a whole stride. The headless check is `se-model-lerp` (judder and pose jumps); the GIF needs
`--game=<dir>/id1` and the pack's progs for a monster.
