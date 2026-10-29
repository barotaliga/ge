// DEPTH OF FIELD (docs/rendering.md § Post, M35d): the picture blurred away from a focus distance
// the game names (the subject a replay's lens or a photo follows), worked at HALF the output's
// pixels in one compute pass after the lens pass ("dof") and mixed in by the post pass
// (render/post.hpp's `kDof`) by each pixel's own blur circle, read off the full-size depth, so a
// sharp subject's edge stays as sharp as its pixels.
//
// - The blur circle (`wgsl/dof_coc.wgsl`) grows with |1 - focus / distance|, as a real lens's
//   does: the far field tends to the widest circle, the near field reaches it a little in front
//   of the focus; `sharp` of the way either side of the focus stays sharp.
// - The gather: 16 taps over a disc as wide as the widest circle (`DofSettings::strength` x 1.5 %
//   of the picture's height), turned per texel; each tap counts as far as its own circle reaches
//   the texel, and a tap behind the texel no further than the texel's own: the background never
//   bleeds over a sharp subject. A texel in focus skips the gather, since the post pass mixes the
//   gather in by each pixel's own circle; so a blurred near thing spreads over a blurred field
//   behind it but keeps a hard edge against a sharp one (the near field's spread over the focus,
//   which a scatter would draw, is left out).
//
// Off (strength 0) the pass is not made and the post pass compiles the mix out. The focus is
// live (`set_dof`, every frame if the game's subject moves).
//
// ge:summary Depth of field: a half-size gather round a focus, mixed in by post by each pixel's
// circle ge:keywords render depth-of-field dof bokeh circle-of-confusion focus half-size gather
// ge:keywords create_dof_pipelines create_dof_targets encode_dof set_dof dof-settings r_dof
// ge:skill render
// ge:test tests/render_dof_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr double kDofWidest = 0.015;  // the widest circle at strength 1, of the height

struct DofSettings {
  double strength = 0;  // the widest circle, 1: 1.5 % of the picture's height; 0: off
  double focus_m = 20;  // the distance in focus, m
  double sharp = 0.15;  // the share of the way either side of the focus kept sharp
};

inline bool dof_on(const DofSettings& s) { return s.strength > 0; }

struct Dof {
  uint32_t width = 0, height = 0;  // half the output's, rounded up
  wgpu::Texture target;
  wgpu::TextureView view;  // rgb the gathered light (pre-exposure), a the texel's own circle
  wgpu::Buffer params;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup groups[2];  // over the post pass's two inputs
  uint32_t pipeline = 0;
};

void dof_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h);
// The pipeline (added to the manifest); `frame` is group 0's layout.
void create_dof_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Dof& dof,
                          const wgpu::BindGroupLayout& frame);
// The target for an output of width x height and the groups over `inputs` (the post pass's: TAA's
// two histories, or the HDR target twice) and the frame's depth; again after a resize.
void create_dof_targets(gpu::Device& device, Dof& dof, uint32_t width, uint32_t height,
                        const wgpu::TextureView inputs[2], const wgpu::TextureView& depth);
// The focus, the sharp share and the strength, live.
void set_dof(gpu::Device& device, const Dof& dof, const DofSettings& settings);
// The pass ("dof"): one dispatch over the target, from input `which`.
void encode_dof(const Dof& dof, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, int which);

}  // namespace ge::render
