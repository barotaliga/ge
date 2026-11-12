// MORPH MESHES DRAWN (docs/rendering.md § Morph meshes): a model animated by whole frames (every
// vertex has a position and a normal a frame, as vertex-animated formats store them) drawn in the
// world, any number of instances, each on its own two frames blended by its own share, in its own
// TINTS, lit by the sun and the sky or by the light a game samples where it stands.
//
// - THE MODEL (`MorphModel`): its frames, its triangles, its texture coordinates and its SKIN.
//   Every model a scene draws is uploaded once (`create_morph`): every frame of every model in
//   one storage buffer (a vertex two vec4: the position, the normal), the triangles and the
//   coordinates in two more, and the skins packed into one atlas (`pack_morph_atlas`), so a frame
//   writes nothing but its draws.
// - THE SKIN is a colour a texel (sRGB) and a MARK a texel: plain, GLOW (it lights itself: an
//   eye, a lamp), a HOLE (`kMarkHole`: nothing drawn there, a game's skin with alpha cut through)
//   or TINTED (`kMarkTintA` / `kMarkTintB` and a STEP 0..15 down a ramp). A tinted
//   texel takes its colour from the instance's ramp for that region at that step, so one skin
//   is drawn in any team's colours with its own shading kept: the step is how light or dark the
//   texel was painted, the ramp what colour that is. The ramps are a table of rows of
//   `kTintSteps` linear colours (`TintRamps`) the game fills: a palette's colour rows, or ramps
//   made from any colour (`ramp_from`).
// - THE DRAW (`MorphDraw`): the model, where it stands and how it is turned and scaled, its
//   two frames and the share between them (a frame drawn between two of the model's at any
//   rate: the frames are 10 a second, the picture 60 or more), the same of the LAST frame (its
//   motion for TAA comes from there, so a limb swinging between frames carries its own motion),
//   its two tints, how it is lit, and whether the skin is read SHARP (each texel a square, the
//   classic look) or SMOOTH (bilinear between the texels after each is resolved to its colour,
//   so a tinted texel never bleeds the wrong ramp into its neighbour).
// - THE LIGHT: `outdoor` (0..1) of the frame's sun and sky (render/wgsl/frame.wgsl's), plus the
//   draw's own AMBIENT and a SHADE from one direction: what a game samples from its world where
//   the model stands (an indoor map's light at the model's feet), so a figure in a dark corridor
//   is dark; in the sun's units, or in the scene's white (`light_in_white`), as a lightmap is.
//   Glowing texels add their own colour times `glow`, whatever the light.
// - THE WARP (`MorphWarp`, render/wgsl/warp.wgsl, shared with the view-model pass): a draw coming
//   into being or going out of it (a spawn, a teleport's arrival or departure) as one share,
//   `shown`, 1 whole and 0 gone. A fragment is drawn where a FIELD over the figure lies under the
//   share: value noise fixed to the model's own frame (so the pattern stays on the body as it
//   turns) plus the point's place along a sweep AXIS (the far end first in, the near end first
//   out). The band at the cut turns to ASH (the surface's light, greyed and tinted), so the figure
//   crumbles; a game throws its flakes where the cut passes (`warp_field` on the CPU says where).
//   Knobs for other looks: its EDGE burning in a light of its own, a PINCH into a column stretched
//   up, a SHIMMER of bands that climb it. The fragments it touches are marked reactive, so TAA
//   does not trail the cut.
// - THE BURN (`MorphBurn`, render/body_burn.hpp's skin): CHARRED blackens it in noise patches fixed
//   to the model that spread as the char grows; EMBERS glow in the char's cracks while it is hot;
//   ETCHED bleaches and pits it (an acid). All three 0: the shader skips it.
// - THE PASS draws after the opaque world (depth tested and written, reversed z, back faces
//   culled), one draw a model instance, at most `kMaxMorphDraws` a frame.
//
// The CPU twins (`pose_morph`, `morph_texel`, `pack_morph_atlas`, `warp_vertex`, `warp_cut`) are
// what the shader does, for the tests and the CPU pictures.
//
// ge:summary Morph meshes drawn: vertex-animated models, frames blended on the GPU, tinted skins
// ge:keywords burn charred embers etched morph_burn scorch-skin
// ge:keywords render morph morph-mesh vertex-animation frames blend interpolation skin atlas
// ge:keywords tint team-colours player-colours ramp glow fullbright sharp smooth nearest
// ge:keywords hole cut-out alpha see-through mark-hole
// ge:keywords warp morph-warp dissolve materialize dematerialize spawn teleport fade-in fade-out
// ge:keywords warp_vertex warp_field warp_cut pack_warp ash dust crumble burning-edge shimmer pinch
// ge:keywords create_morph prepare_morph encode_morph pose_morph morph_texel pack_morph_atlas
// ge:skill render
// ge:test tests/render_morph_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr uint32_t kTintSteps = 16;       // a ramp's colours
inline constexpr uint32_t kMaxMorphDraws = 256;  // drawn a frame
inline constexpr uint32_t kMorphAtlasSize = 2048;

