---
title: A black WebGPU canvas in the headless browser may be the browser, not the viewer
date: 2026-11-19
scope: games/shooters_edge/labs/hub_viewer_lab.cpp
concepts: [hub-viewer, chromium, webgpu, headless, swiftshader, blank-canvas, shared-image]
---

On Chrome for Testing 153 in the container (`/opt/pw-browsers`, both the full browser and the
headless shell), every WebGPU canvas came out blank: the device and the passes ran (the page's
title said so), but the browser's log said "Could not find SharedImageBackingFactory ...
WebgpuSwapChainTexture", and neither a screenshot nor `toDataURL` showed a pixel. No flag mix
(SwiftShader or Vulkan for ANGLE and WebGPU, Graphite on or off, GPU compositing off) fixed it.
Before you hunt a bug in the viewer, open a one-colour WebGPU page (clear to red, read a pixel) in
the same browser: if that is blank too, the browser is at fault. Prove the viewer's logic natively
with a test, and say in the PR that the browser picture could not be taken.
