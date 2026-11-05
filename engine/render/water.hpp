// THE WATER'S SURFACE DRAWN (docs/water.md § Drawn, M24g): a body of water from engine/water
// (`water::Body`: its level, its box, the waves laid on it, its optics) drawn over the opaque
// scene, the reflection, the refraction, the colour and the sun's glitter per pixel; and, when
// the eye is under it, the same surface drawn from below.
//
// - THE PASSES: "water.copy" copies the opaque frame's colour and depth (the water reads what
//   is behind it), then "water" draws every body into the HDR and motion targets, testing and
//   writing the depth, and "water.below" (the eye under a body) draws that body's surface seen
//   from under it into the `below` targets the view under water (render/underwater.hpp) lays
//   into its picture. They run after the sky and before TAA-U (docs/rendering.md § The frame).
// - THE GEOMETRY: `kWaterLevels`-deep nested squares of kWaterGrid quads round the eye,
//   snapped (no vertex swims), morphed at their seams, the last level's rim out to the horizon;
//   clamped into the body's box. A lake's shore is where the ground stands over the water, and
//   the SHORE'S MASK (`shore_mask`: its outline filled on a grid of at most `kShoreMask` texels a
//   side over its box, grown one texel all round) is where it may be drawn at all, so a lake whose
//   box takes in lower ground beyond its shore (a valley below its outlet, another lake) draws
//   none there.
// - THE WAVES are the simulation's own components (water/waves.hpp), the same formula, so the
//   drawn crest is the one a float rides; the DETAIL components (`detail_waves`) carry the
//   spectrum on from the simulation's shortest wave to a few centimetres, for the pixels alone,
//   and the slope variance past them (`WaterDetail::tail_mss`) is the glitter's roughness.
// - THE GUSTS (`WaterDraw::gusts`): the wind never blows evenly, so the short waves (the detail
//   band and its tail) are raised and laid by a field of gusts drifting downwind (patches of
//   ripples on calmer water, as the wind's puffs write on a lake), and no two stretches of the
//   surface repeat. THE STIR (`WaterDraw::stir`): even water no wind reaches is never glass (a
//   room's draughts, a drip, the warmth): a faint slope that wanders and never repeats.
// - THE PICTURE'S TIERS, from the camera: a vertex keeps the waves at least four of its level's
//   spacings long, a pixel the waves at least four pixels long; what each drops becomes
//   roughness, so the far water glints right and never shimmers.
//
// - THE MIRROR: the reflected ray marched in screen space through the depth copy, the sky's own
//   model where the screen holds nothing; or a game's PLANAR MIRROR (`WaterViews`; a classic map's
//   brush/app/bsp_views.hpp), read where each ray's mirror image lands, the sun glittering only
//   where it shows the sky. THE DAY ON THE WATER (`WaterDraw::daylight`): the sun's and the sky's
//   share on the water itself; a room's own light (`room`) over the rest.
// - FROM BELOW ("water.below"): the same surface and waves, each pixel refracted out by Snell's law
//   (the window, 48.6 degrees round straight up) and read along the bent ray (the game's view up,
//   else marched, else the sky), total internal reflection past it mirroring the water below, the
//   two weighed by the exact Fresnel terms for light leaving water.
// - THE WAKES: the rings a hull, a stroke or a splash set off (water/wake.hpp), summed by the
//   vertex and the pixel in the simulation's formula, each faded as the picture's tiers fade a
//   wave (`set_water_wakes`).
//
// - THE FOAM (`foam_of`, render/wgsl/water_surface.wgsl's `foam_at`): whitecaps where the waves'
//   surface folds and surf where a wave stands over 0.78 of the depth, each kept a few seconds as
//   it drifts downwind (the waves' own past, worked out again per pixel: no texture, no frame's
//   history), drawn as lace; a line of it where the water laps the shore.
// - THE SHORE'S WET BAND (render/water_shore.wgsl, in the "water" pass before the bodies): the
//   ground a little over the level near the water darkened where the waves run up and wet it.
//
// Not yet (M24g2): the GPU detail FFT, reflections over Hi-Z, the lake's shore mesh, the ladder.
//
// ge:summary The water's surface drawn: the snapped rings, the waves, Fresnel, refraction, glitter
// ge:keywords render water surface ocean lake rings clipmap snapped morph fresnel reflection
// ge:keywords refraction ssr screen-space-reflection glitter beckmann slope-variance
// ge:keywords detail-waves water-copy create_water add_water_body set_water_body
// ge:keywords prepare_water encode_water detail_waves hidden wake rings set_water_wakes kelvin
// ge:keywords shore-mask shore_mask gusts cats-paws stir still-pool planar-mirror planar-reflection
// ge:keywords water-below underside snell-window total-internal-reflection fresnel-exit daylight
// ge:keywords set_water_views water-views foam whitecap jacobian fold surf foam-trail lace
// ge:keywords wet-band shore-wet run-up swash foam_of
// ge:skill render water
// ge:test tests/render_water_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "water/waves.hpp"
#include "water/world.hpp"

