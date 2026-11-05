// HEAT HAZE (docs/rendering.md § Heat haze): the air something hot has just passed through bends
// the light crossing it, as the air over a road in the sun does. Nothing is added to the picture;
// what is behind the hot air is seen a little displaced, the displacement wandering as the hot
// cells rise and mix, and gone once the air has cooled. A nail's flight leaves a thread of it, a
// rocket's motor a wide wake.
//
// - A TRAIL is laid a stretch a frame (`HazeTrail`: from where the hot thing was to where it is,
//   the frame's time): each stretch ages on its own, so the trail is hottest at its head and fades
//   toward its tail. A game keeps its stretches (a few hundred at most) and hands them over each
//   frame; the pass keeps nothing between frames.
// - THE LOOK (`HazeLook`, presets by id): how wide the hot air is when laid and how fast it
// spreads,
//   how far it bends a ray at its hottest (`bend_mrad`: a few milliradians, the size of a road's
//   mirage, a pixel or three on a 1080-line screen), the cells' size, how fast it rises and how
//   long it lasts. `haze_level` is a stretch's heat at a moment: up in a few milliseconds, then
//   down as (1 - age/life)^2. Pure: a replay and a lab bend the same.
// - THE DRAW ("haze.copy", then "haze"): the frame's colour so far is copied, then every stretch is
//   a ribbon turned to the eye (the beams' ribbon) whose pixels read that copy at a point pushed
//   off by two octaves of smooth noise drifting up through the air, the push largest along the
//   ribbon's middle and nothing at its edges. A push that would land on something nearer than the
//   ribbon keeps the pixel's own colour, so a hand in front never smears into the air behind it.
//   The scene's depth is read, never written; the ribbon is hidden behind what is nearer.
//
// ge:summary Heat haze: a hot trail's air bending what is behind it, laid a stretch a frame
// ge:keywords render haze heat-haze heat-shimmer mirage refraction distortion trail nail rocket
// ge:keywords exhaust wake ribbon scene-copy haze_level haze_look prepare_haze encode_haze
// ge:skill render
// ge:test tests/render_haze_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxHaze = 2048;  // stretches drawn a frame (the newest kept)

struct HazeLook {
  std::string_view id;
  float width_m = 0.06f;    // the hot air's width when laid
  float spread_mps = 0.1f;  // how fast it widens as it mixes, m/s
  float bend_mrad = 1.5f;   // the most it bends a ray, at its hottest along its middle
  float cell_m = 0.05f;     // the hot cells' size
  float rise_mps = 0.3f;    // how fast the hot air rises
  float life_s = 0.25f;     // until it has cooled
};

// The presets: "haze.nail" (a hot nail's thread, a quarter of a second) and "haze.rocket" (a
// motor's wide wake, rising, most of a second). Null for an id not known.
const HazeLook* haze_look(std::string_view id);
std::span<const HazeLook> haze_looks();

// One stretch of a trail, laid on the frame at `born_s` from `a` to `b` (world, m).
struct HazeTrail {
  double a[3] = {0, 0, 0};
  double b[3] = {0, 0, 0};
  double born_s = 0;
  const HazeLook* look = nullptr;
  uint32_t seed = 0;
};

// Its heat at `now_s`, 0..1: up over its first 10 ms, then (1 - age/life)^2; 0 before and after.
double haze_level(const HazeTrail& t, double now_s);
// Whether it has cooled by `now_s` (a game drops it then).
bool haze_cold(const HazeTrail& t, double now_s);

// wgsl's HazeSeg.
struct GpuHazeSeg {
  float a[4];  // camera-relative start; the heat
  float b[4];  // camera-relative end; the half width, m
  float c[4];  // the bend (rad), the cells' size (m), how far it has risen (m), the seed (0..1)
};

struct GpuHazeParams {
  float info[4];  // stretches, the near plane (m), the frame's time (s), 0
};

struct Haze {
  wgpu::BindGroupLayout layout, copy_layout;
  uint32_t p_copy = 0, p_ribbon = 0;
  wgpu::Buffer params, segs;
  GpuHazeParams staged{};
  wgpu::Texture scene;
  wgpu::TextureView scene_view;
  wgpu::BindGroup group, copy_group;
  std::vector<GpuHazeSeg> staging;  // kMaxHaze, reserved at create
  uint32_t count = 0;               // this frame's
};

// The pipelines into the manifest.
void create_haze(gpu::Device& device, gpu::PipelineSet& pipelines, Haze& haze,
                 const wgpu::BindGroupLayout& frame);
// The frame's copy at the targets' size (again when the targets change).
void bind_haze_targets(gpu::Device& device, Haze& haze, const FrameTargets& targets);
// This frame's stretches still warm at `view.time_s`, camera-relative (the newest kMaxHaze).
// Allocates nothing.
void stage_haze(Haze& haze, std::span<const HazeTrail> trails, const FrameView& view);
// stage_haze, then the upload.
void prepare_haze(gpu::Device& device, Haze& haze, std::span<const HazeTrail> trails,
                  const FrameView& view);
// "haze.copy" and "haze": both recorded every frame, empty when nothing is warm.
void encode_haze(const Haze& haze, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                 const wgpu::BindGroup& frame);

}  // namespace ge::render
