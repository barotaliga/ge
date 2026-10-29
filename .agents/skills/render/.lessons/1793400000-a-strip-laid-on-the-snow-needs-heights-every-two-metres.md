---
title: A strip laid on the snow needs its heights every couple of metres, and a family added late resets the cascades
date: 2026-10-28
scope: plugins/mountain/app/village_look.cpp, games/fall_line/app/frame.cpp
concepts: [street-mesh, decal, terrain, z-fighting, lift, shadows, cascades, reset-sun-shadows, felled, instance-family, end-load]
---

A flat strip laid over the terrain at its two ends shows the snow through it wherever the
terrain's own triangles bulge between them: at 6 m runs and a 6 cm lift the car park was dotted
with white wedges; at 2 m runs none show, at 17 000 triangles for a village. The sun's cascades
are cached for what stands still, so anything that changes the forest after the first frame (the
felled trees) must call `reset-sun-shadows`, or the trees' shadows stay on the empty street. A
family made after the frame's load (`create_family`, `bind_family_hiz`) creates no pipeline, but a
second `create_frame` on the same device does: a lab that wants a before and an after films the
before, then changes the one frame. Without `mesa-vulkan-drivers` a GPU lab's pictures are black
with no error; install it before trusting a dark picture. A shot's id is in the lab's digest even when its pixels are
measured, so the no-adapter branch (Windows CI has none) must take the same shot ids as the drawn
one, `-before` shots included, or the digest differs only there.