// A skin texel's mark.
inline constexpr uint8_t kMarkPlain = 0;
inline constexpr uint8_t kMarkGlow = 1;
inline constexpr uint8_t kMarkHole = 2;      // nothing drawn: the skin's alpha cut through
inline constexpr uint8_t kMarkTintA = 0x40;  // | the step, 0..15
inline constexpr uint8_t kMarkTintB = 0x80;  // | the step

struct MorphSkin {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;   // sRGB, four a texel, rows from the top (a's ignored)
  std::vector<uint8_t> marks;  // one a texel (empty: all plain)
};

struct MorphModel {
  std::vector<std::vector<float>> positions;  // a frame each: xyz a vertex, the model's frame
  std::vector<std::vector<float>> normals;    // a frame each: xyz a vertex, unit
  std::vector<float> uv;                      // two a vertex: u right, v down, 0..1
  std::vector<uint32_t> indices;              // three a triangle, fronts by the engine's rule
  MorphSkin skin;
  uint32_t vertex_count() const {
    return positions.empty() ? 0 : static_cast<uint32_t>(positions[0].size() / 3);
  }
};

// Rows of kTintSteps linear colours, rgb a colour.
struct TintRamps {
  std::vector<float> rgb;
  uint32_t rows() const { return static_cast<uint32_t>(rgb.size() / (3 * size_t{kTintSteps})); }
};

// A ramp of `colour` (linear): from near black through the colour to a pale tint of it, step 0
// the lightest (as a painted ramp runs), appended to `ramps`; returns its row.
uint32_t ramp_from(TintRamps& ramps, const float colour[3]);

// One instance's pose at one moment.
struct MorphPose {
  double position[3] = {0, 0, 0};    // world, m
  float rotation[4] = {0, 0, 0, 1};  // x, y, z, w: the model's frame to the world's
  float scale = 1;
  uint32_t frame_a = 0, frame_b = 0;
  float blend = 0;  // 0: frame_a, 1: frame_b
};

// A draw coming into being or going out of it (a spawn, a teleport): `shown` 1 is the plain draw.
struct MorphWarp {
  float shown = 1;  // 0: gone, 1: whole
  // The sweep: the world way (unit) along which the far end comes in first and the near end goes
  // out first, and the span along it (m from the draw's position) the figure fills.
  float axis[3] = {0, 1, 0};
  float low = -1, high = 1;
  float ash[3] = {0.3f, 0.27f, 0.24f};  // the band at the cut: its light greyed, times this
  float edge[3] = {0, 0, 0};            // the band's own light, linear, over the scene's white
  float pinch = 0;    // 0..1: drawn into a column round the upright line, stretched up from `low`
  float shimmer = 0;  // 0..1: bands that climb it (pinch and shimmer read the axis as up)
  float seed = 0;     // the noise's offset: two figures differ
};
inline constexpr float kWarpEdge = 0.08f;  // the band at the cut, a share of the field's 0..1

// A figure's burns, each 0..1 (render/body_burn.hpp's BurnSkin).
struct MorphBurn {
  float charred = 0, embers = 0, etched = 0;
  float seed = 0;  // the patches' offset, 0..1
};

