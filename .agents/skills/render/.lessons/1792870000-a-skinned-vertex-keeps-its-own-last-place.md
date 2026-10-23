---
title: A vertex rewritten on the GPU keeps its own last place for the motion target
date: 2026-10-23
scope: engine/render/skinning
concepts: [skinning, motion-vectors, taa, ghosting, last-position, settle]
---

An instance's motion comes from its last place and rotation, so a mesh rewritten every frame (a
skinned figure) must also keep each vertex's last position, or TAA smears what moves inside the
mesh. The pass that rewrites a vertex copies the old position out before writing (each thread
owns its vertex, so there is no race), and the draw reads it only for the LODs that keep one. A
settle flag rides on the palette's upload, so a cut, a still, or a slot that was not drawn starts
with no motion. Measure it with a picture: the `taa` lab's `--scenario=turns` chases the skier,
and a "before" is the same build with the settle forced on every frame.
