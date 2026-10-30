// THE LAMPS' SHADOWS (docs/rendering.md § Local lights): one depth atlas of square tiles
// (kLampTilesAcross a side), dealt each frame to the local lights that matter most, into which the
// instance families' casters are drawn from each lamp's own place.
//
// - A TILE is a perspective view from a lamp: a spot's one, round its axis, wide enough for its
//   outer cone; a point light's six, a cube's faces (+x, -x, +y, -y, +z, -z, in the tiles in that
//   order), each a little wider than 90 degrees (`face_margin`) so the filter's taps near a face's
//   edge still read it. Depth is reversed (near / the distance along the view, 0 where nothing
//   casts), so the casters' pipeline and the comparison are the sun's (render/shadows.hpp).
// - THE DEAL (`deal_lamp_tiles`, CPU, deterministic): of the lights a frame keeps
//   (render/lights.hpp's bins) those with a `shadow` key, ranked by the light they put on the eye
//   (their candela at their level over the distance squared plus 1 m^2: near, bright and in view;
//   a lamp outside the view is not in the bins at all); a lamp that already holds tiles counts
//   `keep` times its score, so two lamps of a like score do not trade tiles from frame to frame.
//   Each takes its run of tiles while they last (a held lamp keeps its own run); the rest light
//   unshadowed. A light's GPU row's shadow slot is its first tile, -1 without.
// - THE CACHE: a tile is REDRAWN only when it is due: newly dealt, its lamp moved or turned, or a
//   MOVER (a sphere the caller names: a figure, a door, a thrown crate) touches the tile's view.
//   A tile not due keeps its depth, so a still lamp that sees nothing move costs nothing a frame;
//   `redraws` counts the tiles drawn since creation. A muzzle flash is a lamp like any other: its
//   key is the shot's, so its tiles are drawn the frame it fires and kept for its few frames.
// - THE CASTERS: every family handed in with caster slots `first_slot` .. `first_slot` +
//   kMaxLampTiles - 1 (`create_family_casters`): tile k culls into slot `first_slot` + k by its
//   own view's planes and the lamp's range. A family without them casts no lamp's shadow (a
//   lamp's own glowing bulb or flame, which would shade everything round it).
// - THE RECEIVERS read the atlas through group 0 (binding 13, `frame_group`'s `lamp_atlas`) and
//   the tiles off the light buffer: wgsl/lights.wgsl's `lamp_shadow`, inside `local_lights`.
//
// The passes, in a frame's order: `prepare_lights` then `prepare_lamp_shadows` (deal, stage),
// then `encode_lamp_shadows` ("lamps.cull", then "lamps.shadows": each due tile cleared and its
// casters drawn), before the draws that read them.
//
// ge:summary The lamps' shadows: a depth atlas of spot tiles and cube faces, dealt and cached
// ge:keywords render lamp-shadows local-light-shadows shadow-atlas cube-faces point-shadow
// ge:keywords spot-shadow tiles deal cache movers redraw reversed-z casters muzzle-flash
// ge:keywords deal_lamp_tiles prepare_lamp_shadows encode_lamp_shadows lamp_tile_view
// ge:skill render
// ge:test tests/render_lamp_shadows_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/instances.hpp"
#include "render/lights.hpp"
#include "render/sky.hpp"
#include "render/view.hpp"

