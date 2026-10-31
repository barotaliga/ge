// LAVA AND SLIME DRAWN (docs/liquids.md § Drawn): a body of a hot or a thick liquid from
// engine/water (`water::Body`: its level, its box) drawn over the opaque scene by its look, in a
// pass of its own after the water's ("liquid.copy", then "liquid"). Every look is a row of knobs
// a game tunes; the named rows match the liquids of the same name (water/liquids.hpp).
//
// - LAVA (`LavaLook`): the surface's TEMPERATURE is the picture, and its colour and brightness
//   follow from it as a black body's glow (water/glow.hpp's table, Planck under the CIE
//   observer: no colour is picked). The crust is a field of plates (a Voronoi tiling at the
//   plate's size) carried by the flow on two phases of a flow map so it never stretches; each
//   plate has an age, which cools its skin from `crust_hot` to `crust_cold` (dull red to black)
//   and widens the cracks between it and its neighbours, where the melt's own heat shows, falling
//   off exp(-d / w) from the crack's line. Over the plates lie the open patches (a slow noise over
//   `cover`), molten and nearly at `core`, a thin skin wrinkling on them; the crust's edge at a
//   patch is the hottest. Pahoehoe's ropy folds run across the flow on the crust. The crust is a
//   rough black glass lit by the sun and the sky; the melt a smoother one; both glow.
// - SLIME (`SlimeLook`): a turbid volume. Its own colour is the Kubelka-Munk reflectance of a
//   thick layer, R = 1 + K/S - sqrt((K/S)^2 + 2 K/S) per band (K the absorption, S the
//   scattering), so a gel with little scattering shows what lies in it and a toxic ooze is an
//   opaque green; what lies behind is seen through exp(-(K + S) d) of it. On the surface: the
//   wind's ripples cut by the liquid's viscosity (water/liquid.hpp's `ripples`), bubbles that
//   swell and burst into a ring, a scum of patches drifting with the surface, a thin oily film's
//   interference colours (the sheen on tar), a gloss of its own and a glow of its own.
//
// - ITS LIGHT ROUND IT (`liquid_lamps`): a glowing body (lava, a slime with a glow of its own)
//   handed to the frame's lamps (render/lights.hpp) as a few point lights over its plan, one a
//   cell of a grid laid on its box, each as bright as its cell of surface: a flat face of luminance
//   L and area A sends L A cos(theta) cd at theta off its normal, taken at 60 degrees (the walls
//   and the banks round a lake see it low), so L A / 2; lifted over the surface by its cell's
//   radius and softened by it (an area's light, not a point's), its colour the glow's own. Lava's L
//   is the share of open melt at the core's glow and the crust's at a share of its young skin's
//   (the cracks and the plates still hot). Steady: the melt changes slower than a frame.
//
// - THE SCENE IN IT (`Liquids::reflections`): the reflected ray marched through the scene's own
//   depth copy (24 steps growing with distance, then halved down onto the hit), the colour copy
//   read where it meets the scene, faded toward the screen's edges and the march's end, the sky's
//   own light where it meets nothing. The Hi-Z pyramid holds each texel's FARTHEST depth (a
//   cull's bound), so it cannot skip empty space for a ray, which needs the nearest: the full
//   depth copy the pass already makes is marched instead.
// - THE HEAT SHIMMER (`LavaLook::shimmer`): the air over the melt is hot, and a ray through it is
//   bent by its rising cells, a few thousandths of a radian over a lake; the bend is summed along
//   the ray through a layer kShimmerHeight deep, strongest at the surface. What is seen across
//   the lake is drawn again through the bent rays (a box over the body, after the surfaces), and
//   the lava's own surface is looked up where its bent ray lands.
// - THE CRUST BROKEN (`break_liquid_crust`): a hole of open melt where something fell in, opening
//   at once and skinning over from its edge, the plates round it cracked and glowing, a crest
//   running out in the first second; carried by the flow.
// - SEEN FROM BELOW (`Liquids::eye_under`): an eye under a body's level, inside its box, sees the
//   liquid round it in one full-screen pass after the surfaces. A SLIME is a fog of its own
//   colour: the scene fades into the Kubelka-Munk colour of a thick layer, lit by the light
//   that reaches the eye's depth (dimmed by the two-stream diffuse attenuation sqrt(K (K + 2 S))),
//   plus its own glow, over exp(-(K + S) d) of the path; the surface from below shows the world
//   over it within Snell's window (the ray refracted out, the transmitted share by Fresnel) and,
//   past it, the slime below mirrored (total internal reflection: its veil); its scum a shadow
//   on the window. A LAVA is opaque: the melt's own glow fills the view, cooler streaks rising
//   through it, the eye keyed to the melt (0.6 of a white face) so it stays orange. A thick slime's
//   few centimetres of view is honest and unplayable, so `under_reach` (m, 0 none) is a game's
//   least view: no band's attenuation is let past 3 / reach.
//
// Not yet (docs/liquids.md § Not built yet): a flow region's surface; the waterline split for an
// eye at the level (render/underwater.hpp's lens plate).
//
// ge:summary Lava and slime drawn: crust plates, cracks and blackbody glow; turbid slime, bubbles
// ge:keywords render liquid lava slime crust plates cracks voronoi flow-map blackbody glow
// ge:keywords incandescence pahoehoe ropes kubelka-munk turbid bubbles scum iridescence
// ge:keywords thin-film sheen toxic gel mucus sludge tar create_liquid add_liquid_body
// ge:keywords lava_look slime_look liquid_look prepare_liquid encode_liquid liquid_lamps
// ge:keywords glowing-body lava-light area-light
// ge:keywords reflections screen-space-reflections ssr heat-shimmer heat-haze crust-broken
// ge:keywords break_liquid_crust splash under-lava under-slime from-below eye_under under_reach
// ge:keywords snell-window total-internal-reflection
// ge:skill render water
// ge:test tests/render_liquid_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/lights.hpp"
#include "water/optics.hpp"
#include "water/world.hpp"

