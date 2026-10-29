// LOCAL LIGHTS (docs/rendering.md § Local lights): the lamps, torches, flames, flashes and
// headlamps a game sets in its world beside the sun and the sky. A game keeps its own list of
// `LocalLight`s (where each one is, what it is, how it flickers) and hands it to `prepare_lights`
// once a frame; the renderer never stores a light between frames.
//
// - UNITS: a light's `candela` is its luminous intensity (cd) at full level, so a point `d` m off
//   is lit by candela / d^2 lux, in the same units the sun's 128 000 lx is; a 60 W bulb is about
//   70 cd, a torch's flame 10 to 40, a street lamp 1000 to 3000, a muzzle flash 10^5 for a few
//   milliseconds. The shader scales them by the frame's pre-exposure, as it scales the sun.
// - THE REACH: every light ends at its `range_m` (a smooth window, (1 - (d/r)^4)^2, on top of the
//   inverse square), so it lights a bounded sphere and the clusters can drop it outside. The
//   emitter's `radius_m` caps the inverse square at its own surface.
// - A SPOT shines along `direction` inside a cone: whole inside `inner_deg` of the axis, none
//   past `outer_deg`, a smooth fall between; `outer_deg` 0 is a point light (every way).
// - FLICKER (`flicker_level`, a pure function of the light and the frame's time, so a replay and
//   a lab flicker the same): steady; a flame (two octaves of smooth noise, a fire's 8 to 12 Hz
//   waver); a buzz (a failing tube: mostly on, cut out in short dark gaps); a strobe (a square
//   wave); a pulse (a flash born at `born_s`, up in `attack_s`, down by e every `decay_s`: a
//   muzzle flash, an explosion, a spark); and STEPS, a pattern of brightness letters played at
//   `rate_hz` letters a second, 'a' dark, 'm' the light's full level (a step is 1/12), 'z' 25/12 of
//   it, so a map's author can write any pattern ("mmnmmommommnonmmonqnmmo" a fluorescent's
//   flutter).
// - THE CLUSTERS (`bin_lights`, CPU, deterministic, held by tests/render_lights_test.cpp): the
//   view is cut into kTilesX x kTilesY screen tiles and kSlices depth slices (the first
//   [0, kSliceNear] m, then logarithmic to kSliceFar, the last to infinity), and each cluster
//   lists the lights whose sphere (or cone's sphere) touches its box. A pixel reads its own
//   cluster's list, so a frame of hundreds of lamps costs a pixel only the few that reach it.
//   At most kMaxLights lights a frame (the nearest kept), kMaxLightRefs entries in all.
// - THE BUFFER: one storage buffer (wgsl/lights.wgsl's `LightGrid`), group 0 binding 10 of every
//   draw and compute pass (`frame_group`'s `lights`), read by `local_lights` for a surface and
//   `light_inscatter` for a point in the air (a fog volume's march).
//
// Shadows of local lights are the next row (docs/goals/M39.md); a light's `shadow` slot is
// carried already and is -1 (none).
//
// ge:summary Local point and spot lights: units, reach, flicker and pulses, clustered binning
// ge:keywords render lights local-lights point-light spot-light lamp torch flame muzzle-flash
// ge:keywords flicker pulse strobe buzz light-style steps candela lux inverse-square window
// ge:keywords cluster clustered-shading froxel tiles slices bin_lights prepare_lights
// ge:keywords flicker_level light-grid local_lights light_inscatter headlamp street-lamp
// ge:skill render
// ge:test tests/render_lights_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxLights = 256;  // drawn a frame (the nearest kept)
inline constexpr uint32_t kTilesX = 16;      // the clusters' screen tiles across
inline constexpr uint32_t kTilesY = 9;       // and down
inline constexpr uint32_t kSlices = 24;      // and their depth slices
inline constexpr uint32_t kClusters = kTilesX * kTilesY * kSlices;
inline constexpr uint32_t kMaxLightRefs = 32768;  // the clusters' lists in all
inline constexpr double kSliceNear = 2.0;         // m: slice 0 is [0, kSliceNear]
inline constexpr double kSliceFar = 2000.0;       // m: the last slice runs on from here

enum class Flicker : uint8_t { steady, flame, buzz, strobe, pulse, steps };

