// THE POST PASS (docs/rendering.md § Post): the frame's HDR target to the output (an offscreen
// target or the surface's texture) in one full-screen pass: the exposure read off the sky's
// light (frame.wgsl's `exposure`), the tone curve, a dither and the output encoding. It reads
// the frame at the output's size: TAA-U's resolve (render/taa.hpp) or, without it, the HDR target
// itself. Bloom and the colour grade join it with M7d3.
//
// A fragment pass rather than a compute one: it writes the surface's texture, which a compute
// shader may not store to without an optional feature (bgra8unorm storage), and a tile GPU
// resolves a full-screen fragment pass in tile memory.
//
// ge:summary The post pass: exposure from the sky, tone curve, dither, output encoding
// ge:keywords render post exposure tone-mapping tonemap pbr-neutral dither srgb encode output
// ge:keywords create_post post_group encode_post
// ge:skill render
// ge:test tests/render_frame_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

struct Post {
  wgpu::BindGroupLayout layout;
  wgpu::TextureFormat output = wgpu::TextureFormat::RGBA8Unorm;
  uint32_t p_draw = 0;
};

// Add the post pipeline for an output of `format` to the manifest.
void create_post(gpu::Device& device, gpu::PipelineSet& pipelines, Post& post,
                 const wgpu::BindGroupLayout& frame, wgpu::TextureFormat output);
// The pass's own group over the frame it reads (an output-size HDR view: TAA-U's output, or the
// HDR target when it renders at the output's size), made once a set of targets.
wgpu::BindGroup post_group(gpu::Device& device, const Post& post, const wgpu::TextureView& hdr);
// The pass ("post"): the frame into `output` (cleared, then every pixel written).
void encode_post(const Post& post, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame,
                 const wgpu::BindGroup& group, const wgpu::TextureView& output);

}  // namespace ge::render
