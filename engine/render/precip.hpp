// WHAT FALLS ROUND THE CAMERA (docs/rendering.md § Precipitation): snow, sleet and rain as a fixed
// cloud of particles in a box round the eye, carried by the wind and its eddies and falling at
// each one's pace, flown by a compute pass and drawn as soft streaks, with the rain's splashes on
// whatever surface the depth shows under it. What falls and how hard is the world's to say
// (engine/wind/precip.hpp gives the numbers); this draws it.
//
// - THE BOX: `PrecipSettings::particles` particles live in a cube `box` m on a side centred on
//   the eye. One that leaves it (the eye moved, or the wind carried it out) comes back in at the
//   far side (its offset from the eye wrapped), so the eye always stands in a full fall however
//   fast it moves, and none is ever born or killed. Each fades out toward the box's faces so the
//   wrap is never seen.
// - FLAKES AND DROPS: the first `rain` share of the particles by a rank of their index are drops
//   and the rest flakes, so a rising rain share turns flakes to drops one by one and a sleet is
//   both at once (wgsl/precip_types.wgsl's `is_drop`).
// - THE FLIGHT ("precip.sim", one thread a particle): the air's velocity (the game's wind), the
//   EDDIES (a slow field of swirls some metres across, `swirl` m/s: a storm's snow swirls in
//   sheets; a drop shrugs off most of it), and the particle's own fall: a flake's 0.6 to 1.4 m/s by
//   its index with a flutter on its own phase, a drop's 0.75 to 1.25 of `drop_speed`.
// - THE DRAW ("precip", after the particles): only the first `fall` share of the particles, so a
//   flurry is a scatter and a blizzard the whole box. A flake is a STREAK along its motion over the
//   shutter (1/60 s) at least `size` m long, lit white by the sun past the ground's horizon and the
//   sky; a drop a thin clear line over 1/30 s (the eye smears a drop), taking the sky's light and
//   catching the sun. Both are widened to a pixel and a half where they are thinner (and thinned as
//   much), hazed, softened against the depth, blended premultiplied, and written to the motion
//   target's reactive channel as the particles are.
// - THE SPLASHES (the same draw's next instances): `splashes` crowns at a full rain (a `splash`
//   share of them), each a drop landing that lives `splash_life` s at a spot drawn afresh each
//   life out of the screen and set on the surface the depth shows there, kept where it faces up
//   and within `splash_reach` m, at an even density over the ground. Off the depth, so a splash
//   lands on any game's ground, roof or deck; nothing yet keeps one off a floor under a roof.
//
// ge:summary What falls round the eye: snow, sleet and rain in a wrapped box, the wind and its
// ge:summary eddies, drops as clear streaks, splashes off the depth
// ge:keywords render precip precipitation snowfall rain sleet flakes drops splash streak box wrap
// ge:keywords wind eddies swirl blizzard flurry downpour storm
// ge:keywords create_precip prepare_precip encode_precip_sim encode_precip read_particles
// ge:skill render
// ge:test tests/render_precip_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/terrain.hpp"

namespace ge::render {

struct PrecipSettings {
  uint32_t particles = 24576;  // in the box at a fall of 1
  float box = 36.0f;           // m on a side
  float size = 0.03f;   // a flake's width, m (a clump of flakes; a crystal is too small to see)
  float drop = 0.002f;  // a drop's width, m
  float soft_m = 0.3f;  // the depth over which a particle fades into what is behind it
  uint32_t splashes = 6144;    // at a full rain
  float splash_reach = 24.0f;  // m
  float splash_life = 0.16f;   // s
  float splash_size = 0.07f;   // m: a crown's half-width when it is thrown
};

// The fall at a frame: what `prepare_precip` is handed.
struct PrecipFrame {
  double eye[3] = {0, 0, 0};   // world m
  double wind[3] = {0, 0, 0};  // the air's velocity, m/s
  double fall = 0;             // 0 (none drawn) .. 1 (every particle)
  double rain = 0;             // 0 every particle a flake .. 1 every one a drop
  double drop_speed = 6;       // a drop's fall in still air, m/s (wind/precip.hpp's `fall_speed`)
  double swirl = 0.3;          // the eddies' speed, m/s (wind/precip.hpp's `swirl_of`)
  double splash = 0;           // the share of the splashes drawn, 0 .. 1
  double dt = 0;               // s since the last frame; 0 holds every particle still
  double time = 0;             // s, the flutter's, the eddies' and the splashes' clock
  double near_m = 0.1;         // the camera's near plane
};

struct Precip {
  PrecipSettings settings;
  wgpu::Buffer particles, params;
  wgpu::BindGroupLayout sim_layout, draw_layout, light_layout;
  wgpu::BindGroup sim_group, draw_group, light_group;
  uint32_t p_sim = 0, p_draw = 0;
  uint32_t drawn = 0;     // this frame's particles drawn
  uint32_t splashed = 0;  // this frame's splashes drawn
};

// The particles (spread through the box round the origin; the first frame wraps them round the
// eye), the pipelines (into `pipelines`, before the load ends) and the groups. `frame` is group
// 0's layout.
void create_precip(gpu::Device& device, gpu::PipelineSet& pipelines, Precip& precip,
                   const wgpu::BindGroupLayout& frame, const Terrain& terrain,
                   const PrecipSettings& settings = {});

// The draw's group over a frame's depth (again after a resize).
void bind_precip_targets(gpu::Device& device, Precip& precip, const FrameTargets& targets);

// How many particles a fall of `fall` draws, and how many splashes a splash share of `splash`.
uint32_t particles_drawn(const PrecipSettings& settings, double fall);
uint32_t splashes_drawn(const PrecipSettings& settings, double splash);

// The frame's upload.
void prepare_precip(gpu::Device& device, Precip& precip, const PrecipFrame& at);

// The two passes: "precip.sim" and "precip" (after the particles). Both run, empty, when nothing
// falls, so the pass table keeps its rows.
void encode_precip_sim(const Precip& precip, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder);
void encode_precip(const Precip& precip, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame);

// Labs and tests (blocks): every particle's position (xyz) and phase (w).
std::vector<float> read_particles(gpu::Device& device, const Precip& precip);

}  // namespace ge::render
