// THE BSP RENDERER (r_bspmode): a classic map (draw_data.hpp's `Scene`) drawn into a frame's
// targets by one of two paths, picked per frame:
//
// - CLASSIC: the map as it was made to look: its textures times its lightmaps, its liquids as
//   their own warped textures and its sky faces as the sky texture's two scrolling layers.
// - V2: the same surfaces under the engine's world: the sky faces show our sky and clouds (they
//   write a mask, and a fix-up pass gives their pixels the cleared depth the sky pass draws
//   over); the liquids are bodies of our water, lava and slime (bsp_liquids.hpp), not drawn here;
//   and the light is the lightmaps' or REAL-TIME: the map's light entities as point lights (each
//   one's flicker, the nearest `shadowed` with a cube of depth), the lava's glow as lights over
//   it, the sun through the sky faces with its own shadow map, the sky's light by how open a
//   point is overhead, and the glowing texels. The shadow maps are KEPT: the world they see
//   stands still, so a light's cube is drawn when it takes a slot (and only then) and an
//   orthographic map when its camera moves; every `prepare_bsp` is followed by its `encode_bsp`,
//   which draws what the prepare found stale. A frame draws at most `Options::fresh_cubes` new
//   cubes (a light past the budget waits a frame or more unshadowed), the slots are reused least
//   recently first, and a dynamic light (it moves or goes out) casts no shadow.
//
// - THE WEATHER (v2, `Options::weather`): a third orthographic map looks along the fall, so the
//   faces the rain and the snow reach (a courtyard's flags, a sill, the floor inside a window as
//   far as the wind drives it, a wall facing the wind) are wet, pooled with puddles that ripple
//   while it rains, or snowed on, and the rooms behind stay dry; a lightning flash lights what
//   is open to the sky. The same map is the fall's shelter (`bsp_shelter`).
//
// The passes, in a frame's order: "bsp.shadows" (v2 with lights or weather: the stale maps, timed
// as one), then with r_cull
// "bsp.cull", "bsp", "hiz", "bsp.cull.late" and "bsp.late", else "bsp"; then "bsp.sky" (v2);
// then the caller's sky, clouds, water and liquids over the frame's targets.
//
// ge:summary The BSP renderer: classic or v2 (our sky, our liquids, real-time lights and shadows)
// ge:keywords bsp brush render renderer r_bspmode classic v2 lightmap point-light cube-shadow
// ge:keywords sun-shadow sky-mask flicker lava-light weather rain snow puddles shelter bsp_shelter
// ge:keywords set_movers movers moving-brush-models doors plats
// ge:keywords shadow-cache shadow-slot fresh_slots fresh_cubes shadow-budget shadow-hitch
// ge:skill brush render
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brush/app/bsp_cull.hpp"
#include "brush/app/draw_data.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/precip.hpp"

