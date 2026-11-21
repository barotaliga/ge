---
title: A lab that moves a body in a staged FxScene turns off the model lerp
date: 2026-11-21
scope: games/shooters_edge/labs/gpu_se_blood_lab.cpp
concepts: [lab, fx-scene, step-fx, lerp, model-lerp, staged, walk, placed, origin]
---

`step_fx` draws every placed model between the listen server's states (`lerp_models` on
`FxScene::clock`). A lab that builds its own scene and moves a body by hand (`scene.models[0].origin`
each frame) never advances that clock, so the body stays where it was first seen and anything that
reads `FxFrame::models` (the wounds, the prints) sees it still. Set `fx_state.on.lerp = {}` in such a
lab. Also check the floor along a staged walk (`floor_at` every 8 units): dm3's first way off ran
off a ledge and the feet's floor trace missed from there on.
