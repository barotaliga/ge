// THE FOG IN AN AREA (docs/rendering.md § Fog in an area): volumes of fog, mist, dust or smoke a
// game places in its world (a valley's morning mist, a wood's ground fog, the dust of a hall, the
// glowing haze over a pool of lava), lit by the sun past its shadows, by the sky and by their own
// glow, gathered into one froxel volume a frame that every material reads through frame.wgsl's
// `haze`, so no material has its own fog and a surface, a particle, the water, the sky and the
// cloud behind a bank are all veiled by the same air.
//
// - THE VOLUMES (`FogVolume`): a box or an ellipsoid turned about the vertical, its extinction at
//   the floor thinning upward by `falloff`, softened over `edge` m inside its faces, carved by the
//   cloud's tiled noise (frame.wgsl's `cloud_noise`) drifting on the wind, scattering a share
//   `albedo` of what it takes out with a Henyey-Greenstein lobe `phase`, and glowing `glow` cd/m2 a
//   metre of extinction, its share `sky` of the open sky's light (a closed room's dust takes none
//   of it, M38g). `prepare_fog` keeps the kMaxFogVolumes nearest that reach into the froxel
//   volume and hands them to the GPU camera-relative.
// - THE FROXELS: `FogSettings::width` x `height` columns over the screen, `slices` deep out to
//   `range` m, a slice's far edge at range x (slice / slices)^2, so the slices near the eye are
//   centimetres deep and the last ones metres.
// - "fog.inject" (one thread a froxel): the medium at a point jittered inside the froxel (a new
//   depth each frame on the golden ratio), lit by the key light (frame.wgsl's `sun_light`: the
//   air's transmittance and the cloud's shadow) past the mountains (a march toward the sun over
//   the ground's heights, to about 11 km, with the horizon maps' answer near the snow) and
//   the casters' maps (render/shadows.hpp), so a wood throws SHAFTS through its mist, and past
//   the fog's own medium toward the light (a short march: the floor of a deep bank lies in its
//   top's shade), with the further orders of scattering held to a deep bank's lit top
//   (`key_scatter`: a sunlit bank as bright as the snow, a thin dust scatters once), by the
//   sky's light, by the ground's light from below (the sun and sky sent back by the terrain's
//   albedo under the point) and by its glow; then blended with the last frame's light at the
//   same world point (reprojected), `history` of it, so the jitter averages into a smooth medium
//   with no bands.
// - "fog.integrate" (one thread a column): front to back, each slice's light over its depth
//   (energy-conserving: the light a slice scatters is dimmed by the slice itself), its
//   transmittance multiplied on, both written at the slice's far edge into `volume`: the light
//   scattered toward the eye and the share of what is behind that is seen.
// - THE READ (frame.wgsl's `local_fog`, called by `haze`, the sky and the cloud): a pixel at `d` m
//   takes the volume at sqrt(d / range): colour x transmittance + in-scatter. With no volume in
//   reach `fog_uniforms` writes a range of 0 and nothing reads the volume, so a game without fog
//   draws the same bits it drew before.
//
// - ITS GLOW AS A LAMP (M38d): a glowing volume lights what is round it, the snow under a fire's
//   haze, a cave's walls round a pool's glow. `fog_glow_light` hands it to the frame's lamps
//   (render/lights.hpp) as one point light at its heart: an optically thin volume's intensity is
//   its glow times its extinction times its volume (cd), the share that escapes a thick one
//   ((1 - e^-tau) / tau over its smallest half-size) kept, softened over its own size, flickering
//   by its `flicker` as its glow in the froxels does (`fog_glow_level`, read by `prepare_fog`).
// - THE LAMPS (M38e): "fog.inject" also scatters every lamp of the cluster a froxel falls in
//   (render/lights.hpp's list in group 0, wgsl/lights.wgsl's `light_inscatter`) on the volume's
//   lobe, past each lamp's own shadow map where it has one: a lamp in a mist has a halo, a
//   floodlight's beam and a post's shadow show in the air. A frame with no lamps adds nothing.
//
// ge:summary Fog, mist, dust and glowing haze in areas: volumes into a lit froxel volume
// ge:summary every material's haze reads, with shafts through the sun's shadows
// ge:keywords render fog local-fog volumetric-fog froxel mist ground-fog dust smoke haze glow
// ge:keywords light-shafts god-rays inject integrate henyey-greenstein temporal reprojection
// ge:keywords fog-volume create_fog prepare_fog encode_fog fog_uniforms read_fog_column
// ge:keywords lamps-in-fog halo lamp-halo beam light_inscatter fog_glow_light fog_glow_level
// ge:keywords glow-lamp glowing-haze flicker
// ge:skill render
// ge:test tests/render_fog_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/lights.hpp"
#include "render/view.hpp"

