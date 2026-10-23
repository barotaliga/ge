---
title: A model's mesh nodes load as joints, so a level of detail adds a joint
date: 2026-10-23
scope: engine/assets/gltf.cpp, engine/lab/model_sheets.cpp, plugins/skier/assets/skier/build.py
concepts: [lod, joints, mesh-node, gltf, palette, detail]
---

The loader reads every node into `Model::joints`, the mesh objects included, so the skier went
from 28 joints to 30 when its builder added `skier.lod1` and `skier.lod2`. A palette then carries
two more rows, and anything that walks the skeleton's leaves meets the mesh nodes. The
`model-views` and `skier-views` detail sheets picked them as close-ups until `detail_joints` left
out every joint named after a mesh.

When a builder adds a mesh, check what walks `model.joints` (the sheets, the poser's binding, the
palette's size) and the lab digests that read the model. Build a level of detail as its own mesh
off the same rig, never as a decimated copy, so its weights are the builder's and its skis still
stand on the snow (`skier_rig_test` checks every level).
