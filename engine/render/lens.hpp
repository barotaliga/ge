// THE LENS PASS (docs/rendering.md § Post, M35b): the light a camera's lens adds round the key
// light (the sun, or the moon when it keys), worked at a QUARTER of the output's pixels in one
// compute dispatch and read by the post pass in one bilinear tap (render/post.hpp's `kLens`):
//
// - SUN RAYS through what stands: from each texel a march toward the sun's place on the screen,
//   summing the open sky the march crosses near the sun (the depth target's sky: nothing drawn),
//   in that sky's own colour. Where a trunk, a ridge or a figure stands in front of the sun, the
//   march crosses it and the ray behind it is dark: the light streams through the gaps. This is
//   the screen's half of the sun's beams; the air's half (the cloud's shadow across the air) is
//   the sky's own (frame.wgsl's `shafts`), and the two stack.
// - THE FLARE: the reflections between a lens's glasses, drawn analytically on the line from the
//   sun's place through the middle of the screen (a few soft ghosts, each its own size and coating
//   tint, and a faint ring round the sun), scaled by how much of the sun is open sky (a ring of
//   depth taps round it) and how much of it reaches the ground through the cloud.
//
// Both fade as the sun leaves the screen and are nothing with the sun behind the eye or under the
// horizon. Each is an `override` switch (LensSettings above 0 when the pipeline is made); the
// strengths are live (`set_lens`). The pass reads the post pass's input (TAA-U's history, or the
// HDR target) and the depth target, and nothing else; it knows no game.
//
// ge:summary The lens pass: sun rays through what stands and the sun's flare, at a quarter size
// ge:keywords render lens sun-rays god-rays light-shafts crepuscular radial-blur lens-flare flare
// ge:keywords ghosts halo occlusion quarter-resolution create_lens_pipelines create_lens_targets
// ge:keywords encode_lens set_lens LensSettings r_sunrays r_flare
// ge:skill render
// ge:test tests/render_lens_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

// Each strength 0..1; 0 at create_lens_pipelines leaves its half out of the shader.
struct LensSettings {
  double rays = 0;   // 1: a gap in the trees beside the sun streams about as bright as the sky
  double flare = 0;  // 1: the brightest ghost about a quarter of white
};

struct Lens {
  LensSettings settings;
  uint32_t width = 0, height = 0;  // the target's: a quarter of the output's (rounded up)
  wgpu::Texture target;
  wgpu::TextureView view;  // what the post pass reads (pre-exposure light, rgb)
  wgpu::Buffer params;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup groups[2];  // over the post pass's two inputs
  uint32_t p_lens = 0;
};

// True when either half is on: a frame with neither makes no lens pass at all.
inline bool lens_on(const LensSettings& s) { return s.rays > 0 || s.flare > 0; }

// The target's size for an output of width x height.
void lens_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h);

// The pipeline (added to the manifest); `frame` is group 0's layout.
void create_lens_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Lens& lens,
                           const wgpu::BindGroupLayout& frame, const LensSettings& settings);
// The target for an output of width x height and the groups over `inputs` (the post pass's: TAA's
// two histories, or the HDR target twice) and the frame's depth; again after a resize.
void create_lens_targets(gpu::Device& device, Lens& lens, uint32_t width, uint32_t height,
                         const wgpu::TextureView inputs[2], const wgpu::TextureView& depth);
// The strengths, live; a half not compiled stays off whatever it is given.
void set_lens(gpu::Device& device, const Lens& lens, const LensSettings& settings);
// The pass ("lens"): one dispatch over the target, from input `which`.
void encode_lens(const Lens& lens, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, int which);

}  // namespace ge::render