namespace ge::render {

struct Terrain;

inline constexpr uint32_t kMaxFogVolumes = 32;

struct FogVolume {
  enum class Shape : uint8_t { box, ellipsoid };
  double centre[3] = {0, 0, 0};  // world m
  double half[3] = {10, 2, 10};  // half its size along its own x, y, z, m
  double yaw = 0;                // its turn about the vertical, a world heading (rad)
  double density = 0.05;         // extinction at its floor, 1/m (a fog's visual range is 3 / it)
  double falloff = 0;            // 1/m: the density times exp(-falloff x height over its floor)
  double edge = 2;               // m: the soft margin inside each face
  double albedo[3] = {0.95, 0.95, 0.95};  // the share of the light taken out that is scattered
  double phase = 0.4;          // Henyey-Greenstein asymmetry: 0 even, toward 1 a glow round the sun
  double glow[3] = {0, 0, 0};  // its own light, cd/m2 a metre of extinction (lava's haze, a flare)
  double noise = 0.6;          // the share of the density the noise carves away, 0..1
  double noise_size = 24;      // m: the noise's tile
  double wind[3] = {0, 0, 0};  // m/s: the noise drifts on it
  // The glow's flicker, as a flame's (render/lights.hpp's Flicker::flame): its swing, 0 steady ..
  // 1, its rate and its own noise. The volume's glow and its lamp (`fog_glow_light`) waver as one.
  float flicker = 0;
  float rate_hz = 9;
  uint32_t seed = 0;
  // The share of the open sky's light that reaches it, 0..1: 1 under the open sky, 0 in a closed
  // room (a hall's dust lit only by the sun through its windows, past the casters' maps, and by
  // its lamps). It scales the sky's light and the ground's light from below.
  float sky = 1;
  // It stands on the ground (the frame's terrain): its height is measured up from the ground
  // under each point, from 0 to 2 x half[1], so a mist lies as deep over a slope as over a flat;
  // `centre[1]` is then unused. Without a terrain it stands on y = 0.
  bool hug_ground = false;
  Shape shape = Shape::box;
};

struct FogSettings {
  uint32_t width = 160, height = 90, slices = 64;  // the froxels
  double range = 160;                              // m: the last slice's far edge
  double history = 0.9;                            // the last frame's share of a froxel's light
};

struct Fog {
  FogSettings settings;
  wgpu::Texture media[2], volume;
  wgpu::TextureView media_view[2], volume_view;
  wgpu::Buffer params, volumes;
  wgpu::BindGroupLayout inject_layout, integrate_layout, ground_layout, shadow_layout;
  wgpu::BindGroup inject_group[2], integrate_group[2], ground_group, shadow_group;
  uint32_t p_inject = 0, p_integrate = 0;
  // What prepare decided.
  uint32_t count = 0;     // volumes handed to the GPU this frame
  int current = 0;        // the media this frame writes ([1 - current] is the history)
  bool had_last = false;  // the last frame injected (its light is a history to blend)
  bool ground = false;    // a terrain lends its heights and horizon
  std::vector<float> staged;
};

// The froxel textures, the pipelines (into `pipelines`, before the load ends) and the groups.
// `frame` is group 0's layout; `terrain` lends its horizon maps (null: no ground's shadow). Until
// bind_fog_shadows, nothing casts.
void create_fog(gpu::Device& device, gpu::PipelineSet& pipelines, Fog& fog,
                const wgpu::BindGroupLayout& frame, const Terrain* terrain,
                const FogSettings& settings = {});

// The casters' maps the fog's light is shadowed by (render/shadows.hpp's `SunShadows::group`).
void bind_fog_shadows(Fog& fog, const wgpu::BindGroup& shadows);

// Which of `volumes` reach into the froxel volume round `view`'s eye, nearest first, at most
// kMaxFogVolumes (pure; prepare_fog's choice).
std::vector<size_t> fog_in_reach(const FogSettings& settings, const FrameView& view,
                                 std::span<const FogVolume> volumes);

// The frame's upload: the volumes in reach, camera-relative, and the noise's drift at
// `view.time_s`. `cut` forgets the history (a camera cut).
void prepare_fog(gpu::Device& device, Fog& fog, const FrameView& view,
                 std::span<const FogVolume> volumes, bool cut = false);

// The frame block's fog row: the range and the slices while a volume is in reach, else zero (no
// pixel reads the volume). After fill_uniforms, before the block is written.
void fog_uniforms(const Fog& fog, FrameUniforms& u);

// "fog.inject" then "fog.integrate". Both open (empty) when no volume is in reach, so the pass
// table keeps its rows. `frame` is group 0.
void encode_fog(Fog& fog, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame);

// Labs and tests (blocks): the integrated volume's column through froxel (x, y), front to back:
// four floats a slice (the in-scatter rgb, the transmittance).
std::vector<float> read_fog_column(gpu::Device& device, const Fog& fog, uint32_t x, uint32_t y);

// The lamp a glowing volume lights its surroundings with (render/lights.hpp), at its heart:
// `floor_y` is the ground under it for a volume that hugs the ground (its height measured up
// from there), unused for one that does not. A candela of 0 for a volume that does not glow.
LocalLight fog_glow_light(const FogVolume& volume, double floor_y = 0);
// The glow's level at `t_s` (1 for a steady glow): its lamp's `flicker_level`.
float fog_glow_level(const FogVolume& volume, double t_s);

// A slice's far edge, m: range x (slice / slices)^2.
double fog_slice_far(const FogSettings& settings, uint32_t slice);

}  // namespace ge::render
