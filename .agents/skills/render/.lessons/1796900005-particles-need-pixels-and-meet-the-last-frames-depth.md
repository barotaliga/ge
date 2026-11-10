---
title: A small particle needs a pixel's width and the reactive mask, collides with the last frame's depth, and each flake goes its own way
date: 2026-10-22
scope: engine/render/particles_sim.wgsl, engine/render/particles.wgsl, engine/render/precip.wgsl, plugins/mountain/app/snow.wgsl, engine/render/bloom.hpp, engine/render/wgsl/warp.wgsl, engine/render/morph.cpp, games/shooters_edge/client/world_draw.cpp, games/shooters_edge/client/world_fx.cpp, games/shooters_edge/labs/gpu_se_models_lab.cpp
concepts: [particles, spray, snowfall, sub-pixel, glitter, reactive, bloom, collision, depth-buffer, soft-particles, warp, dissolve, ash, spawn, fx-clock]
---

- Spray is born under the snow where skis sink: kill a particle at the ground only when it falls.
  Check a pool's live count beside its births before tuning rates.
- A 1-2 cm flake is under a pixel past a few metres: widen the quad to about 1.5 px in the vertex
  stage and divide its alpha by the same factor. A glint on sunlit snow needs about 20 times the
  first flare, measured by contrast against its eight neighbours, and marked reactive or TAA-U
  averages it away. A chain whose length follows a target's size carries its own count (bloom).
- The particle sim runs before drawing, so the depth bound is the last frame's: keep the camera
  that drew it. Prove bounces by counting them (stills hide sparks in smoke). A blast point lies
  inside the floor; a lift is a brush entity, so trace with `trace_move`.
- A speck meant to sit ON a surface is lifted toward the eye past the soft band; a burn darkens by
  the mark's relief (`BurnVertex::relief`), not its cover, or a bullet hole is a flat disc.
- A spawn or teleport is one share on the draw (`MorphWarp::shown`); its noise is fixed to the
  model's frame and spread to 0..1, and the CPU reads the same field (`warp_field`). Ash lands on
  the skin (`body_triangles`), each flake with its own wind, drag, size and life, near black
  albedo indoors; first person, land it ahead of the eyes. Labs on a fresh listen server settle
  the effects clock past the spawn (`kFxSettle`). An enhanced effect keeps the original beside it.
