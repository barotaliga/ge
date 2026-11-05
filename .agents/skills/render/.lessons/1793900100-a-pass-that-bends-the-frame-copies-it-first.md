---
title: A pass that bends the frame reads a copy of it, and judges itself by a diff against itself off
date: 2026-11-05
scope: engine/render/haze
concepts: [heat-haze, refraction, scene-copy, depth-read-only, ribbon, before-after, subtle-effect]
---

A pass that shows the frame displaced (heat haze, a liquid's shimmer) cannot sample the target it
draws into: copy the colour first (`haze_copy.wgsl`, a full-screen triangle into a texture of the
targets' size, rebound on resize), then draw the ribbons into the HDR target with the depth
attached `depthReadOnly`, which lets the same depth view be bound as a texture for the "is what I
pull from nearer than the air" test. Keep the copy's shader in its own file: two group-0
declarations in one module (the frame's and the copy's) collide. An effect a few milliradians
strong is invisible to the eye in a still and to a picture digest, so prove it by rendering the
same lab run with the pass's encode skipped and diffing (pixels changed by more than 3 levels: the
nail's thread 89, then 267 after the bend went from 1.4 to 2.5 mrad, a rocket's wake 3181 at
960 x 540); a straight-away shot from the eye shows nothing, so look from the side camera.
