// THE SUN'S SHADOWS of what stands on the ground (docs/rendering.md § Lighting and shadows): the
// ground's own shadow is the horizon map's (render/horizon.hpp); what stands on it casts into
// two kinds of depth map, one layer each of one depth array, every map an orthographic view
// along the sun (depth 1 toward the sun, 0 away, cleared to 0 where nothing casts).
//
// - TWO CACHED CASCADES of what stands still (the `still` families: the forest), round the
//   camera: the near one `near_radius` m to a side, the far one `far_radius`.
//   A cascade is REDRAWN only when it is due: never drawn, the eye has drifted more than `drift`
//   of its radius from its centre (across the sun, in its plane), or the sun has turned more
//   than its `turn` since it was drawn. A cascade that is not redrawn keeps its centre and its
//   matrix, so its shadows stay exactly where they were; `redraws` counts each one's.
// - THE FIGURES' ATLAS (the third layer): one tile a figure (`figures` centres a frame, up to
//   kMaxShadowFigures), each a view `figure_radius` m to a side round its figure, redrawn every
//   frame from the `moving` families (the figures, the crowd). A tile's view runs along the
//   sun, so it holds the figure's whole shadow however long it falls. Each tile culls the moving
//   casters by its own box into a caster slot of its own (slot k for tile k), so what is drawn
//   into the atlas grows with the figures near a tile, not with the moving families' counts.
//
// A map's centre is snapped to its texels across the sun, so a redrawn cascade's texels land on
// the old ones'. The receivers (the terrain, the instances) read `ShadowParams` and the array
// through `group` (`sun_shadow` in wgsl/sun_shadow.wgsl): the near cascade where it reaches,
// else the far one, and the least of that and every tile the point falls in.
//
// The passes, in a frame's order: `prepare_sun_shadows` (decide, stage; after `prepare_family`),
// then `encode_sun_shadows` ("shadows.cull": the casters of every map drawn this frame, then
// "shadows.near", "shadows.far" and "shadows.figures", each a depth pass of its layer). Before
// the draws that read them.
//
// ge:summary The sun's shadows: two cached cascades of what stands, a figures' atlas, one depth
// array
// ge:keywords render shadows sun-shadows cascades cached-cascades shadow-atlas figures casters
// ge:keywords per-tile-cull crowd-shadows
// ge:keywords depth-array comparison-sampler pcf orthographic texel-snap redraw-threshold
// ge:keywords create_sun_shadows prepare_sun_shadows encode_sun_shadows shadow-params
// ge:keywords ortho_shadow_params ortho-shadow
// ge:skill render
// ge:test tests/render_shadows_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/instances.hpp"
#include "render/sky.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr int kCascades = 2;
inline constexpr int kMaxShadowFigures = 16;  // the atlas's tiles (4 x 4)
inline constexpr int kShadowLayers = kCascades + 1;
inline constexpr int kShadowViews = kCascades + kMaxShadowFigures;  // a frame block each

struct ShadowSettings {
  uint32_t size = 2048;         // a map's texels a side (every layer)
  double near_radius = 48;      // m, half the near cascade's side
  double far_radius = 400;      // m, half the far cascade's side
  double depth = 2000;          // m, half a map's depth along the sun
  double drift = 0.25;          // the share of its radius the eye may drift from a cascade's centre
  double near_turn_deg = 0.25;  // how far the sun may turn before the near cascade is redrawn
  double far_turn_deg = 1.0;    // and the far one
  double figure_radius = 1.4;   // m, half a figure's tile's side
  double figure_depth = 300;    // m, half a tile's depth along the sun
  // The share of a tile's texels a metre its casters pick their LOD by: a shadow needs the
  // figure's silhouette, not its folds, so a figure filling a tile is drawn at a coarser level.
  double figure_lod_share = 0.25;
  double bias_m = 0.05;  // the receiver's depth bias, m along the sun
};

// wgsl/sun_shadow.wgsl's `SunShadow`, field for field.
struct ShadowParams {
  float cascade[kCascades][16];
  float offset[kCascades][4];
  float atlas[16];
  float tile[kMaxShadowFigures][4];  // the eye minus figure k's centre in the atlas's clip units
  float info[4];   // texels a side, tiles across, a cascade's depth bias (clip z), figures
  float texel[4];  // a texel's size (m): cascade 0, cascade 1, the atlas; a tile's depth bias
  float reach[4];  // where some tile reaches, atlas clip x, y about the eye: min x, y, max x, y
};

// Where a cascade was last drawn from: CPU state, decided by `update_cascade`.
struct ShadowCascade {
  bool drawn = false;
  double centre[3] = {0, 0, 0};
  double sun[3] = {0, 1, 0};
  double basis[3][3] = {};  // across (x), up (y), toward the sun (z)
};

