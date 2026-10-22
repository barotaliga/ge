// THE POST PASS (docs/rendering.md § Post): the frame's HDR target to the output (an offscreen
// target or the surface's texture) in one full-screen pass: the bloom mixed in (render/bloom.hpp),
// the exposure read off the sky's light (frame.wgsl's `exposure`), the tone curve, the colour
// grade, a dither and the output encoding. It reads the frame at the output's size: TAA-U's
// resolve (render/taa.hpp) or, without it, the HDR target itself.
//
// THE GRADE is a kGradeSize³ lookup table the game bakes from its look (`set_grade`): an
// sRGB-encoded colour in, the graded sRGB-encoded colour out, read trilinearly after the tone
// curve, so it works where the eye judges colour and its 32 steps a channel are evenly spent.
// It starts as the identity (`identity_grade`).
//
// A fragment pass rather than a compute one: it writes the surface's texture, which a compute
// shader may not store to without an optional feature (bgra8unorm storage), and a tile GPU
// resolves a full-screen fragment pass in tile memory.
//
// ge:summary The post pass: bloom mixed in, exposure from the sky, tone curve, the colour grade's
// lookup table, dither, output encoding
// ge:keywords render post exposure tone-mapping tonemap pbr-neutral dither srgb encode output
// ge:keywords bloom colour-grade color-grade lut lookup-table 3d-lut grade
// ge:keywords create_post post_group encode_post set_grade identity_grade
// ge:skill render
// ge:test tests/render_post_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr uint32_t kGradeSize = 32;  // the grade's lookup table, a side

struct PostSettings {
  double bloom = 0;  // the share of the bloom's glow mixed in (BloomSettings::strength); 0: none
};

struct Post {
  PostSettings settings;
  wgpu::BindGroupLayout layout;
  wgpu::TextureFormat output = wgpu::TextureFormat::RGBA8Unorm;
  wgpu::Sampler sampler;
  wgpu::Texture grade;
  wgpu::TextureView grade_view;
  wgpu::Texture black;  // 1 x 1: the bloom the pass reads when there is none
  wgpu::TextureView black_view;
  uint32_t p_draw = 0;
};

// Add the post pipeline for an output of `format` to the manifest; the grade starts as the
// identity.
void create_post(gpu::Device& device, gpu::PipelineSet& pipelines, Post& post,
                 const wgpu::BindGroupLayout& frame, wgpu::TextureFormat output,
                 const PostSettings& settings = {});
// The pass's own group over the frame it reads (an output-size HDR view: TAA-U's output, or the
// HDR target when it renders at the output's size) and the bloom's top level (null: none), made
// once a set of targets.
wgpu::BindGroup post_group(gpu::Device& device, const Post& post, const wgpu::TextureView& hdr,
                           const wgpu::TextureView& bloom = nullptr);
// The pass ("post"): the frame into `output` (cleared, then every pixel written).
void encode_post(const Post& post, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame,
                 const wgpu::BindGroup& group, const wgpu::TextureView& output);

// The grade's table: kGradeSize³ RGB triples, red fastest, then green, then blue; entry (r, g, b)
// is what the sRGB-encoded colour (r, g, b) / (kGradeSize - 1) becomes.
std::vector<float> identity_grade();
// Upload a table (identity_grade's layout); any group made since reads it.
void set_grade(gpu::Device& device, Post& post, std::span<const float> table);

}  // namespace ge::render
