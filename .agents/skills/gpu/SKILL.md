---
name: gpu
description: "Use for engine/gpu in gameengine2: the webgpu.h wrapper over Dawn (device, feature tiers, surface, buffers, textures, bind groups, pipelines and the pipeline manifest, the frame encoder, timestamp queries, staging uploads, readback), WGSL embedding and make shaders-check, Dawn built for desktop, iOS and Android, mobile tiers, and Dawn on a software adapter in the GPU-less container and CI."
---

# GPU (WebGPU through Dawn)

`engine/gpu/` is the only directory that includes `webgpu.h`. The plan, and the reasons
for each choice, are in `docs/rendering.md`. Read § The backend and § Feature tiers
there before the first edit.

## The backend

- **Dawn native on every target**: macOS, Linux, Windows, iOS, iPadOS, the Mac App
  Store and Android. There is no web build. Desktop links Dawn's prebuilt GitHub
  release, pinned to its tag. iOS, iPadOS and Android build Dawn from source, cached in
  CI.
- **Debug validates, release does not.** Debug builds keep Dawn's validation. Release
  builds set `skip_validation` and `disable_robustness`.
- **Keep to the standard header.** Use a Dawn-only entry point only behind a feature
  tier, with a path for when it is missing. Code that keeps to `webgpu.h` could still
  run on wgpu-native.
- **No second graphics API.** If WebGPU blocks a measured goal, the answer is a Dawn
  native extension behind a tier. Never add Vulkan, Metal or D3D12 code.
- Rows without a Dawn build yet use `GE_GPU=OFF` (everything except
  `engine/gpu`, `engine/render` and the app).

## The wrapper's job

- **Thin RAII handles** (`gpu::Buffer`, `gpu::Texture`, `gpu::Pipeline`, …) that
  release on destruction. Do not abstract over WebGPU's model: an agent reading the
  WebGPU spec should recognise every call.
- **Device creation** asks for the adapter's own limits and turns on every feature in
  `docs/rendering.md` § Feature tiers that the adapter has. `gpu::Features` records
  which ones are on, and a pass branches on that struct, never on a vendor string.
- **Device loss and validation errors** go through `core/output` as agent-actionable
  lines: the object's label, the call, and the next action.
- **Label every object** (`label = "terrain.ring3.indirect"`). Dawn's errors quote the
  label, and the labels are the names lab captures use.

## Pipelines

- Every pipeline is listed in the game's **pipeline manifest** and created with
  `createRenderPipelineAsync` / `createComputePipelineAsync` at load.
- `gpu::Device` counts creations. After `end_load()`, a creation is an error:
  `gpu: pipeline 'x' created mid-run; add it to the manifest`. Never silence it.
- Quality switches are WGSL `override` constants set at creation, not text variants.

## Shaders

- WGSL, one file per pass, beside the pass's C++. A CMake step resolves `#include`
  and embeds the result as a string. No shader is read from disk at run time.
- `make shaders-check` compiles every WGSL file with Tint offline, so a shader error
  fails CI with its file and line, without a GPU.
- Work in linear light. The post pass is the one place that encodes for the display.

## Uploads and readback

- One `queue.writeBuffer` per changed buffer per frame, from a staging array that a
  pass's `prepare` filled. Instance families send only the changed ranges.
- Readback (lab captures, the culling counters) maps a buffer one or more frames
  later. Never stall a frame waiting for a map.

## Timing

- The pass table wraps every pass in a timestamp query pair when `timestamp-query` is
  on. The resolved times feed the frame's stats one frame late.

## Testing

- CI has no GPU. Every pass's `prepare` is testable without a device.
- **The main environment has no GPU.** The development container and CI's Linux x64
  row run Dawn on the same pinned software Vulkan adapter (SwiftShader or lavapipe):
  the device smoke test, every pipeline in the manifest, and the lab captures against
  golden images within a tolerance. A capture from another driver is never compared
  bit for bit.
- **The Mac (M3 Max, Metal) is for what software cannot show**: GPU timings, Metal-only
  or real-GPU-only bugs, the full-resolution look. `docs/rendering.md` § Where it is
  developed lists them. When a task needs it, tell the person and open a `needs-mac`
  GitHub issue with the commands, what to look at and what to paste back. Never for
  speed alone.
- Test both sides of every feature tier: a test can create the device with a feature
  forced off.

## Labs

M6 lands the headless capture `Lab::shot` takes GPU frames from: an offscreen target on the
software adapter, so it runs in the GPU-less Linux container and on CI, read back to RGBA8
(`docs/labs.md` § Pictures before and after the renderer). Its smoke lab clears to a known
colour and checks the pixels.
