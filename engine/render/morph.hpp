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
//   eye, a lamp) or TINTED (`kMarkTintA` / `kMarkTintB` and a STEP 0..15 down a ramp). A tinted
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
//   is dark. Glowing texels add their own colour times `glow`, whatever the light.
// - THE PASS draws after the opaque world (depth tested and written, reversed z, back faces
//   culled), one draw a model instance, at most `kMaxMorphDraws` a frame.
//
// The CPU twins (`pose_morph`, `morph_texel`, `pack_morph_atlas`) are what the shader does, for
// the tests and the CPU pictures.
//
// ge:summary Morph meshes drawn: vertex-animated models, frames blended on the GPU, tinted skins
// ge:keywords render morph morph-mesh vertex-animation frames blend interpolation skin atlas
// ge:keywords tint team-colours player-colours ramp glow fullbright sharp smooth nearest
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
