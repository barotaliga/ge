---
title: A shadow caster is placed by how it moves, a strip on the snow follows the ground every two metres, and a GPU lab's shots match without an adapter
date: 2026-10-22
scope: engine/render/shadows.hpp, games/fall_line/app/frame.cpp, plugins/mountain/app/village_look.cpp
concepts: [shadows, cascades, caster-slots, still, moving, street-mesh, z-fighting, lab, digest, lavapipe]
---

- What stands casts into the cached cascades (`still`, `kCascades` slots), what moves into the
  atlas (`moving`, `kMaxShadowFigures`, culled per tile), a flat sheet into neither. Caster slots
  are made before `bind_family_hiz`. A lab showing a moving caster's shadow stands a figure beside
  it, or no tile draws it.
- A flat strip laid on the terrain takes heights every 2 m; at 6 m the ground's triangles bulged
  through a car park.
- A family made after load creates no pipeline, but a second `create_frame` on one device does:
  a lab's before and after change the one frame.
- Without `mesa-vulkan-drivers` a GPU lab draws black with no error. Its no-adapter branch owes
  the same digest (the `labs` skill, § Digests and determinism).
