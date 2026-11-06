---
title: Work that depends only on the camera's ray goes in a volume made once a frame
date: 2026-11-06
scope: engine/render/sky.cpp
concepts: [shafts, shaft-volume, froxel, group-0, binding-16, sparse-layout, compute, haze]
---

The light shafts were the mean of six cloud-shadow reads along the view ray, run in every pixel's
haze (with overdraw), the sky and each cloud layer: 1.84 ms of a 1440p frame on the M3 Max. A
function of the ray and its length alone is a 3D table over the screen's uv and sqrt(distance),
like the aerial volume: `sky.shafts` (128x72x32) keeps the running mean at each slice's end, and a
pixel reads it once. A compute pass that writes a texture group 0 samples cannot bind group 0
itself: give it a layout of its own holding only the bindings its code reads, at group 0's
numbers (the frame block at 0, the cloud's noise at 8 and 9), built by hand since `make_layout`
numbers its slots in order. Check the pictures against the per-pixel version with the lab's
`--compare` over the other build's run copied in as the kept one.
