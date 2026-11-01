---
name: gpu
description: "Use for engine/gpu in gameengine2: the native GPU device over Dawn (its webgpu.h header) (device, feature tiers, surface, buffers, textures, bind groups, pipelines and the pipeline manifest, the frame encoder, timestamp queries, staging uploads, readback), WGSL embedding and make shaders-check, Dawn built for desktop, iOS and Android, mobile tiers, and Dawn on a software adapter in the GPU-less container and CI."
---

# GPU (Dawn, native)

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
- **No second graphics API.** If Dawn's core API blocks a measured goal, the answer is a Dawn
  native extension behind a tier. Never add Vulkan, Metal or D3D12 code.
- Rows without a Dawn build yet use `GE_GPU=OFF` (everything except
  `engine/gpu`, `engine/render` and the app).
- **On Windows, Dawn's DLLs load only from a path named in full.** Dawn opens a library at
  each runtime search path plus its name, and Windows refuses its last try, the bare name, with
  error 87. So `create_device` names System32 (`d3dcompiler_47.dll`, `vulkan-1.dll`), and CMake
  copies the Windows SDK's `dxcompiler.dll` and `dxil.dll` (D3D12's shader compiler and its
  validator, which the archive lacks) beside every executable (`ge_dxc_beside_executables`,
  cmake/Dawn.cmake). A device that fails with `DynamicLib.Open: <name> Windows Error` has lost
  one of them.

## What is built (M6), and where

| File | What |
| --- | --- |
| `cmake/Dawn.cmake` | `GE_GPU`, THE PIN (tag, commit, three SHA-256s), the download into `.cache/deps/`, `GE_DAWN_DIR`, `GE_TINT` |
| `cmake/Wgsl.cmake`, `WgslEmbed.cmake`, `WgslCheck.cmake` | `ge_embed_wgsl`, `#include` resolution, `shaders_check` (every entry point through Tint) |
| `engine/gpu/device.hpp` | `create_device` (adapter, limits, tiers, toggles, error lines), `Features`, `end_load`, `wait`, `adapter_line` |
| `engine/gpu/pipeline.hpp` | `create_shader` (compile messages as lines), `PipelineSet` (the manifest, async, counted) |
| `engine/gpu/target.hpp` | `Target`, `Readback` (frame-late), `read_now` (blocking, tests and labs), `Image` |
| `engine/gpu/timer.hpp` | `PassTimer`: GPU ms per pass, inert without `timestamp-query` |
| `engine/gpu/surface.hpp` | `Surface` from `platform::NativeWindow`: configure, acquire, present at FIFO |
| `engine/gpu/test_scene.hpp` | The golden capture's frame; the demo window's frame until M7 |

## The wrapper's job

