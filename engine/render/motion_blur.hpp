// MOTION BLUR'S TILES (docs/rendering.md § Post, M35c): what the post pass needs to blur the
// picture along the motion the opaque passes already write (the `motion` target: each pixel's
// screen motion since the last frame, the camera's and its own), worked in two small compute
// dispatches before post:
//
// - "motion_blur.tiles": the fastest motion in each kTile x kTile tile of the motion target (and
//   the slowest's length), one lane a tile;
// - the same pass's second dispatch, the fastest of each tile's 3 x 3 neighbours, so a fast
//   object's blur reaches past its own tile into the still ones round it.
//
// The post pass (render/post.hpp's `PostLook::motion_blur`) reads the neighbours' fastest in one
// load a pixel: where it is under half a pixel the pixel is left as it is (a still picture costs
// that one load), and elsewhere it gathers a few taps along that motion, each weighted by whether
// the tap's own motion reaches the pixel and which of the two stands in front (the depth), so a
// sharp figure the camera follows stays sharp against a streaming background and the background
// does not smear over it.
//
// The targets are RGBA16F (rg the motion in uv a frame, as the motion target holds it), at the
// render size over kTile, rounded up.
//
// ge:summary Motion blur's tiles: the fastest motion a tile and over its neighbours, for post
// ge:keywords render motion-blur tiles tile-max neighbour-max velocity reconstruction shutter
// ge:keywords create_motion_blur_pipelines create_motion_blur_targets encode_motion_blur
// ge:skill render
// ge:test tests/render_motion_blur_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr uint32_t kTile = 16;  // the tile's side, render pixels

struct MotionBlur {
  uint32_t width = 0, height = 0;  // the tiles across and down
  wgpu::Texture tiles, near;       // each tile's fastest; its neighbours' fastest
  wgpu::TextureView tiles_view, near_view;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup tile_group, near_group;
  uint32_t p_tiles = 0, p_near = 0;
};

// The tiles across and down for a motion target of `width` x `height`.
void motion_blur_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h);
void create_motion_blur_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines,
                                  MotionBlur& blur);
// The two tile targets for a motion target at the render size, and the groups over it.
void create_motion_blur_targets(gpu::Device& device, MotionBlur& blur, uint32_t width,
                                uint32_t height, const wgpu::TextureView& motion);
// The pass ("motion_blur.tiles"): both dispatches.
void encode_motion_blur(const MotionBlur& blur, const gpu::PipelineSet& pipelines,
                        FrameRecorder& rec, wgpu::CommandEncoder& encoder);

}  // namespace ge::render
