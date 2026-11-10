---
title: A hand-built face is wound by where it looks, a caster is placed by how it moves, and a skinned model is embedded and posed only when drawn
date: 2026-10-22
scope: engine/render/shadows.hpp, engine/render/skinning.hpp, engine/render/skinning.cpp, engine/render/instances.wgsl, plugins/mountain/app/building_look.cpp, plugins/mountain/app/village_look.cpp, games/shooters_edge/view/weapons.cpp, games/fall_line/app, games/fall_line/labs/gpu_camera_world_lab.cpp
concepts: [mesh, winding, add-triangle, roof, viewmodel, mount, framing, dither, shadows, cascades, caster-slots, street-mesh, skinning, embed, glb, reach-box]
---

- `render::add_triangle` takes the normal from the corners' order; `building_look.cpp`'s `face`
  takes the direction it should look. A roof needs its soffit; look from below too.
- A kit of guns needs a held length and forward offset each (0.30-0.44 m), tuned by the placed
  box's screen share. A dithered occluder fade is measured off the depth target and kept in the
  prepass under the LOD dither.
- What stands casts into the cached cascades (`still`), what moves into the atlas (`moving`,
  culled per tile), a flat sheet into neither; caster slots come before `bind_family_hiz`. A lab
  showing a moving caster's shadow stands a figure beside it, or no tile draws it.
- A flat strip on the terrain takes heights every 2 m (at 6 m the ground bulged through).
- A second `create_frame` on one device creates pipelines: a lab's before and after change one
  frame. Without `mesa-vulkan-drivers` a GPU lab draws black with no error.
- A game's model is embedded (`ge_embed_binary`, `assets::load_gltf(bytes, name)`); a skinned
  family poses only what is drawn, with a fixed reach held by a test (`skier::pose_reach`). Prove
  motion with the `taa` lab's `--scenario=turns` against the settle forced every frame.