struct SunShadows {
  ShadowSettings settings;
  wgpu::Texture maps;
  wgpu::TextureView array_view;
  std::array<wgpu::TextureView, kShadowLayers> layer_views;
  wgpu::Sampler compare;
  wgpu::Buffer params;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;  // what the receivers bind
  // A frame block a view (the cascades, then the atlas's tiles): the casters' cull and draws read
  // it as group 0.
  std::array<wgpu::Buffer, kShadowViews> view_uniforms;
  std::array<wgpu::BindGroup, kShadowViews> view_groups;

  std::array<ShadowCascade, kCascades> cascades{};
  std::array<bool, kCascades> due{};
  std::array<uint64_t, kCascades> redraws{};  // since creation
  int figures = 0;                            // this frame's tiles
  ShadowParams staged{};
};

// The maps (settings.size a side, three layers), the receivers' group and a frame block a view.
// `frame` and `sky` make the views' groups (the sky's tables are unused by the casters).
void create_sun_shadows(gpu::Device& device, SunShadows& shadows, const ShadowSettings& settings,
                        const wgpu::BindGroupLayout& frame, const Sky& sky);

// The receivers' group with nothing drawn: what a terrain or a family binds until a frame's
// shadows are bound (render/terrain.hpp, render/instances.hpp). Its one 1 x 1 map casts nothing.
wgpu::BindGroup no_sun_shadows(gpu::Device& device);
wgpu::BindGroupLayout sun_shadow_layout(gpu::Device& device);

// Decide which cascades are due for `view`, place the figures' tiles round `figures` (world
// centres, at most kMaxShadowFigures) and stage every block, with the casters' cull of the maps
// drawn this frame (the `still` families' slots 0 and 1, `create_family_casters(.., kCascades)`;
// the `moving` families' slot k for tile k, `create_family_casters(.., kMaxShadowFigures)`).
void prepare_sun_shadows(gpu::Device& device, SunShadows& shadows, const FrameView& view,
                         std::span<const std::array<double, 3>> figures,
                         std::span<InstanceFamily* const> still,
                         std::span<InstanceFamily* const> moving);
// Forget every cascade: the next frame redraws both (a new map, a jump of the camera).
void reset_sun_shadows(SunShadows& shadows);

// "shadows.cull", then "shadows.near", "shadows.far" and "shadows.figures": the due cascades'
// casters (`still`) and the figures' (`moving`, each tile's own), culled and drawn into their
// layers.
void encode_sun_shadows(SunShadows& shadows, std::span<InstanceFamily* const> still,
                        std::span<InstanceFamily* const> moving, const InstancePipelines& ip,
                        const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                        wgpu::CommandEncoder& encoder);

// The orthographic view along `sun` (unit, toward the sun) of a square `radius` m to a side and
// `depth` m along the sun round a centre: `basis` its axes (across, up, toward the sun) and `m`
// the matrix of a point relative to the centre to clip. The CPU twin of what a map stores.
void shadow_view(const double sun[3], double radius, double depth, double basis[3][3], float m[16]);

// Cascade `c`'s decision for an eye and a sun (unit, toward the sun): true when it is due (never
// drawn, the eye drifted past `settings.drift` of its radius across the sun, or the sun turned past
// its threshold), and then `k` is placed again (its centre snapped to its texels across the sun)
// and `m` is its matrix. Under the horizon nothing is due and `k` is forgotten. Pure CPU: what
// `prepare_sun_shadows` runs, and what a lab counts redraws by without a device.
bool update_cascade(const ShadowSettings& settings, int c, const double sun[3], const double eye[3],
                    ShadowCascade& k, float m[16]);

// AN ORTHOGRAPHIC MAP A GAME DRAWS ITSELF, read as the sun's casters (a BSP map's sun map, drawn
// by its own renderer, for the fog's light): centred on `o`, `x` and `y` across and up its face,
// looking along `z`, `half` m to each side and `depth` m deep each way, a texel storing
// (z . (p - o) + depth) / (2 depth) of what it sees. The block reads it as cascade 0 (layer 0 of
// the maps bound beside it) with the sampler the game binds beside it: less-or-equal for a map
// looking along the sun's light (the nearest caster's depth the least), lit where a point is no
// deeper; `bias` is added to the point's depth (negative for such a map). No figures.
struct OrthoShadow {
  double o[3] = {0, 0, 0};
  double x[3] = {1, 0, 0}, y[3] = {0, 1, 0}, z[3] = {0, 0, 1};
  double half = 1, depth = 1;
  uint32_t size = 1;  // texels a side
  double bias = -0.0015;
};
// The block for `map` seen from `eye` (world m); pure.
ShadowParams ortho_shadow_params(const OrthoShadow& map, const double eye[3]);

}  // namespace ge::render