namespace ge::render {

inline constexpr uint32_t kWaterGrid = 64;     // a level's quads a side (water.wgsl's override)
inline constexpr int kWaterLevels = 12;        // at most
inline constexpr int kWaterDetailWaves = 32;   // the detail components
inline constexpr int kWaterBodies = 8;         // bodies a Water draws
inline constexpr int kWaterRings = 512;        // a body's wake rings drawn (the newest)
inline constexpr double kWaterlineBand = 0.5;  // m over a level an eye is at its waterline
inline constexpr int kShoreMask = 1024;        // a lake's shore mask's texels a side, at most
inline constexpr double kShoreTexel = 0.5;     // m: the finest texel

// The detail band past the simulation's waves: components from the laid band's top frequency
// to waves `shortest` m long, and the slope variance of the spectrum from there to `finest` m.
struct WaterDetail {
  std::vector<water::Component> waves;
  double tail_mss = 0;  // the mean square slope past the detail band
  double mss = 0;       // the detail components' own
};
WaterDetail detail_waves(const water::Waves& laid, int count, uint32_t seed, double shortest = 0.04,
                         double finest = 0.004);

// How a body is drawn, beside what engine/water says of it.
struct WaterDraw {
  double base_spacing = 0.5;  // level 0's quad, m
  int levels = 9;             // ≤ kWaterLevels
  double rim = 40;            // the last level's rim pushed out this many times its distance
  double refraction = 0.04;   // how far the normal bends the view into the water
  double calm_mss = 2e-4;     // a calm surface's own roughness (the capillary ripples)
  uint32_t seed = 7;          // the detail components' directions and phases
  // Under a roof (a cave's pool, a hall's): the share of the dome the surface sees the sky
  // through (its reflection, the sun and the sky's light), and the room's own light round it, a
  // share of a white face's luminance under the open day, over the rest.
  double open_sky = 1;
  double room = 0;
  // The share of the sun's and the sky's light on the water itself (its colour and glitter); < 0:
  // `open_sky`'s. A pool in a courtyard sees the sky overhead but the walls keep the sun off.
  double daylight = -1;
  // The share of the surface the sun itself reaches (its caustics and the shafts under it); < 0:
  // the day's share. A courtyard's pool under the open sky may lie in a wall's shade.
  double sunlit = -1;
  double stir = 0;       // a still surface's own wandering slope (rad, about): 0 glass
  double gusts = 0;      // 0..1: how strongly the gusts' patches raise and lay the short waves
  bool foam = true;      // the whitecaps, the surf and the line at the shore (`foam_of`)
  bool wet_band = true;  // the shore darkened where the waves run up it
};

// A game's own pictures of the world for one body (`set_water_views`): THE MIRROR, the world
// drawn again reflected in the body's level (the screen's own uv; colour in rgb, alpha 0 where it
// shows the sky or nothing), with its depth (as the frame's view_proj mirrored writes it; the
// distance to a mirrored point is the distance along the reflected ray), seen from over the
// surface or, the eye under it, of the water below it; and THE VIEW UP (the eye under the body),
// the world over the level drawn from the surface straight over the eye looking straight up, by
// `up_view_proj` (relative to that point, world to clip), alpha 0 where it shows the sky.
struct WaterViews {
  int body = -1;       // the body they are of; -1: none
  bool below = false;  // the mirror is of the water under the level (the eye is under it)
  // m: the level the mirror lies in; from over the surface every body at this level (within a
  // centimetre: a pool a bridge splits into two) reads it, since one plane mirrors them all.
  double level = 0;
  bool has_mirror = false, has_up = false;
  wgpu::TextureView mirror, mirror_depth, up;
  float up_view_proj[16] = {};
};

// A body's SHORE MASK: 255 where it may be drawn, under 128 where not, row-major from its box's
// (x0, z0) corner, each texel `texel` m a side; the lake's outline filled even-odd and grown by one
// texel. Past it the texels fall from 127 to 0 over `kShoreReach` m: how near the water the ground
// is, for the shore's wet band.
// A body with no outline (an ocean, a volume) is one texel, drawn everywhere.
struct ShoreMask {
  int cols = 1, rows = 1;
  double texel = 0;
  std::vector<uint8_t> texels{255};
};
ShoreMask shore_mask(const water::Body& body);

// THE FOAM a body's waves make (docs/water.md § Foam): a crest whose surface folds (the Jacobian of
// the horizontal motion of the waves `waves` lays and the `detail` band's, down to `kFoamShortest`
// m, under `fold`) breaks white, and a wave over 0.78 of the depth surfs.
// `wind` is the wind at 10 m that grew the sea (`water::wind_of`), `cover` its whitecap cover
// (`water::whitecap_cover`, the share of the face white, the trails included), and the fold is
// set so the crests breaking now are `kFoamActive` of it (the waves' slopes taken as Gaussian);
// the foam drifts downwind at `drift` m/s. The run-up is how far over the level the waves wet the
// shore (a calm lake's lap included).
inline constexpr double kFoamActive = 0.15;   // the share of the cover that is crests breaking now
inline constexpr double kShoreReach = 12;     // m past a lake's shore its wet band may lie
inline constexpr double kFoamShortest = 1.5;  // m: the shortest wave that folds (faded in to 3 m)
struct WaterFoam {
  double wind = 0, cover = 0;
  double fold = -1e9;      // J under this breaks; far under -1 when nothing can
  double drift = 0;        // m/s downwind
  double hs = 0;           // the laid band's significant height, m
  double runup = 0;        // m over the level
  double fold_length = 0;  // m: the wavelength the folds' slopes weigh most (0: no sea)
};
WaterFoam foam_of(const water::Waves& waves, std::span<const water::Component> detail = {});

struct WaterGpuBody {
  float info[4];
  float box[4];
  float grid[4];
  float rough[4];  // the tail's slope variance, the calm surface's, refraction, the open sky
  float c[4], kd[4], deep[4], emission[4];
  float wake[4];  // rings, their life (s), the shortest wave kept (m), 0
  float air[4];   // the day's share on the water, the stir's slope, the gusts, the wind (m/s)
  float sun[4];   // the share of the surface the sun reaches (the caustics, the shafts), 0, 0, 0
  float view[4];  // the mirror (0 none, 1 over, 2 under), the view up (1), its heading's cos, sin
  // The foam (`foam_of`): the fold's threshold on the Jacobian, the whitecap cover, its drift x, z
  float foam[4];
  float surf[4];  // the laid band's Hs (m), the run-up (m), the folds' wavelength (m), 0
  float up_view_proj[16];
  float levels[kWaterLevels][4];
};

struct Water {
  wgpu::BindGroupLayout layout, copy_layout;
  uint32_t p_copy = 0, p_draw = 0, p_shore = 0;
  wgpu::Texture scene_colour, scene_depth;
  wgpu::TextureView scene_colour_view, scene_depth_view;
  wgpu::BindGroup copy_group;
  wgpu::Buffer indices;
  struct Slot {
    WaterGpuBody staged{};
    WaterDraw draw;
    wgpu::Buffer uniform, waves, rings;
    wgpu::Texture shore;  // its shore mask (`shore_mask`, R8)
    wgpu::TextureView shore_view;
    wgpu::BindGroup group;
    bool hidden = false;  // the caller's switch: not drawn (a lab's dry picture)
    bool shown = true;    // this frame's: not hidden and seen from over its level
    // m: a volume's lowest hull floor (an eye under it is not in it); open water has none
    double floor = -1e300;
    float veil[4] = {0, 0, 0, 0};  // the fog a view under it looks through (water::Optics)
    double mss =
        0;  // the surface's mean square slope: the laid waves', the detail's and the tail's
  };
  std::vector<Slot> bodies;
  // The moving water (render/water_stream.hpp): this frame's mesh and the bodies' optics.
  struct Streams {
    wgpu::BindGroupLayout layout;
    uint32_t p_draw = 0;
    wgpu::Buffer vertices, indices, optics;
    uint64_t vertex_bytes = 0, index_bytes = 0;
    uint32_t count = 0;  // indices
    wgpu::BindGroup group;
  } streams;
  // The surface from below ("water.below"): rgb its light, a its distance from the eye (m; 0 none);
  // and the share of it that is the water's own light (total internal reflection with nothing to
  // mirror), which the view under water fills with its veil.
  uint32_t p_below = 0;
  wgpu::Texture below, below_veil;
  wgpu::TextureView below_view, below_veil_view;
  WaterViews views;                         // the game's (set_water_views)
  wgpu::Texture blank_colour, blank_depth;  // 1 x 1 stand-ins where a game hands in none
  wgpu::TextureView blank_colour_view, blank_depth_view;
  int eye_under = -1;  // this frame's: the body the eye is under (render/underwater.hpp), or -1
  int eye_near = -1;   // this frame's: the body the eye is just over (its waterline), or -1
};

// The pipelines into the manifest.
void create_water(gpu::Device& device, gpu::PipelineSet& pipelines, Water& water,
                  const wgpu::BindGroupLayout& frame);
// The scene's copies at the targets' size (again when the targets change).
void create_water_targets(gpu::Device& device, Water& water, const FrameTargets& targets);
// A body to draw (its waves, box, level and optics read now); its index.
int add_water_body(gpu::Device& device, Water& water, const water::Body& body,
                   const WaterDraw& draw = {});
// The share of body `index`'s surface the sun reaches now (WaterDraw::sunlit), as the sun moves.
void set_water_sunlit(Water& water, int index, double share);
// The body read again (a lake whose waves were laid again, a new look).
void set_water_body(gpu::Device& device, Water& water, int index, const water::Body& body);
// The body's wake rings (water/wake.hpp: the rings of `body` in the world's `wakes`, the newest
// kWaterRings of them), drawn as the simulation sums them; again whenever rings are set off.
void set_water_wakes(gpu::Device& device, Water& water, int index, const water::Wakes& wakes,
                     int body);
// The game's planar mirror and view up for one body (WaterViews; again whenever they are made
// anew, as at a resize; a default WaterViews takes them away).
void set_water_views(gpu::Device& device, Water& water, const WaterViews& views);
// `prepare_water`'s eye: worked out from the bodies' boxes and levels, or the body the game says
// it is in (a classic map's leaf at the eye: brush/app/bsp_liquids.hpp's eye_water_body; -1 the
// air).
inline constexpr int kEyeFromBoxes = -2;
// The levels round this frame's eye, uploaded; a body seen from under its level is not drawn
// from above: the eye is under it (`Water::eye_under`), and "water.below" draws its underside.
void prepare_water(gpu::Device& device, Water& water, const FrameView& view,
                   int eye_in = kEyeFromBoxes);
// "water.copy", "water" (the still bodies, then the moving water) and "water.below".
void encode_water(const Water& water, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                  const wgpu::BindGroup& frame);

}  // namespace ge::render
