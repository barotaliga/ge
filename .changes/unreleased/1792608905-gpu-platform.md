---
type: Added
title: The window and the GPU device
---

`engine/platform` opens a window over SDL3 and reads its input into a per-frame snapshot; `engine/gpu` makes a WebGPU device over Dawn's prebuilt release (the adapter, its feature tiers, validation and agent-actionable error lines), compiles embedded WGSL, creates pipelines through a manifest that counts any made after load, presents to the window at vsync, renders offscreen and reads frames back, and times passes with timestamp queries. `make shaders-check` compiles every shader with Tint offline. Both are built with the engine's own tree (`GE_PLATFORM`, `GE_GPU`) and are not part of the published package yet.
