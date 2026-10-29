// THE BEAMS' PASS (docs/rendering.md § Beams): the lightning beams of render/beam.hpp drawn over
// the lit picture, and the burns they leave on the world's surfaces. A game builds each held
// beam's channel (`build_beam`) and hands the shapes with their looks to `prepare_beams` once a
// frame, with the vertices of its burns (a decal system's clipped meshes: engine/detail's
// scorches, their ember from `ember_level`); `encode_beams` draws the burns, then the beams.
//
// - THE BURNS go first, over the opaque picture and under the beams: a scorch's soot multiplies
//   what is there and its ember adds its glow, one blend (src + dst * src.alpha), depth-tested
//   against the surface they lie a millimetre off. Their mask is the game's atlas (`BeamSettings`'
//   picture: a decal atlas's alpha the cover, green the relief).
// - THE BEAMS are additive ribbons (render/beams.wgsl), depth-tested, softened against the depth
//   behind them, writing the motion target's reactive channel so TAA does not smear them.
// - Up to kMaxBeamDraws beams a frame with their own looks, `max_segments` segments and
//   `max_burn_verts` burn vertices in all, reserved at create; more are dropped and counted.
//
// ge:summary The beams' pass: lightning ribbons added over the picture, and their burns' soot and
// ember ge:keywords render beams pass ribbon additive glow core sheath burn scorch ember soot decal
// ge:keywords prepare_beams encode_beams create_beams lightning
// ge:skill render
// ge:test tests/render_beam_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/beam.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxBeamDraws = 8;

struct BeamSettings {
  uint32_t max_segments = 2048;
  uint32_t max_burn_verts = 12288;
  // The burns' mask atlas, RGBA8 (a: cover, g: relief); empty: a soft round mark.
  const std::vector<uint8_t>* atlas = nullptr;
  uint32_t atlas_width = 0, atlas_height = 0;
};

// One beam to draw: its channel this frame and its look.
struct BeamDraw {
  const BeamShape* shape = nullptr;
  const BeamLook* look = nullptr;
  uint32_t seed = 0;
};

// One vertex of a burn's mesh (three a triangle), in world metres.
struct BurnVertex {
  double at[3] = {0, 0, 0};
  float u = 0, v = 0;  // in the atlas
  float fade = 1;      // its triangle's fade (turned from the decal's normal)
  float soot = 0.85f;  // how dark the scorch makes what is under it, 0..1
  float ember = 0;     // its glow, cd/m^2 (ember_level)
};

// wgsl's BeamLookGpu, BeamSeg and BurnVert.
struct GpuBeamLook {
  float core[4], glow[4], shape[4], more[4];
};
struct GpuBeamSeg {
  float a[4], b[4], c[4];
};
struct GpuBurnVert {
  float at[4], uv[4];
};
struct GpuBeamParams {
  float info[4];
  float more[4];
  GpuBeamLook looks[kMaxBeamDraws];
};

struct Beams {
  BeamSettings settings;
  wgpu::Buffer params, segs, burns;
  wgpu::Texture atlas;
  wgpu::TextureView atlas_view;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_burn = 0, p_beam = 0;
  // The CPU's staging, reserved at create.
  GpuBeamParams staged{};
  std::vector<GpuBeamSeg> seg_staging;
  std::vector<GpuBurnVert> burn_staging;
  uint32_t seg_count = 0, burn_count = 0;
  uint32_t dropped = 0;  // segments, burn vertices or beams past the reserve this frame
};

void create_beams(gpu::Device& device, gpu::PipelineSet& pipelines, Beams& beams,
                  const wgpu::BindGroupLayout& frame, const BeamSettings& settings);
// The pass's group over a frame's depth (again after a resize).
void bind_beam_targets(gpu::Device& device, Beams& beams, const FrameTargets& targets);

// CPU (no GPU): this frame's beams and burns staged in `beams`' reserve, camera-relative to
// `view`'s eye. Allocates nothing.
void stage_beams(Beams& beams, std::span<const BeamDraw> draws, std::span<const BurnVertex> burns,
                 const FrameView& view, float hot_ember_cd);
// stage_beams, then the staging uploaded.
void prepare_beams(gpu::Device& device, Beams& beams, std::span<const BeamDraw> draws,
                   std::span<const BurnVertex> burns, const FrameView& view, float hot_ember_cd);

void encode_beams(const Beams& beams, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                  const wgpu::BindGroup& frame);

}  // namespace ge::render
