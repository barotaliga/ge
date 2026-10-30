---
title: A very bright, thin light gets a black rim from the TAA and the sharpening, not from itself
date: 2026-10-30
scope: engine/render/taa.wgsl, engine/render/post.wgsl, engine/render/bolts.wgsl
concepts: [taa, history, clip-box, sharpen, hdr, black-rim, lightning, reactive]
---

A lightning channel thousands of times brighter than the night came out with a dark rim. The
additive pass was not the cause. Two filters were. The TAA blended the last frame's dark into
pixels beside the channel, because a neighbourhood holding the channel makes the clip box too wide
to clip anything. The post's sharpening applied a tiny negative lobe in linear light, enough beside
such a light to pull a pixel to black. Find the culprit by turning filters off one at a time and
printing a row of pixels across the edge against the same row without the light. The fixes: a
frame that a flash lights takes no history, and sharpening works on light squashed into 0..1. Draw
a thin light with a soft halo capped on screen, never as a hard-edged core alone.
