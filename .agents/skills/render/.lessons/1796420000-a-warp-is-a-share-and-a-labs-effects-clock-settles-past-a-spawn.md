---
title: A spawn or a teleport is one share on the draw; its ash lands on the skin and each flake goes its own way
date: 2026-11-05
scope: engine/render/wgsl/warp.wgsl, engine/render/morph.cpp, engine/render/viewmodel.wgsl, games/shooters_edge/app/effects.cpp, games/shooters_edge/client/world_fx.cpp, games/shooters_edge/labs/warp_film.hpp, games/shooters_edge/labs/gpu_se_models_lab.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [warp, dissolve, dust, ash, spawn, teleport, value-noise, wgsl-precedence, fx-clock, lab-fixture, particle-light, first-person]
---

- A figure's arrival or departure is one number on its draw (`MorphWarp::shown`); the game times it. Fix the dissolve's noise to the model's own frame so it stays on the body, and spread two octaves of value noise to 0..1 (`(n - 0.2) / 0.6`) or nothing shows for a third of the way. The flakes read the same field on the CPU (`warp_field`), so they leave and land on the cut.
- Ash that lands on the body's box, not its skin, reads as a cloud beside it: keep the morph frames on the CPU (`WorldModels::shapes`, `body_triangles`). Flakes that share a velocity, drag and life move as one block: give each its own way round the wind, speed, drag, buoyancy (a small negative `fall` holds one up), size and life, and set arriving ones off 0.1 to 0.5 s before they land, frame by frame, so the time thrown is the same at any frame rate.
- The particle pool lights every particle by the open sky, indoors too: ash reads blue-white in a dark room unless its albedo is near black (0.02 to 0.07). First person, the body's own points sit at or below the eye: land the ash in the air ahead of the eyes instead, and keep every path clear of the eye.
- WGSL refuses `a * b ^ c`; bracket a hash's products. Labs that call `step_fx` on a fresh listen server now see the player spawn: settle the effects clock past it (`se-models`' `kFxSettle`) or drop the births gathered over it (`se-client`). `se-models`' fixture had no `progs/player.mdl`; it stands in now.
- An enhanced effect keeps the original beside it: `ce_warps` in the modern look turns the dust on, and off (or classic) the warp throws the teleport's fog at each end with the figure there or gone at once. A campaign's program throws its own fog: add ours only where no program does, and leave the program's out where the dust replaces it.
