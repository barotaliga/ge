---
type: Added
title: GE_GPU_LIMITS=default runs the GPU on a browser's limits
---

`GE_GPU_LIMITS=default` (or `DeviceOptions::default_limits`) asks for WebGPU's default limits with
validation on, so a native run names every pipeline a browser would refuse. The classic map's
shadow cubes are as many as the device's array layers hold, and its cull fits 8 storage buffers.