- **Dawn's `webgpu_cpp.h` handles are the RAII handles** (`wgpu::Buffer`, `wgpu::Texture`,
  …, reference-counted, released on destruction). The engine adds no second handle layer:
  it adds the decisions (the device's choices, the manifest, readback, timing). An agent
  reading Dawn's API recognises every call.
- **Device creation** asks for the adapter's own limits and turns on every feature in
  `docs/rendering.md` § Feature tiers that the adapter has. `gpu::Features` records
  which ones are on, and a pass branches on that struct, never on a vendor string.
- **Device loss and validation errors** go through `core/output` as agent-actionable
  lines: the object's label, the call, and the next action.
- **Label every object** (`label = "terrain.ring3.indirect"`). Dawn's errors quote the
  label, and the labels are the names lab captures use.

## Pipelines

- Every pipeline is listed in the game's **pipeline manifest** and created with
  `createRenderPipelineAsync` / `createComputePipelineAsync` at load: `PipelineSet::add_*`
  starts each one, `finish` waits for them all. Do not move a `PipelineSet` while it is
  loading (its callbacks hold its address).
- `gpu::Device` counts creations. After `end_load()`, a creation is an error:
  `gpu: pipeline 'x' created mid-run; add it to the manifest`. Never silence it.
- Quality switches are WGSL `override` constants set at creation, not text variants.

## Shaders

- WGSL, one file per pass, beside the pass's C++. A CMake step resolves `#include`
  and embeds the result as a string. No shader is read from disk at run time.
- The embedded string stays under 60 000 bytes (MSVC's literal cap), counted after the
  includes are resolved. Whole-line `//` comments are left out as it is embedded and never count,
  so comment a shader as fully as any C++ file; past the cap, split the pass.
- `make shaders-check` compiles every WGSL file with Tint offline, so a shader error
  fails CI with its file and line, without a GPU.
- Work in linear light. The post pass is the one place that encodes for the display.

## Uploads and readback

- One `queue.WriteBuffer` per changed buffer per frame, from a staging array that a
  pass's `prepare` filled. Instance families send only the changed ranges. That is the
  whole upload path: the queue stages the copy, so there is no wrapper to call.
- Readback (lab captures, the culling counters) maps a buffer one or more frames
  later. Never stall a frame waiting for a map.

## Timing

- The pass table wraps every pass in a timestamp query pair when `timestamp-query` is
  on. The resolved times feed the frame's stats one frame late.

## Moving the Dawn pin

1. Pick a release tag on github.com/google/dawn whose release has all three archives
   (`Dawn-<commit>-{ubuntu,macos,windows}-latest-Release.tar.gz`); not every nightly does.
2. Set `GE_DAWN_TAG`, `GE_DAWN_COMMIT` and the three SHA-256s in `cmake/Dawn.cmake` together.
3. Rebuild from a clean `build/` and run every `gpu_` and `platform_` test, `make
   shaders-check` and the GPU labs. A header change shows as a compile error in
   `engine/gpu` only (nothing else includes webgpu.h).

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
  forced off (`DeviceOptions::force_off`).
- **`tests/gpu_support.hpp`**: `test_device()` (validation on; skip without an adapter, fail
  under `GE_GPU_REQUIRE=1`), and golden images as binary PPM under `tests/golden/`,
  compared within a tolerance (`check_golden_image`) on the pinned adapter only
  (`pinned_adapter`: lavapipe). `GE_UPDATE_GOLDEN=1` rewrites one; open it (convert it to
  PNG to view it) and say in the PR what changed.
- `gpu_` and `platform_` tests exit 77 (CTest's skip) when the machine cannot run them, and
  run without LeakSanitizer (the software driver's worker threads keep allocations past
  exit).
- The window: `xvfb-run -a ctest -R platform` (or `xvfb-run -a fall_line --frames=60`) in the
  container. Xvfb has no vsync, so FIFO there is unthrottled: frame pacing is a Mac check.

## Labs

- **`gpu-capture`** (`engine/lab/labs/gpu_capture_lab.cpp`): the test scene drawn into an
  offscreen target, read back with `read_now` and handed to `Lab::shot` as a `Picture`. That
  is the pattern every renderer lab follows (`docs/labs.md` § Pictures before and after the
  renderer). On the pinned adapter at 96x64 it holds the frame to the golden and fails when
  it is off; with no adapter it draws a placeholder and notes it.
- **A GPU frame is a `measured` shot**: `lab.shot(shot, picture, Det::measured)`. Each
  adapter rasterises in its own way, so its pixels never enter the lab's digest (the id
  does). Whatever must hold across rows goes in a deterministic metric, and a pixel check
  is the lab's own, on the pinned adapter.
- GPU and window labs are named `gpu_*_lab.cpp` and `platform_*_lab.cpp` in
  `engine/lab/labs/`; the root `CMakeLists.txt` drops them from the `labs` executable when
  `GE_GPU` (or `GE_PLATFORM`) is off, so every lab's digest must be the same whether or not
  an adapter or a display is there.