struct MorphDraw {
  uint32_t model = 0;
  MorphPose now, last;        // `last`: where it was the frame before (a cut: the same as now)
  uint32_t tint[2] = {0, 0};  // ramp rows for the A and B regions
  float outdoor = 1;          // the share of the frame's sun and sky
  float ambient[3] = {0, 0, 0};
  float shade[3] = {0, 0, 0};      // light from `shade_dir`, the same units as ambient
  float shade_dir[3] = {0, 1, 0};  // toward the light, world, unit
  float glow = 1;                  // a glowing texel's light over the white of the scene
  bool smooth = true;              // bilinear (modern) or sharp (classic)
  // `ambient` and `shade` in the scene's WHITE (1: lights a white face as white as the white the
  // eye is adapted to, as a baked lightmap's 1 does) rather than in the sun's units: what a game
  // whose light is a map's lightmaps samples under a model.
  bool light_in_white = false;
  // How much of it shows: 1 solid; under 1 a share of its pixels left out by an ordered pattern
  // that moves every frame, so the temporal resolve reads it as see-through (no sorting, and it
  // keeps its depth and motion); 0 or less: not drawn.
  float alpha = 1;
  MorphWarp warp;
  MorphBurn burn;
};

// Where each model's skin sits in the atlas, texels.
struct AtlasRect {
  uint32_t x = 0, y = 0, w = 0, h = 0;
};
// Shelf-pack the skins (tallest first) into a kMorphAtlasSize square with a texel of gap. False
// when they do not fit.
bool pack_morph_atlas(std::span<const MorphModel> models, std::vector<AtlasRect>& rects);

// The CPU twins. Frames `a` and `b` of `model` blended by `t`: positions and renormalised normals.
void pose_morph(const MorphModel& model, uint32_t a, uint32_t b, float t,
                std::vector<float>& positions, std::vector<float>& normals);
// Texel (x, y) of `skin` as drawn: linear rgb, and 1 in `glow` when it lights itself.
void morph_texel(const MorphSkin& skin, const TintRamps& ramps, const uint32_t tint[2], uint32_t x,
                 uint32_t y, float rgb[3], float* glow);

// The warp's move of one point: `v` the point's offset from the draw's position (world axes, m),
// at `time_s` (the frame's clock: the shimmer's phase), into `out`. `shown` 1 leaves it.
void warp_vertex(const MorphWarp& w, float time_s, const float v[3], float out[3]);
// The share at which a point appears: `local` its place in the model's own frame times the scale
// (m), `along` its offset from the draw's position along `w.axis` (m). Drawn while under
// `shown * (1 + kWarpEdge)`.
float warp_field(const MorphWarp& w, const float local[3], float along);
// Whether that point is drawn under `w`, and its burn 0..1 in `edge` (1 at the cut).
bool warp_cut(const MorphWarp& w, const float local[3], float along, float* edge);
// The warp as the shaders read it (render/wgsl/warp.wgsl's Warp): four vec4.
void pack_warp(const MorphWarp& w, float out[16]);

struct Morph {
  wgpu::Buffer models, vertices, uvs, indices, draws, ramps;
  wgpu::Texture atlas;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_draw = 0;
  std::vector<uint32_t> index_counts;  // a model each
  std::vector<uint8_t> staged;         // kMaxMorphDraws GpuDraws
  std::vector<uint32_t> staged_models;
  uint32_t drawn = 0;
  uint32_t dropped = 0;  // past kMaxMorphDraws this frame
};

// Upload `models` and `ramps` (at least one row) once. False when the skins do not fit the atlas.
bool create_morph(gpu::Device& device, gpu::PipelineSet& pipelines, Morph& morph,
                  const wgpu::BindGroupLayout& frame, std::span<const MorphModel> models,
                  const TintRamps& ramps);
// This frame's draws, relative to `view`'s eye.
void prepare_morph(gpu::Device& device, Morph& morph, std::span<const MorphDraw> draws,
                   const FrameView& view);
// The draws over the targets (after the opaque world, before the sky).
void encode_morph(const Morph& morph, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                  const wgpu::BindGroup& frame);

}  // namespace ge::render
