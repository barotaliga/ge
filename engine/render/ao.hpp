// AMBIENT OCCLUSION (docs/rendering.md § Ambient occlusion, M35e): how much of the sky each
// pixel's surface can see, worked out from the depth target alone, so the sky's light
// (`sky_irradiance`) darkens in the creases the clipmap and the meshes make between them: the
// snow at a trunk's foot, the ground under a hut's eaves, the skier's legs and the snow beside
// his boots, a gully's floor.
//
// One compute pass ("ao") after the depth prepass and before the first colour pass, at HALF the
// render size, ground-truth horizon search over the depth (a few slices round the view ray, a
// few steps out each way along each, the arc between the two horizons integrated against the
// surface's normal, which is rebuilt from the depth itself). Each frame turns the slices and
// shifts the steps by a noise of the pixel and the frame laid out so any 3 x 3 texels hold its
// turns evenly; a second dispatch takes each texel's mean over the 3 x 3 round it whose depth is
// like its own, and the temporal filter averages what is left over the frames.
//
// The target is RGBA16F: r the occlusion (0 open, 1 closed, already times the strength), g the
// depth of the half-size texel it was worked at, so a full-size surface reads the nearest texels
// whose depth is like its own (frame.wgsl's `sky_seen`) and an edge does not bleed one surface's
// shade onto the other. The frame group binds it as its binding 10; with AO off the frame group
// binds the sky's 1 x 1 blank there and `sky_seen` is one dimension query.
//
// What it costs: one full-size depth load for the texel, four for its normal and
// 2 x kAoSlices x kAoSteps along the slices, a half-size texel each, then nine loads to smooth it;
// the far ground's texels, whose radius on screen is under two pixels, stop after the normal.
//
// ge:summary Ambient occlusion: a half-size horizon search over the depth, read by the sky's light
// ge:keywords render ambient-occlusion ao gtao horizon-based ssao sky-visibility half-size
// ge:keywords create_ao_pipelines create_ao_targets encode_ao set_ao ao-settings r_ao
// ge:skill render
// ge:test tests/render_ao_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr uint32_t kAoSlices = 2;  // the slices round the view ray a texel a frame
inline constexpr uint32_t kAoSteps = 4;   // the steps out each way along a slice

struct AoSettings {
  double strength = 0;    // the occlusion's share taken off the sky's light, 0..1; 0: off
  double radius_m = 1.5;  // how far from a surface something still shades it, m
};

inline bool ao_on(const AoSettings& s) { return s.strength > 0; }

struct Ao {
  uint32_t width = 0, height = 0;  // half the render size, rounded up
  wgpu::Texture raw, target;       // the search's; smoothed (what the frame group binds)
  wgpu::TextureView raw_view, view;
  wgpu::Buffer params;
  wgpu::BindGroupLayout layout, blur_layout;
  wgpu::BindGroup group, blur_group;
  uint32_t pipeline = 0, p_blur = 0;
};

// The half size for a render target of `width` x `height`.
void ao_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h);
void create_ao_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Ao& ao);
// The target for a render size of `width` x `height`, its group over the frame block and the
// depth target. The frame group must then be remade with `ao.view` (sky.hpp's `frame_group`).
void create_ao_targets(gpu::Device& device, Ao& ao, uint32_t width, uint32_t height,
                       const wgpu::Buffer& frame_uniforms, const wgpu::TextureView& depth);
// The strength and the radius, written to the pass's own block.
void set_ao(gpu::Device& device, const Ao& ao, const AoSettings& settings);
// The pass ("ao"): the search, then the smoothing.
void encode_ao(const Ao& ao, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
               wgpu::CommandEncoder& encoder);

}  // namespace ge::render
