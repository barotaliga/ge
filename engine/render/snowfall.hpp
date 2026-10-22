// THE FALLING SNOW ROUND THE CAMERA (docs/rendering.md § Particles and figures): a fixed cloud of
// flakes in a box round the eye, carried by the wind and falling at a flake's pace, flown by a
// compute pass and drawn as soft streaks.
//
// - THE BOX: `SnowfallSettings::flakes` flakes live in a cube `box` m on a side centred on the
//   eye. A flake that leaves it (the eye moved, or the wind carried it out) comes back in at the
//   far side (its offset from the eye wrapped), so the eye always stands in a full fall however
//   fast it moves, and no flake is ever born or killed. A flake fades out toward the box's faces
//   so the wrap is never seen.
// - THE FLIGHT ("snowfall.sim", one thread a flake): the air's velocity (the game's wind), the
//   flake's own fall (0.6 to 1.4 m/s, its own by its index), and a flutter of a few tenths of a
//   metre a second swinging on the flake's own phase.
// - THE DRAW ("snowfall", after the particles): only the first `fall` share of the flakes, so a
//   flurry is a scatter and a blizzard the whole box. A flake is a STREAK along its motion over
//   the shutter (1/60 s) at least `size` m long (widened to a pixel and a half where it is
//   smaller, and thinned as much), lit by the sun past the ground's horizon and the
//   sky, hazed, softened against the depth, blended premultiplied, and written to the motion
//   target's reactive channel as the particles are.
//
// ge:summary The falling snow: flakes in a wrapped box round the eye, flown by the wind, drawn
// ge:keywords render snowfall flakes snow falling wrap box wind flutter streak blizzard flurry
// ge:keywords create_snowfall prepare_snowfall encode_snowfall_sim encode_snowfall
// ge:skill render
// ge:test tests/render_snowfall_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/terrain.hpp"

namespace ge::render {

struct SnowfallSettings {
  uint32_t flakes = 24576;  // in the box at a fall of 1
  float box = 36.0f;        // m on a side
  float size = 0.03f;       // a flake's width, m (a clump of flakes; a crystal is too small to see)
  float soft_m = 0.3f;      // the depth over which a flake fades into what is behind it
};

// The fall at a frame: what `prepare_snowfall` is handed.
struct SnowfallFrame {
  double eye[3] = {0, 0, 0};   // world m
  double wind[3] = {0, 0, 0};  // the air's velocity, m/s
  double fall = 0;             // 0 (none drawn) .. 1 (every flake)
  double dt = 0;               // s since the last frame; 0 holds every flake still
  double time = 0;             // s, the flutter's clock
  double near_m = 0.1;         // the camera's near plane
};

struct Snowfall {
  SnowfallSettings settings;
  wgpu::Buffer flakes, params;
  wgpu::BindGroupLayout sim_layout, draw_layout, light_layout;
  wgpu::BindGroup sim_group, draw_group, light_group;
  uint32_t p_sim = 0, p_draw = 0;
  uint32_t drawn = 0;  // this frame's flakes drawn
};

// The flakes (spread through the box round the origin; the first frame wraps them round the eye),
// the pipelines (into `pipelines`, before the load ends) and the groups. `frame` is group 0's
// layout.
void create_snowfall(gpu::Device& device, gpu::PipelineSet& pipelines, Snowfall& snowfall,
                     const wgpu::BindGroupLayout& frame, const Terrain& terrain,
                     const SnowfallSettings& settings = {});

// The draw's group over a frame's depth (again after a resize).
void bind_snowfall_targets(gpu::Device& device, Snowfall& snowfall, const FrameTargets& targets);

// How many flakes a fall of `fall` draws.
uint32_t flakes_drawn(const SnowfallSettings& settings, double fall);

// The frame's upload.
void prepare_snowfall(gpu::Device& device, Snowfall& snowfall, const SnowfallFrame& at);

// The two passes: "snowfall.sim" and "snowfall" (after the particles). Both run, empty, when
// nothing falls, so the pass table keeps its rows.
void encode_snowfall_sim(const Snowfall& snowfall, const gpu::PipelineSet& pipelines,
                         FrameRecorder& rec, wgpu::CommandEncoder& encoder);
void encode_snowfall(const Snowfall& snowfall, const gpu::PipelineSet& pipelines,
                     FrameRecorder& rec, wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                     const wgpu::BindGroup& frame);

// Labs and tests (blocks): every flake's position (xyz) and phase (w).
std::vector<float> read_flakes(gpu::Device& device, const Snowfall& snowfall);

}  // namespace ge::render