namespace ge::render {

struct LampShadowSettings {
  uint32_t size = 2048;       // the atlas's texels a side (a tile is size / kLampTilesAcross)
  int first_slot = 0;         // the families' caster slot of tile 0
  double near_m = 0.05;       // the tiles' near plane, m
  double face_margin = 1.06;  // a cube face's half-width over 90 degrees' (tan 45 = 1)
  double bias = 0.004;        // the receiver drawn nearer the lamp by this share of its distance
  double keep = 1.5;          // a held lamp's score weighed up by this
  double max_spot_deg = 80;   // a spot's tile is no wider than this off its axis
};

// A tile's lamp and where it drew from: CPU state, decided by `deal_lamp_tiles`.
struct LampTileState {
  uint32_t key = 0;  // the lamp holding it (0: free)
  int face = 0;      // a point light's face, 0..5 (a spot's 0)
  bool drawn = false;
  double at[3] = {0, 0, 0};
  float dir[3] = {0, 0, 0};
  float outer_deg = 0, range_m = 0;
};

struct LampShadows {
  LampShadowSettings settings;
  wgpu::Texture atlas;
  wgpu::TextureView atlas_view;  // what group 0 binds at 13
  uint32_t p_clear = 0;          // a tile's depth back to 0 before its casters are drawn
  std::array<wgpu::Buffer, kMaxLampTiles> view_uniforms;
  std::array<wgpu::BindGroup, kMaxLampTiles> view_groups;

  std::array<LampTileState, kMaxLampTiles> tiles{};
  std::array<bool, kMaxLampTiles> due{};
  std::array<float, size_t{kMaxLampTiles} * 16> views{};  // each tile's matrix, for its frame block
  uint32_t lamps = 0;                                     // lamps dealt tiles this frame
  uint32_t used = 0;                                      // tiles dealt this frame
  uint32_t drawn = 0;                                     // of them, redrawn this frame
  uint32_t unshadowed = 0;  // lamps with a key that got no tiles this frame
  uint64_t redraws = 0;     // tiles drawn since creation
};

// The atlas, its clear pipeline (added to `pipelines` before its `finish`) and a frame block a
// tile; `frame` and `sky` make the tiles' groups (with the sky's empty atlas: a tile never reads
// the atlas it is drawn into).
void create_lamp_shadows(gpu::Device& device, gpu::PipelineSet& pipelines, LampShadows& shadows,
                         const LampShadowSettings& settings, const wgpu::BindGroupLayout& frame,
                         const Sky& sky);
// Forget every tile: the next frame redraws what it deals (a new map, a jump of the camera).
void reset_lamp_shadows(LampShadows& shadows);

// Tile `face`'s view of `light` (a spot's face is 0): `m` takes a point relative to the lamp to
// the tile's clip (x, y in [-1, 1] across the tile, depth near / the distance along the view), and
// `planes` (inside >= 0, unit, relative to the lamp) bound it: four sides and the near plane.
// Pure CPU; the twin of what `lamp_shadow` in wgsl/lights.wgsl reads.
void lamp_tile_view(const LampShadowSettings& settings, const LocalLight& light, int face,
                    float m[16], float planes[5][4]);

// THE DEAL for this frame's `bins` (binned from `lights`, `view`): which lamps take which tiles,
// which tiles are due (`movers`: world centre and radius of each thing that moves), each kept
// light's shadow slot, and the tiles' rows (`bins.tiles`). Pure CPU: what `prepare_lamp_shadows`
// runs, and what a test deals without a device.
void deal_lamp_tiles(LampShadows& shadows, LightBins& bins, std::span<const LocalLight> lights,
                     const FrameView& view, std::span<const std::array<double, 4>> movers);

// Deal, then stage: the lights' rows and the tiles' (over what `prepare_lights` wrote), each due
// tile's frame block and its casters' cull in every family of `casters`.
void prepare_lamp_shadows(gpu::Device& device, LampShadows& shadows, Lights& lights,
                          std::span<const LocalLight> list, const FrameView& view,
                          std::span<const std::array<double, 4>> movers,
                          std::span<InstanceFamily* const> casters);

// "lamps.cull" and "lamps.shadows": every due tile's casters culled, then each due tile cleared
// and its casters drawn into it.
void encode_lamp_shadows(LampShadows& shadows, std::span<InstanceFamily* const> casters,
                         const InstancePipelines& ip, const gpu::PipelineSet& pipelines,
                         FrameRecorder& rec, wgpu::CommandEncoder& encoder);

}  // namespace ge::render
