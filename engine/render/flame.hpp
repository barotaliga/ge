// FLAMES (docs/rendering.md § Flames): the fire on a torch, a candle, a brazier or a camp fire,
// drawn as burning gas rather than a sprite or a model, with the light it casts.
//
// - THE GAS: each flame is a volume marched per pixel ("flames", after the sky, before the
//   resolve). Its body is a teardrop standing on `base` (narrow at the fuel, widest an eighth of
//   the way up, tapering to a tip at `height_m`), torn by turbulence that rises with the hot gas
//   (three octaves of 3D value noise carried up at `rise_mps`) and grows toward the tip, so the
//   top breaks into licking tongues. The gas is optically thin: it only adds light, so a flame
//   never darkens what is behind it, and the march ends at the scene's depth (a bracket or the
//   torch's head hides the flame behind it).
// - THE COLOUR is blackbody's: the hottest gas on the axis is `temperature_k`, cooling toward the
//   rim and the tip, and every temperature's colour and relative brightness come from Planck's law
//   integrated against the CIE 1931 observer (render/blackbody.hpp, its table the shader looks up).
//   So the core is yellow-white, the rim orange and the tongues red, and they dim the way a
//   cooling gas does. A gas or a candle flame has a blue base (`blue`), the light of burning
//   radicals rather than of soot.
// - THE LEVEL: a flame and the light it casts flicker as one. `flame_light` makes its
//   `LocalLight` (render/lights.hpp): at the gas's centre, the colour of its mean temperature, its
//   intensity its luminance times its area seen side on (a torch's 40 to 150 cd), flickering as a
//   flame with the flame's seed; `prepare_flames` reads that light's level and the gas swells and
//   brightens with it. A game puts the light in its lights list beside its other lamps; one whose
//   lights flicker by their own rules sets `level` to its light's and the gas follows that.
// - THE AIR: `wind` (m/s, the air moving past it) bends the flame downwind, more toward the tip.
//
// Presentation only: the flames are a pure function of the list and the frame's time, and feed no
// simulation.
//
// ge:summary Flames drawn as burning gas: a marched teardrop of turbulent blackbody emission
// ge:keywords render flame fire torch candle brazier campfire bonfire blackbody planck cie
// ge:keywords colour-temperature kelvin turbulence value-noise volume ray-march emission additive
// ge:keywords flame_light prepare_flames encode_flames blackbody flame-flicker wind-lean
// ge:skill render
// ge:test tests/render_flame_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/blackbody.hpp"
#include "render/frame.hpp"
#include "render/lights.hpp"
#include "render/view.hpp"

namespace ge::render {

struct Flame {
  double base[3] = {0, 0, 0};   // world m: where the gas leaves its fuel
  float height_m = 0.4f;        // the tip's height at full level
  float radius_m = 0.07f;       // the body's radius at its widest
  float temperature_k = 1900;   // the hottest gas, on the axis low down (K)
  float luminance_nits = 9000;  // the core's luminance at full level, cd/m^2
  float wind[3] = {0, 0, 0};    // m/s: the air moving past it
  float rise_mps = 1.0f;        // how fast the hot gas rises (the turbulence carried up)
  float rate_hz = 9;            // its flicker's rate
  float depth = 0.35f;          // its flicker's swing, 0..1
  float blue = 0.0f;            // the blue base's share: 0 a pitch torch, 0.6 a candle
  uint32_t seed = 0;            // its own turbulence, so two torches never burn as one
  float level = -1;             // >= 0: the level the caller's own light is at (the gas follows
                                // it); < 0: its own flicker (`flame_level`)
};

// Presets, standing at `base`.
Flame torch_flame(const double base[3], uint32_t seed);     // a pitch or wood torch, 0.4 m
Flame candle_flame(const double base[3], uint32_t seed);    // a candle's, 4 cm
Flame brazier_flame(const double base[3], uint32_t seed);   // a brazier's or a big torch's, 0.8 m
Flame campfire_flame(const double base[3], uint32_t seed);  // a camp fire's, 1.2 m

// The light the flame casts (render/lights.hpp): flickering as the flame does, with its seed.
LocalLight flame_light(const Flame& flame);

// The flame's level at `t_s`: its light's (`flicker_level` of `flame_light`).
float flame_level(const Flame& flame, double t_s);

// The world box the flame's gas stays inside at any level and in its wind (min, max).
void flame_bounds(const Flame& flame, double lo[3], double hi[3]);

inline constexpr uint32_t kMaxFlames = 256;

// wgsl's `GpuFlame`, four rows a flame.
struct GpuFlame {
  float base[4];   // camera-relative base, m; the height at this level
  float shape[4];  // radius; the hottest gas, K; the core's luminance, cd/m^2; the seed's phase
  float wind[4];   // the tip's offset downwind per metre of height (x, z); the rise, m/s; the blue
  float bound[4];  // the bounding sphere's centre (camera-relative xyz) and radius
};

struct Flames {
  wgpu::Buffer params, list;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t pipeline = 0;
  uint32_t count = 0;    // this frame's
  uint32_t dropped = 0;  // past kMaxFlames this frame (the farthest)
  std::vector<GpuFlame> staged;
  std::vector<uint32_t> order;
  std::vector<float> key;
};

// The pass's pipeline (into `pipelines`, before the load ends) and its buffers. `frame` is
// group 0's layout.
void create_flames(gpu::Device& device, gpu::PipelineSet& pipelines, Flames& flames,
                   const wgpu::BindGroupLayout& frame);
// The group over a frame's depth (again after a resize).
void bind_flame_targets(gpu::Device& device, Flames& flames, const FrameTargets& targets);
// This frame's flames, at `view`'s time and eye (the nearest kMaxFlames kept).
void prepare_flames(gpu::Device& device, Flames& flames, std::span<const Flame> list,
                    const FrameView& view);
// The pass: "flames", additive into the picture, after the opaque passes and the sky.
void encode_flames(const Flames& flames, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame);

}  // namespace ge::render
