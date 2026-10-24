---
title: Skipping three pixels in four inside one pass saves nothing; give the pixels that work a pass of their own
date: 2026-10-24
scope: engine/render/clouds.wgsl
concepts: [clouds, temporal, reprojection, spread-over-frames, divergence, lanes, checkerboard, perf]
---

The cloud march was first spread over frames inside the half-size pass itself: each pixel of a
two by two marched on its own frame and the other three read the last frame's picture. The
weather lab timed it no cheaper than marching every pixel, because a GPU (and lavapipe, which
runs a fragment shader eight or sixteen pixels to a SIMD group) runs a group's branch as long as
its slowest lane: with one marching pixel in every two by two, every group marches. The fix is
the layout, not the shader: the pixels whose turn it is march in a pass of their own over a
target a quarter of the half-size one, side by side, and the half-size pass only gathers (the
fresh pixel, or the history). A fallback march for a pixel the history cannot stand for stays
divergent, so keep it rare: that is what the history's bilinear taps and their depth test are
for. Time a frame's passes as the median of several frames, not the last one alone.
