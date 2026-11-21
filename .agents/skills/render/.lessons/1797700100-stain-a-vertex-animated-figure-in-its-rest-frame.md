---
title: Stain a vertex-animated figure in its rest frame, not its posed one
date: 2026-11-21
scope: engine/render/morph.wgsl
concepts: [morph, blood, wound, rest-frame, frame-0, stain, vertex-animation, local]
---

A mark on a morph figure (blood soaked into its clothes) placed by the posed point (`in.local`)
slides over the cloth as the frames move the limbs. The vertex stage reads the same vertex's frame
0 (`vertices[2 * (first_vertex + idx)]`, times the draw's scale) and passes it beside the posed
point; a wound placed by its triangle and barycentrics in frame 0 (Shooter's Edge's
`body_rest`) then stays on the same cloth. Down in that frame is -y (models are stored in the
engine's frame), good enough for a run of blood on a standing figure. On the classic player the
gun sits in front of the chest, so a line from the front often wounds the gun first: test with
hits spread down to the legs.
