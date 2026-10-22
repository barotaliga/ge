// THE HI-Z PYRAMID (docs/rendering.md § Culling): the depth prepass's depth reduced to a mip chain
// of R32F, each texel the FARTHEST depth under it (reversed-z: the minimum). A box whose nearest
// depth is farther than the pyramid's texel over its screen rectangle is hidden behind what the
// prepass drew, so a cull can drop it after reading one texel at the mip its size picks. The
// instanced draws cull against it (M7d); the terrain is its first writer.
//
// THE MIPS: mip 0 is half the render size (rounding down, at least one texel), each next halves
// the last, down to 1x1. A texel covers every texel of the level below that overlaps it, so an
// odd size's last row or column is read by both of its neighbours and never dropped: the
// pyramid is conservative. `hiz_reduce` is the CPU reference, texel for texel.
//
// ge:summary The Hi-Z pyramid: the depth's farthest value per texel over a mip chain, and its
// CPU reference
// ge:keywords render hi-z hiz depth-pyramid occlusion-culling min-depth reversed-z mip-chain
// ge:keywords create_hiz encode_hiz hiz_reduce read_hiz read_depth hiz_mips
// ge:skill render
// ge:test tests/render_draw_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr int kHizMaxMips = 14;

struct Hiz {
  uint32_t width = 0, height = 0;  // mip 0's
  int mips = 0;
  wgpu::Texture pyramid;
  wgpu::TextureView mip_views[kHizMaxMips];
  wgpu::TextureView view;  // every mip, for a cull to sample
  wgpu::BindGroupLayout first_layout, next_layout;
  wgpu::BindGroup groups[kHizMaxMips];
  uint32_t p_first = 0, p_next = 0;
};

// Mip 0's size for a render of width x height, and the chain's length.
void hiz_size(uint32_t width, uint32_t height, uint32_t* mip0_w, uint32_t* mip0_h, int* mips);

// The pipelines (added to the manifest).
void create_hiz_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Hiz& hiz);
// The pyramid and its groups for a set of targets (again after a resize).
void create_hiz_targets(gpu::Device& device, Hiz& hiz, const FrameTargets& targets);
// The reduction ("hiz"): one dispatch a mip, after the depth prepass.
void encode_hiz(const Hiz& hiz, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder);

// The CPU reference: the pyramid of a row-major depth image, mip 0 first.
std::vector<std::vector<float>> hiz_reduce(const std::vector<float>& depth, uint32_t width,
                                           uint32_t height);

// Labs and tests (block): one mip of the pyramid, and the frame's depth, read back row-major.
std::vector<float> read_hiz(gpu::Device& device, const Hiz& hiz, int mip);
std::vector<float> read_depth(gpu::Device& device, const FrameTargets& targets);

}  // namespace ge::render
