---
title: A monster names its frame ranges once; the mind's steps show them
date: 2026-11-05
scope: games/shooters_edge/qc/campaign/ai.qc, games/shooters_edge/qc/campaign/monsters*.qc
concepts: [qc, animation, frames, monster, anim-step, frame-range]
---

Frames are data, not a state machine per kind: each spawn function sets `fr_stand`, `fr_walk`,
`fr_run`, `fr_melee`, `fr_missile`, `fr_leap`, `fr_pain` and `fr_death` as '<first> <last> 1'
vectors, read off the model file's own frame names (a scratch reader of the model's frame table
lists them as runs like `run=73-80`). The 0.1 s thinks already exist, so the stand, walk and run
thinks call `frame_loop`, and every attack or pain chain starts with `anim_begin(<range>)` and
steps with `anim_tick()` instead of touching `anim_step` by hand. A new kind or chain that sets
`anim_step` directly shows no frames.
