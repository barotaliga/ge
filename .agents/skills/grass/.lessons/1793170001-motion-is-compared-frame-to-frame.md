---
title: A blade's motion is tested frame against frame, never against zero
date: 2026-10-28
scope: tests/render_grass_test.cpp
concepts: [motion-vectors, taa, test, readback, rgba16f]
---

With a still camera the motion target is not zero: the terrain and the sky write their own values
over half the frame. Test the grass's own motion as the difference between a frame at `t` and one at
`t + dt` (two frames at the same `t` must match exactly, and the pixels that change must be grass
pixels). The target is RGBA16F, which `gpu::read_now` does not read; the test copies it out itself,
and the target needs `CopySrc` (render/frame.cpp).