namespace ge::render {

inline constexpr uint32_t kLiquidGrid = 96;  // the surface's quads a side (liquid.wgsl's override)
inline constexpr int kLiquidRipples = 16;    // the ripple components a body draws
inline constexpr int kGlowEntries = 32;      // the glow table's entries
inline constexpr double kGlowLo = 600;       // K: the table's first (under any visible glow)
inline constexpr double kGlowHi = 1700;      // K: its last
inline constexpr int kLiquidBreaks = 8;      // the crust's breaks a body remembers (the latest)
inline constexpr double kShimmerHeight = 3;  // m: the hot air's layer over lava, drawn as a box

enum class LiquidKind : uint8_t { lava, slime };

struct LavaLook {
  double core = 1150;       // degrees C: the melt
  double crust_hot = 820;   // degrees C: a crust's skin as it forms
  double crust_cold = 380;  // degrees C: an old crust's (black in daylight)
  double cover = 0.75;      // the share crusted over
  double plate = 1.6;       // m: a plate's size
  double crack = 0.06;      // m: a young plate's crack, the glow's fall-off from its line
  double ropes = 1;         // the ropy folds on the crust (pahoehoe), 0 none
  double age_rate = 0.02;   // 1/s: how fast a plate ages and is renewed
  double emissivity = 0.9;
  double crust_rough = 0.35;  // the crust's glassy rind
  double melt_rough = 0.12;
  water::Rgb crust_albedo{0.045, 0.041, 0.038};
  double patch = 0;  // m: the open melt's patches' size (0: seven plates)
  // rad/m: the heat shimmer, how far a metre of the hot air over the melt bends a ray (0 none).
  double shimmer = 0.0008;
};

struct SlimeLook {
  water::Rgb absorb{1, 1, 1};   // K, 1/m
  water::Rgb scatter{1, 1, 1};  // S, 1/m
  water::Rgb glow{0, 0, 0};     // its own light, cd/m^2 (a toxic ooze's)
  double rough = 0.08;          // the surface's gloss (a GGX roughness)
  double film = 0;              // nm: an oily film's mean thickness (0 none; 200..600 a sheen)
  double scum = 0;              // the share a scum covers
  water::Rgb scum_albedo{0.2, 0.2, 0.15};
  double bubbles = 0;         // bubbles a square metre a second
  double bubble_size = 0.04;  // m: a bubble's radius when it bursts
  double pulse = 0;           // the glow's slow pulse, a share of it
  double grime = 0;           // 0..1: silt and rot mottled dark over the pool (a sewer's)
  double debris = 0;          // floating bits (twigs, leaves, clumps) a square metre
};

struct LiquidLook {
  LiquidKind kind = LiquidKind::slime;
  LavaLook lava;
  SlimeLook slime;
  double flow[2] = {0, 0};  // m/s: the surface's own motion (a channel's current, the drift)
  // Under a roof (a cave's lava, a sewer's slime): the share of the dome the surface sees the sky
  // through, and the room's own light round it, a share of a white face's luminance under the
  // open day, over the rest.
  double open_sky = 1;
  double room = 0;
};

// The named looks: "lava", "lava-crusted", "slime", "slime-gel", "slime-mucus", "slime-sludge",
// "slime-tar" (the rows of water/liquids.hpp), and nullptr for any other name.
const LiquidLook* find_liquid_look(std::string_view name);
std::span<const std::string_view> liquid_look_names();

struct LiquidGpuBody {
  float info[4];     // level, kind (0 lava, 1 slime), ripples, the eye's footprint scale
  float box[4];      // x0, z0, x1, z1
  float flow[4];     // flow x, z (m/s), the open sky's share, the room's light
  float lava0[4];    // core K, crust_hot K, crust_cold K, cover
  float lava1[4];    // plate, crack, ropes, age_rate
  float lava2[4];    // emissivity, crust_rough, melt_rough, 0
  float crust[4];    // the crust's albedo
  float absorb[4];   // slime K
  float scatter[4];  // slime S
  float glow[4];     // slime's own light, cd/m^2; w: the pulse
  float slime0[4];   // rough, film nm, scum, bubbles
  float slime1[4];   // bubble_size, grime, debris, 0
  float scum[4];     // the scum's albedo
  float fx[4];       // reflections on (1) or off, the shimmer (rad/m), the breaks held, reach
  float breaks[kLiquidBreaks][4];  // x, z (m), radius (m), the time it fell in (s)
  float table[kGlowEntries][4];    // the glow table, cd/m^2 rgb (water/glow.hpp)
};

struct Liquids {
  wgpu::BindGroupLayout layout, copy_layout;
  uint32_t p_copy = 0, p_draw = 0, p_shimmer = 0, p_under = 0;
  bool reflections = true;  // the scene mirrored in the surface (else the sky's alone)
  double under_reach = 0;   // m: the least an eye under a body sees (0: as thick as it is)
  int eye_under = -1;       // this frame's: the body the eye is under, or -1
  wgpu::Texture scene_colour, scene_depth;
  wgpu::TextureView scene_colour_view, scene_depth_view;
  wgpu::BindGroup copy_group;
  wgpu::Buffer indices;
  struct Slot {
    LiquidGpuBody staged{};
    wgpu::Buffer uniform, ripples;
    wgpu::BindGroup group;
    bool hidden = false;
    bool shown = true;
    int next_break = 0;
    double floor = 0;  // m: the lowest its liquid reaches (an eye under it is not in it)
  };
  std::vector<Slot> bodies;
};

// The luminance an eye adapts to looking at this look (FrameView::adapt_cd), cd/m^2: `key` times
// its brightest part's (lava's open melt, a slime's glow). At 1 the melt is keyed as a white face
// is; its red is so saturated that the tone curve's shoulder whitens it, so a view of lava keys
// it higher (1.5 holds it orange, as a photograph exposed for the lava does). 0 for a look that
// does not glow.
double adapt_for(const LiquidLook& look, double key);

// The share of a crust's young skin's glow its whole face sends, the cracks and the plates still
// hot over the cold ones (`liquid_lamps`).
inline constexpr double kCrustGlowShare = 0.2;
// The most lamps a body hands the frame (a grid of at most this many cells over its box).
inline constexpr int kLiquidLampsMost = 16;
inline constexpr double kLiquidLampReach = 150;  // m: the most a body's lamp reaches

// The mean luminance a body in `look` sends off its face, cd/m^2 (linear sRGB, luminance in it):
// 0 for a look that does not glow.
water::Rgb liquid_glow(const LiquidLook& look);
// Append the lamps `body` in `look` lights its surroundings with: a cell of `cell_m` (fewer and
// larger past kLiquidLampsMost) a lamp; none for a look that does not glow.
void liquid_lamps(const water::Body& body, const LiquidLook& look, std::vector<LocalLight>& out,
                  double cell_m = 10);

// The pipelines into the manifest.
void create_liquid(gpu::Device& device, gpu::PipelineSet& pipelines, Liquids& liquids,
                   const wgpu::BindGroupLayout& frame);
// The scene's copies at the targets' size (again when the targets change).
void create_liquid_targets(gpu::Device& device, Liquids& liquids, const FrameTargets& targets);
// A body drawn in `look`, its ripples grown by `wind` m/s toward `toward` over `fetch` m and cut
// by the body's liquid; its index.
int add_liquid_body(gpu::Device& device, Liquids& liquids, const water::Body& body,
                    const LiquidLook& look, double wind = 0, double toward = 0, double fetch = 10);
void set_liquid_body(gpu::Device& device, Liquids& liquids, int index, const water::Body& body,
                     const LiquidLook& look, double wind = 0, double toward = 0, double fetch = 10);
// Uploads this frame's; a body seen from under its level is not drawn, and the one the eye is
// inside (under its level, over its floor, in its box) is `eye_under`, drawn round the eye.
void prepare_liquid(gpu::Device& device, Liquids& liquids, const FrameView& view);
// Something fell into body `index` at (x, z) at time `t` (s, the frame's clock), breaking its
// crust over `radius` m: the melt shows through a hole that opens at once and skins over again,
// the broken plates round it glowing hot (lava; a slime ignores it). The latest kLiquidBreaks are
// kept.
void break_liquid_crust(Liquids& liquids, int index, double x, double z, double radius, double t);
// "liquid.copy" and "liquid" (the surfaces, then the shimmer over lava, then the view from under
// `eye_under`).
void encode_liquid(const Liquids& liquids, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame);

}  // namespace ge::render
