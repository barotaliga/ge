// BLOOM (docs/rendering.md § Post): the light a lens and the eye scatter round anything bright,
// from the frame the post pass reads (at the output's size), as a chain of half-size mips:
//
// - DOWN: each level is the one above it filtered by the 13-tap downsample (four overlapping
//   2x2 boxes and a centre box, each tap a bilinear read), so a thin bright line is neither
//   dropped nor aliased. The first level weighs each box by the inverse of its brightness (a
//   Karis average), so a lone bright pixel (a sun glint) cannot flicker into a blob.
// - UP: from the smallest level back up, each level is its down level plus the level below it
//   read through a 3x3 tent, so every level adds its own width of glow.
//
// The post pass reads the top up level and mixes `strength` of it into the frame, divided by the
// levels it summed (an energy-conserving mix: nothing is thresholded, so a bright surface glows
// in proportion to its light and a dim one barely at all). A compute pass ("bloom"), one
// dispatch a level each way; nothing here knows a game.
//
// ge:summary Bloom: a half-size mip chain down (13-tap, Karis first) and up (tent), for post
// ge:keywords render bloom glow dual-filter downsample upsample karis tent mip-chain post
// ge:keywords create_bloom_pipelines create_bloom_targets encode_bloom bloom_size
// ge:skill render
// ge:test tests/render_post_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr int kBloomMaxLevels = 8;

struct BloomSettings {
  int levels = 6;         // half the output's size down to 1/64 of it (fewer when it is small)
  double strength = 0.0;  // the share of the glow the post pass mixes in; 0: no bloom
};

struct Bloom {
  BloomSettings settings;
  int levels = 0;                  // this output's (bloom_size)
  uint32_t width = 0, height = 0;  // level 0's
  wgpu::Texture down, up;          // `levels` and `levels - 1` mips
  wgpu::TextureView down_views[kBloomMaxLevels], up_views[kBloomMaxLevels];
  wgpu::TextureView view;  // what the post pass reads: the top up level (or the down level)
  wgpu::Sampler sampler;
  wgpu::BindGroupLayout down_layout, up_layout;
  wgpu::BindGroup first[2];  // level 0 from each of the post pass's inputs
  wgpu::BindGroup downs[kBloomMaxLevels], ups[kBloomMaxLevels];
  uint32_t p_first = 0, p_down = 0, p_up = 0;
};

// Level 0's size for an output of width x height and the levels the chain has: each level
// halves the last (rounding down, at least one texel), at most `levels`, stopping before a level
// would be under 2 texels either way.
void bloom_size(uint32_t width, uint32_t height, int levels, uint32_t* w0, uint32_t* h0,
                int* count);

// The pipelines (added to the manifest), the sampler and the layouts.
void create_bloom_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Bloom& bloom,
                            const BloomSettings& settings);
// The chain for an output of width x height, read from `inputs` (the post pass's: TAA's two
// histories, or the HDR target twice); again after a resize.
void create_bloom_targets(gpu::Device& device, Bloom& bloom, uint32_t width, uint32_t height,
                          const wgpu::TextureView inputs[2]);
// The pass ("bloom"): down from input `which`, then up.
void encode_bloom(const Bloom& bloom, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, int which);

}  // namespace ge::render
