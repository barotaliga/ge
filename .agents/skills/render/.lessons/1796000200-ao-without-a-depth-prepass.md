---
title: A renderer with no depth prepass takes ambient occlusion after its opaque passes, multiplied into the colour
date: 2026-11-23
scope: engine/render
concepts: [ambient-occlusion, ao, ao-apply, depth-prepass, classic-maps, ce-ao, readback]
---

The AO search needs the depth before anything reads it, and Fall Line's lit passes read it as
`sky_seen` because Fall Line draws its depth first. The classic maps draw colour and depth in one
pass, so their frame runs `ao` after the map and the models and multiplies the colour drawn by the
sky each pixel sees (`encode_ao_apply`, blend dst * src), the sky's pixels (no depth) left alone.
Its pipelines are made at load with the frame's others, never on the first frame it is turned
on: the device counts a pipeline made after `end_load` as an error (a hitch on a real GPU). In a
test, read back a texture of your own made with `CopySrc`: the frame targets have no copy usage,
and the failed copy shows only as a GPU error count, not as a message, unless a sink is attached.