struct LocalLight {
  double position[3] = {0, 0, 0};      // world, m
  float direction[3] = {0, -1, 0};     // a spot's axis, the way it shines (unit)
  float colour[3] = {1, 1, 1};         // linear rgb, its luminance about 1
  float candela = 100;                 // luminous intensity at full level, cd
  float range_m = 10;                  // where its light ends
  float radius_m = 0.05f;              // the emitter's size: the inverse square stops here
  float inner_deg = 0, outer_deg = 0;  // a spot's cone off its axis; outer 0: a point light
  Flicker flicker = Flicker::steady;
  float rate_hz = 10;   // a flame's waver, a buzz's or strobe's beat, the steps' letters a second
  float depth = 0.35f;  // a flame's or buzz's swing, 0..1 of its level
  uint32_t seed = 0;    // a flame's or buzz's own noise, so two torches never flicker as one
  double born_s = 0;    // a pulse's birth, the frame's clock
  float attack_s = 0.002f, decay_s = 0.03f;  // a pulse's rise and its fall to 1/e
  const char* steps = nullptr;               // the steps' letters ('a'..'z', 'm' full)
};

// The light's level at time `t_s` (1: its `candela`; a step's 'z' is 25/12, a pulse past its decay
// tends to 0). Pure: the same light and time give the same bits.
float flicker_level(const LocalLight& light, double t_s);

// wgsl/lights.wgsl's `GpuLight`: what the shader reads of a light this frame.
struct GpuLight {
  float at[4];      // camera-relative position, m; 1 / range^2
  float colour[4];  // rgb times candela times the level (cd); the emitter's radius^2
  float axis[4];    // a spot's axis; the cosine of its outer cone (-2: a point light)
  float cone[4];    // 1 / (cos inner - cos outer); the shadow slot (-1: none); 0; 0
};

// wgsl/lights.wgsl's `LightGrid`'s fixed head; the lists follow it in the buffer.
struct LightHead {
  uint32_t info[4];  // lights, kTilesX, kTilesY, kSlices
  float grid[4];     // a tile's width and height, px; the near plane, m; slices a doubling
  float more[4];     // kSliceNear, 0, 0, 0
  float pad[4];
};
inline constexpr uint64_t kLightsOffset = sizeof(LightHead);
inline constexpr uint64_t kCellsOffset = kLightsOffset + uint64_t{kMaxLights} * sizeof(GpuLight);
inline constexpr uint64_t kRefsOffset = kCellsOffset + uint64_t{kClusters} * 8;
inline constexpr uint64_t kLightBufferBytes = kRefsOffset + uint64_t{kMaxLightRefs} * 4;

// The CPU's half of a frame's lights, reserved at create time: the lights kept, each cluster's
// (offset, count) into the lists, and the lists.
struct LightBins {
  LightHead head{};
  std::vector<GpuLight> lights;  // kMaxLights
  std::vector<uint32_t> cells;   // 2 a cluster
  std::vector<uint32_t> refs;    // kMaxLightRefs
  std::vector<uint32_t> order;   // scratch: the lights by distance
  std::vector<float> order_key;  // scratch
  uint32_t ref_count = 0;
  uint32_t dropped = 0;  // lights past kMaxLights or refs past kMaxLightRefs this frame
};
void reserve_bins(LightBins& bins);

// The cluster a camera-relative point lands in seen through `cam` on a `width` x `height`
// render (its pixel and its depth along the view), as the shader finds it; -1 behind the eye.
int cluster_of(const CameraMatrices& cam, double near_m, uint32_t width, uint32_t height,
               const double rel[3]);

// Bin `lights` (their levels at `view.time_s`) into the clusters of `view` on a `width` x
// `height` render. Allocates nothing once reserved. Lights at level 0, or whose sphere lies
// wholly outside the view, are dropped.
void bin_lights(LightBins& bins, std::span<const LocalLight> lights, const FrameView& view,
                uint32_t width, uint32_t height);

// The illuminance the lights put on a white face at world point `at` turned to each of them
// (lx, at time `t_s`): what an eye there adapts to (render/view.hpp's `lamp_lux`).
double lamp_lux_at(std::span<const LocalLight> lights, const double at[3], double t_s);

// The GPU half: the buffer group 0 binds at 10 (`frame_group`), and the CPU half.
struct Lights {
  wgpu::Buffer buffer;
  LightBins bins;
};
void create_lights(gpu::Device& device, Lights& lights);
// Bin and upload this frame's lights (only the lists' used prefix is written).
void prepare_lights(gpu::Device& device, Lights& lights, std::span<const LocalLight> list,
                    const FrameView& view, uint32_t width, uint32_t height);

}  // namespace ge::render
