---
title: A skeletal model draws through the morph pass as baked frames, not a second pipeline
date: 2026-11-06
scope: plugins/morph/skeletal.cpp, games/shooters_edge/client/classic_adapter.cpp
concepts: [skeletal-model, iqm, bake, frame-group, morph-pass, atlas]
---

The window's models (lighting off the floor, team tints, warps, glows, the nearest-N cull) all
live in the morph pass. A mod's skeletal model drawn through the GPU skinning pass would need all
of that again; posing it on the CPU through each clip into morph frames (`bake_skeletal`, a clip
a frame group at 20 a second, a looping clip's repeated end left off) gets every one for free,
and the pass's frame blending hides the rate. A morph model has one skin, so the materials'
pictures are stacked into one atlas and each mesh's coordinates moved into its rect. Sniff the
first bytes as well as the name: a mod may ship a skeletal model under a `.mdl` name, and the
`se-models` lab's `--models=iqm` relies on it.
