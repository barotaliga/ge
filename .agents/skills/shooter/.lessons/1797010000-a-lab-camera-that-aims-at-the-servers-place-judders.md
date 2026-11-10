---
title: A lab camera aimed at the server's place films judder the game does not have
date: 2026-11-09
scope: games/shooters_edge/labs/gpu_se_model_lerp_lab.cpp, games/shooters_edge/client/present.cpp, games/shooters_edge/client/demo_lerp.cpp, games/shooters_edge/client/held.cpp
concepts: [model-lerp, lab-camera, aim-judder, eye-lerp, demo-strides, weapon-frame, track-frame]
---

The first `se-model-lerp` film looked "jittery when targeting" because the harness pointed its
camera at the monster's stepped server origin while the model was drawn smoothly: the two
disagreed every stride. Aim a filming camera at what is DRAWN (the lerped place), eased, never at
the simulation's state. The same mistake lived in the game: the player's own eye read the newest
72 Hz state, so `frame_view` now draws it one server frame behind like the models. Two more paths
had their own step: a played demo's entities are drawn between frames 14 ms apart, so a monster's
stride needs the stride rule there too (`DemoLerp::stride`), and the recorded weapon frame
(playerinfo bit 0x100, multiview 1024, single client data) was read and dropped; `track_frame`
is the one reusable "frame drawn between its changes" helper for any such number.
