---
name: gpu
description: "Use for engine/gpu in gameengine2: the webgpu.h wrapper (device, surface, buffers, textures, bind groups, pipelines, the frame encoder, timestamp queries), WGSL shader embedding, and the wgpu-native/Dawn/emdawnwebgpu backends."
---

# GPU (WebGPU via webgpu.h)

`engine/gpu/` is the only directory that includes `webgpu.h`. Native builds start on
wgpu-native's prebuilt release, and the code keeps to the standard header so Dawn can
be swapped in. The web build uses Emscripten's emdawnwebgpu.

## The wrapper's job

- Thin RAII handles (`gpu::Buffer`, `gpu::Texture`, `gpu::Pipeline`) that release
  on destruction. No abstraction over WebGPU's model: an agent reading the WebGPU
  spec should recognise every call.
- Device creation asks for the adapter's own limits and turns on the optional features
  it has (`timestamp-query`, `shader-f16`, `float32-filterable`), following game5's
  `gpu-device.ts`.
- One frame = acquire the surface texture, one command encoder, the passes in fixed
  order, submit, present.
- Timestamp queries per pass feed the `perf` skill's GPU numbers.

## Shaders

- WGSL, one file per pass under the pass's directory (`*.wgsl`), embedded at build time
  as a string by a CMake step. No runtime file reads.
- game5's TSL/GLSL is the oracle for the look. Port the maths and keep the names. Every
  hand-written shader works in linear light and converts at the end, as game5's do.

## Rules

- Pipelines and bind group layouts are created once at load. Nothing is compiled
  mid-run.
- Upload per-frame data with one `queue.writeBuffer` per buffer per frame, from a CPU
  staging array filled by `prepare`.
- CI has no GPU. Every pass's CPU half (`prepare`) is testable without a device, and
  a headless smoke test runs where an adapter exists (M6).