namespace ge::brush::app {

enum class Mode : uint8_t { classic, v2 };
enum class Lighting : uint8_t { lightmaps, lights, rebaked };
// "classic" or "v2"; false for any other word.
bool parse_mode(std::string_view word, Mode* out);

struct Options {
  Mode mode = Mode::v2;  // the renderer for BSP maps; classic stays selectable (r_bspmode)
  Lighting lighting = Lighting::lights;
  double ambient = 0.015;       // the light every face gets, the lightmaps' units
  double brightness = 2.0;      // a lightmap's 1 against a white face under the open day
  double strength = 1;          // the point lights' strength against the classic's
  double dynamic_strength = 1;  // the dynamic lights' (set_dynamic_lights) against the classic's
  int shadowed = 32;            // the nearest lights that cast shadows
  // The cubes drawn a frame at most for lights new to the shadowed few (each one the map drawn six
  // times); the rest wait their turn unshadowed. 0 draws none new.
  int fresh_cubes = 2;
  int most = 160;             // the lights a frame draws at most (the nearest)
  bool lava_lights = true;    // the lava's glow lights the walls round it
  double lava_glow = 1;       // that glow's strength, 0 none
  double slime_glow = 1;      // the slime's faint green glow on the walls round it, 0 none
  bool portal_lights = true;  // each teleporter's panel lights its room with a slow pulse
  // The sky faces as the pack's own scrolling sky under v2 too (ce_skies off): they write their
  // depth and no mask, so our sky and clouds do not fill them; the rooms keep our sky's light.
  bool pack_sky = false;
  // r_texfilter: the map's textures filtered (between texels, between mip levels and along a
  // surface seen at a slant) or CRISP, each texel a square as the classic look draws them.
  bool crisp = false;
  // r_minlight: the light no face falls below, the lightmaps' units, as a soft floor
  // (sqrt(e^2 + m^2)): a dark room comes up to about `minlight`, a lit one keeps its light.
  double minlight = 0.3;
  // r_bounce: the share of the map's baked light added to the real-time lights as the light
  // bounced round the room (the lightmaps as light probes: the room's own light, not flat grey).
  double bounce = 0.35;
  // r_skylight: the sky through the sky faces against the rooms, and its light in them (the
  // sun's and the open sky's): 1 our sky as it is, under 1 dimmer (a dusk's against torch-lit
  // halls), times `sky_tint` (each channel's share, a tint toward the map's own sky).
  double sky_light = 1;
  double sky_tint[3] = {1, 1, 1};
  // The sky as SEEN through the sky faces against the rooms, over r_skylight's share and with no
  // light in the rooms: over 1 a night sky lifted so its moonlit cloud reads to an eye adapted to
  // the torch-lit halls (bsp_sky.hpp's classic night), as a photograph's window is.
  double sky_seen = 1;
  // r_bump: the relief the rebaked lighting reads off each texture's luminance, m from black to
  // white (0: flat faces). Only `Lighting::rebaked` draws it.
  double bump = 0.012;
  // THE WEATHER (v2; bsp_weather.hpp): on, the rain falling now (0 .. 1: its ripples in the
  // puddles), and the way it falls (unit, down and downwind), which a third orthographic map looks
  // along so only what the fall reaches is wet, pooled or snowed on. How wet, how deep the
  // puddles, how much snow and the lightning's flash are the frame's (render/view.hpp's Weather).
  bool weather = false;
  double raining = 0;
  double spread = 0.3;  // the tangent of the spread of ways round `fall` the drops and flakes come
  double fall[3] = {0, -1, 0};
  // r_cull: the world's faces culled by the eye's potentially visible set, the frustum and the
  // depth pyramid in two phases (bsp_cull.hpp); off draws every face.
  bool cull = true;
  // r_novis's opposite: the cull's first step, the potentially visible set; off, every face goes
  // on to the frustum and the pyramid (a map whose visibility is not conservative).
  bool pvs = true;
  // v2 leaves the liquids' faces to the engine's water and liquid passes; these draw a kind's
  // faces warped as the classic does instead (a game whose pass for that kind is switched off).
  bool warped_water = false, warped_slime = false, warped_lava = false;
};

inline constexpr uint32_t kCubeSize = 256;    // a cube face's texels a side
inline constexpr uint32_t kOrthoSize = 2048;  // the sun's and the zenith's maps
inline constexpr int kMaxShadowed = 48;
inline constexpr uint32_t kOrthos = 3;  // the sun's, the zenith's and the fall's

// A DYNAMIC LIGHT: one a game lights for a moment (a muzzle's flash, a rocket's glow, an
// explosion, a glowing pickup), set each frame (`set_dynamic_lights`). The classic path adds it
// to the lightmaps' light as the classic did, falling off in a straight line to nothing at its
// radius; v2 lit in real time draws it as one more point light.
struct DynamicLight {
  double pos[3] = {0, 0, 0};     // engine metres
  double radius = 5;             // m: where its light ends
  double colour[3] = {1, 1, 1};  // its light at its middle, the lightmaps' units (1: as baked 'm')
};

struct GpuLight {
  float pos[4];
  float colour[4];
  float info[4];
};

struct GpuJob {
  float o[4], x[4], y[4], z[4], info[4];
};

// A brush model as the shaders move it: its run of vertices (first, end), 1 hidden, and its offset.
struct GpuMover {
  uint32_t range[4];
  float offset[4];
};

struct GpuBsp {
  float info[4];
  float info2[4];
  float sky[4];
  float info3[4];    // the floor (minlight), the bounce, the sky's light, filtered (1) or crisp
  float info4[4];    // the relief's depth (bump), m; the pack's sky under v2 (1)
  float veil[4];     // the sky faces' pixels times these (the sky's light, seen and tinted)
  float weather[4];  // on (1), the rain falling now, the fall's spread, 0
  GpuJob ortho[kOrthos];
  float info5[4];              // a lightmap layer's height in the atlas's uv, the layers, 0, 0
  float styles[kLightStyles];  // each light style's level now (draw_data.hpp)
};

struct BspDraw {
  const Scene* scene = nullptr;
  wgpu::Buffer vertices, indices;
  // Index ranges by kind (draw_data.hpp's `Kind`): first, count.
  uint32_t range[kKinds][2] = {};
  wgpu::Texture atlas, light;
  // The scene's rebaked light (RGB9E5) and its direction (RGBA8), or 1 x 1 stand-ins.
  wgpu::Texture rebake_rgb, rebake_dir;
  wgpu::Buffer uniform, lights, jobs;
  wgpu::Texture cubes, orthos;
  std::vector<wgpu::TextureView> cube_layers;  // a view a cube face
  wgpu::TextureView ortho_layers[kOrthos];
  wgpu::TextureView cubes_view, orthos_view;
  wgpu::Sampler light_sampler, compare;
  wgpu::BindGroupLayout layout, fix_layout, shadow_layout;
  wgpu::BindGroup group, fix_group, shadow_group;
  wgpu::Texture mask;
  wgpu::TextureView mask_view;
  uint32_t p_draw = 0, p_fix = 0, p_veil = 0, p_shadow = 0;
  // This frame's (prepare_bsp).
  GpuBsp staged{};
  std::vector<GpuLight> frame_lights;
  std::vector<GpuJob> frame_jobs;  // every slot's six cube faces, then the orthographic maps
  int shadowed = 0;
  // THE SHADOW MAPS KEPT: the world they see does not move, so a light's cube is drawn when the
  // light takes a slot (or moves) and kept while it holds it, and an orthographic map when its
  // camera changes. A slot's key is its light's place and reach as the GPU sees them.
  std::array<std::array<float, 4>, kMaxShadowed> slot_key{};
  std::array<bool, kMaxShadowed> slot_held{};      // a slot's cube holds its key's light
  std::array<uint64_t, kMaxShadowed> slot_used{};  // the shadow_frame a light last read it
  uint64_t shadow_frame = 0;                       // prepare_bsp's count
  int shadows_waiting = 0;       // this frame's casters over the budget, unshadowed (prepare_bsp)
  std::vector<int> fresh_slots;  // this frame's slots to draw (prepare_bsp)
  std::array<GpuJob, kOrthos> ortho_drawn{};  // what each orthographic map holds
  std::array<bool, kOrthos> ortho_held{};
  std::array<bool, kOrthos> ortho_fresh{};  // this frame's to draw
  Options options;
  // The light styles' patterns (draw_data.hpp), empty the light as baked (set_light_style).
  std::array<std::string, kLightStyles> light_styles;
  wgpu::Buffer frames;  // the animated textures' frames (Scene::frames)
  // The brush models' runs of vertices and where each stands now (set_movers), [0] the world's.
  wgpu::Buffer movers;
  std::vector<GpuMover> mover_rows;
  bool movers_dirty = false;
  BspCull cull;
  std::vector<DynamicLight> dynamic;       // this frame's (set_dynamic_lights)
  std::vector<PointLight> dynamic_points;  // the same as v2's point lights
  uint64_t culled_frames = 0;  // frames culled since the targets were made (0: no pyramid yet)
  // The scene's lava lights and teleporters' lights (made once).
  std::vector<PointLight> lava, slime, portals;
  std::vector<PointLight> lava_points, slime_points;  // this frame's, at the options' strengths
};

void create_bsp(gpu::Device& device, gpu::PipelineSet& pipelines, BspDraw& draw, const Scene& scene,
                const wgpu::BindGroupLayout& frame);
// The mask at the targets' size (again when they change).
void create_bsp_targets(gpu::Device& device, BspDraw& draw, const render::FrameTargets& targets);
// This frame's lights (picked round the eye, flickered at the view's time) and shadow cameras.
void prepare_bsp(gpu::Device& device, BspDraw& draw, const render::FrameView& view,
                 const Options& options);
// This frame's dynamic lights (before `prepare_bsp`); the classic draws at most kMaxDynamic.
inline constexpr int kMaxDynamic = 32;
void set_dynamic_lights(BspDraw& draw, std::span<const DynamicLight> lights);
// THE MOVING MODELS: a brush model (a door, a plat, a button; model k of the map, k > 0) drawn
// `offset` engine metres from where the map built it, or not drawn at all when `hidden` (an
// entity the game removed). A model not named keeps its last place; all start at the map's.
struct Mover {
  uint32_t model = 0;
  double offset[3] = {0, 0, 0};
  bool hidden = false;
};
void set_movers(BspDraw& draw, std::span<const Mover> movers);
// Light style `style` (0 .. 63) played from now on as `pattern`: letters 'a' (dark) .. 'z', 'm'
// the light as baked, ten a second (draw_data.hpp's `style_level`); empty for steady.
void set_light_style(BspDraw& draw, int style, std::string_view pattern);
// "bsp.shadows", "bsp" and "bsp.sky": the map into the cleared targets.
void encode_bsp(const BspDraw& draw, const gpu::PipelineSet& pipelines, render::FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const render::FrameTargets& targets,
                const wgpu::BindGroup& frame);
// "bsp.veil", after the sky and the clouds: the sky faces' pixels times the options' sky light
// and tint (nothing when both are 1, or in the classic mode).
void encode_bsp_veil(const BspDraw& draw, const gpu::PipelineSet& pipelines,
                     render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                     const render::FrameTargets& targets);

// THE FALL'S SHELTER for render/precip.hpp: the third orthographic map, so the rain and the snow
// are drawn only where they reach (valid after `prepare_bsp` with the weather on).
render::PrecipShelter bsp_shelter(const BspDraw& draw);

// The lights the scene's lava gives off: a grid over each pool's top, each its share of the
// pool's glow (physical, cd), just over the surface. `core_c` the melt's temperature, `cover` the
// share crusted.
std::vector<PointLight> lava_lights(const Scene& scene, double core_c = 1150, double cover = 0.75);
// The lights the scene's slime gives off: the same grid, a faint green of `luminance` cd/m^2.
std::vector<PointLight> slime_lights(const Scene& scene, double luminance = 3);
// The lights the teleporters give off: one half a metre before each panel, a cool grey-white,
// breathing slowly, its brightness by the panel's area.
std::vector<PointLight> portal_lights(const Scene& scene);

}  // namespace ge::brush::app
