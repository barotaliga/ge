// THE LIGHTNING'S CHANNEL DRAWN (docs/storms.md § The storm drawn): a flash's segments as
// glowing ribbons over the picture, before the resolve and the bloom, so the bloom spreads the
// channel's glare as an eye sees it.
//
// - A SEGMENT (`BoltSegment`) carries its two ends (world m), the channel's true width (m) and its
//   light as LUMINOUS INTENSITY PER METRE (cd/m, seen side on, the frame's mean): a game shares
//   its flash's candela (engine/wind/lightning.hpp's `light_of` times `kUnitCandela`) out along
//   the channel by each segment's glow and length, so the drawn channel and the light it casts
//   (render/wgsl/flash.wgsl) are one flash.
// - THE DRAW ("bolts"): one camera-facing quad a segment, two widths across with a Gaussian
//   core, the width never under one and a half pixels at its distance; its luminance the intensity
//   over the drawn width, so a far channel drawn wider is dimmer and its light the same; added to
//   the HDR target, dimmed by the fog's or the fall's extinction on the way, depth-tested (a ridge
//   hides a strike behind it), and marked reactive (motion b) so the resolve does not smear a
//   stroke into the frames after it.
//
// ge:summary The lightning's channel drawn: glowing ribbons, light per metre, before the bloom
// ge:keywords render lightning bolt bolts channel ribbon glow strike encode_bolts prepare_bolts
// ge:keywords create_bolts bolt-segment
// ge:skill render water
// ge:test tests/render_bolts_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

// One segment, as bolts.wgsl's `Segment` reads it.
struct BoltSegment {
  float a[3];     // world m
  float width;    // the channel's true width, m
  float b[3];     // world m
  float candela;  // its luminous intensity per metre, cd/m (the frame's mean)
};
static_assert(sizeof(BoltSegment) == 32, "match bolts.wgsl's Segment");

struct Bolts {
  uint32_t capacity = 0;
  uint32_t count = 0;  // this frame's
  wgpu::Buffer segments, params;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_draw = 0;
};

// The buffers (`capacity` segments) and the pipeline (into `pipelines`, before the load ends);
// `frame` is group 0's layout.
void create_bolts(gpu::Device& device, gpu::PipelineSet& pipelines, Bolts& bolts,
                  const wgpu::BindGroupLayout& frame, uint32_t capacity = 4096);

// This frame's segments (past `capacity` are left out), seen from `view` at `render_height` rows.
void prepare_bolts(gpu::Device& device, Bolts& bolts, std::span<const BoltSegment> segments,
                   const FrameView& view, uint32_t render_height);

// The pass "bolts" (after the sky and the cloud, before the resolve); it runs empty with none.
void encode_bolts(const Bolts& bolts, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                  const wgpu::BindGroup& frame);

}  // namespace ge::render
