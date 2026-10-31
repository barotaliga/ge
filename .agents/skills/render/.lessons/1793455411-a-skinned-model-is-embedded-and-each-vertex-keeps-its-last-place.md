---
title: A skinned model is embedded in the binary, posed only when drawn, and each vertex keeps its own last place
date: 2026-10-22
scope: games/fall_line/app, engine/render/skinning.hpp, engine/render/skinning.cpp, engine/render/skinning.wgsl
concepts: [skinning, embed, glb, poser, reach-box, motion-vectors, taa, settle]
---

- A game's model goes in with `ge_embed_binary` and is read by `assets::load_gltf(bytes, name)`,
  so the window and labs run from any directory. A skinned family holds a mesh and a `Skin` a
  racer, and the frame poses only those drawn (`encode_skinning`). Keep the reach fixed and held by
  a test (`skier::pose_reach`); a reach measured off one pose clamps the next.
- A vertex rewritten on the GPU copies its old position out before writing (each thread owns its
  vertex), or TAA smears motion inside the mesh. A settle flag on the palette upload starts a cut,
  a still or an undrawn slot with no motion; a lab still passes `settle` to every poser.
- Prove it with the `taa` lab's `--scenario=turns`, the "before" being the settle forced on every
  frame.
