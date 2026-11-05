// THE FAR LAND (docs/rendering.md § The far land): the land round a small world that is never
// walked, a "3D skybox", drawn behind everything the world itself draws. A level that stands
// closed under the sky (a classic map's halls under their sky faces) shows it only where the sky
// shows, so it is seen once the eye is high enough to look over the walls: ranges of mountains to
// the horizon, lakes and seas, the snow down to where the air freezes, meadows, forest and rock.
//
// THE SITE is where the world stands: its middle on the plan, the circle that holds all of it
// (`radius_m`), and its lowest point (`floor_y`). The land never rises into that cylinder: inside
// the circle it is a level top just under the floor (the hill the world stands on); past it the
// ground falls away to the valley floor, and the ranges rise from there, so the world's own faces
// hide the land wherever the two could meet, and the depth test needs nothing else.
//
// THE MESH is built once a site on the CPU (`build_far_land`): a polar grid round the site's
// middle, its rings spaced by a constant ratio out to `kFarReach` (fine under the world, coarse
// at the horizon), each vertex its height, its normal, its moisture and its horizon in eight
// bearings; the Earth's curve sinks it with distance. A kind's water is a second polar sheet at
// its level. The ranges' own shadows come from the horizons (no shadow map), and so does the
// shade of a valley's floor under the open sky.
//
// THE KIND (`FarLandKind`, `far_land_kinds`) is the land's recipe: its shape (the hill's fall,
// the ranges' height, span and sharpness, towers, terraces, the lakes dug), its climate (the
// valley's temperature: the snow lies where the air is under freezing by the standard lapse
// rate, and the forest stops where it is under the tree line's warmth) and its colours. Eleven
// are named: eight of the Earth's and three of other worlds'. Nothing is built: no houses.
//
// THE PASS ("far_land") draws after the world's opaque passes and before the sky, testing
// `greater` (reversed-z) and writing depth, colour and motion, so the sky pass and the clouds
// draw over what is left; the clouds march to its depth, so a range stands in the cloud. The
// colour goes through the frame's haze, and past the aerial volume's reach into the kind's own
// far air toward the sky at the horizon. The shader is render/far_land.wgsl.
//
// ge:summary The far land: distant mountains, water, snow and grass round a world, a 3D skybox
// ge:keywords render far-land farland skybox 3d-skybox backdrop distant-terrain mountains horizon
// ge:keywords far_land_kinds find_far_land build_far_land create_far_land prepare_far_land
// ge:keywords encode_far_land snowline lapse-rate tree-line lake sea fjord alps alien
// ge:keywords earth-curvature horizon-shadow far-haze site keep-out
// ge:skill render
// ge:test tests/render_far_land_test.cpp
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

inline constexpr double kFarReach = 90000;       // m: the outer ring, past every horizon's ranges
inline constexpr double kEarthRadius = 6.371e6;  // m: the curve the land sinks on
inline constexpr double kLapse = 0.0065;         // K/m: the standard atmosphere's lapse rate
inline constexpr int kFarBearings = 8;           // the horizons a vertex keeps, 45 degrees apart

enum class FarLiquid : uint8_t { water, lava };

// A FAR LAND'S RECIPE. Heights are m over the valley floor; colours are linear albedo.
struct FarLandKind {
  const char* name;
  const char* what;  // one line, for a list and a caption
  const char* sky;   // the sky type it suits (render/sky_style.hpp)
  // THE SHAPE
  double hill_drop;  // m: from the site's top down to the valley floor
  double hill_run;   // m: over how far past the site's circle
  double near_m;     // m from the site: where the first range rises
  double peak;       // m: the ranges' crests over the valley floor
  double span;       // m: the ranges' feature size (a ridge to the next)
  double ridged;     // 0 rounded hills .. 1 sharp ridges and arêtes
  double towers;     // 0 none .. 1 tall towers and needles standing out of the ranges
  double cones;      // 0 none .. 1 the share of a coarse grid's cells a volcano's cone stands in
  double terraces;   // m: the step of a mesa's terraces (0 none)
  double lakes;      // 0 none .. 1 lakes dug into the valleys (a share of the peak)
  double water;      // m: the water's level over the valley floor (under 0: none)
  // THE CLIMATE
  double temp_c;    // the air on the valley floor, degrees C (the snow line is temp_c / kLapse up)
  double tree_m;    // m over the valley floor where the forest stops (0: no forest)
  double strata_m;  // m: the rock's bands (0 none)
  double clarity_km;  // the far air's visual range past the aerial volume's 8 km
  // THE COLOURS
  float rock[3], rock2[3], grass[3], dry[3], forest[3], snow[3], sand[3];
  float deep[3], shallow[3];  // the water's colour deep and over the shore (lava: crust, glow)
  bool alien;                 // another world's (the rest are the Earth's)
  FarLiquid liquid;
};

// The named kinds, the Earth's first.
std::span<const FarLandKind> far_land_kinds();
// The kind named `name`; null for an unknown word.
const FarLandKind* find_far_land(std::string_view name);

// WHERE THE WORLD STANDS (world metres).
struct FarSite {
  double x = 0, z = 0;    // its middle on the plan
  double radius_m = 100;  // the circle that holds all of it
  double floor_y = 0;     // its lowest point: the land stays under it inside the circle
  uint32_t seed = 1;      // where in the noise the ranges start
};

// One vertex of the far land as the GPU reads it (far_land.wgsl's `FarVertex`, 48 bytes).
struct FarVertex {
  float pos[4];         // x, y, z from the site's middle (m, y the world's); w moisture (-1 water)
  float n[4];           // the normal; w the share of the sky it sees (0..1)
  uint32_t horizon[2];  // the horizon's elevation in kFarBearings bearings, a byte each
  uint32_t pad[2];
};

struct FarMesh {
  std::vector<FarVertex> vertices;
  std::vector<uint32_t> indices;  // the land's rings inside out, then the water's
  uint32_t land_indices = 0;
  int bearings = 0, rings = 0;   // the land's polar grid
  double valley_y = 0;           // the valley floor, world m
  double water_y = -1e30;        // the water's level, world m (none: -1e30)
  double low_y = 0, high_y = 0;  // the land's lowest and highest vertex, world m
};

// The land's height at plan (x, z) (world m) for `kind` round `site`, before the Earth's curve.
double far_height(const FarLandKind& kind, const FarSite& site, double x, double z);
// How far the Earth's curve sinks a point `d` m away, m.
inline double far_sink(double d) { return d * d / (2 * kEarthRadius); }
// The snow line of `kind` warmed by `warm_c`: where the air is at freezing, world m.
double far_snow_line(const FarLandKind& kind, const FarMesh& mesh, double warm_c);

// The mesh for `kind` round `site`: `bearings` x `rings` land vertices (the defaults are what a
// game draws; a test passes fewer) and the water's sheet.
FarMesh build_far_land(const FarLandKind& kind, const FarSite& site, int bearings = 640,
                       int rings = 384);
// One horizon's elevation (rad) from a vertex's packed bytes.
double far_horizon(const FarVertex& v, int bearing);

struct FarLand {
  wgpu::Buffer vertices, indices, params;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_draw = 0;
  uint32_t index_count = 0;
  bool on = false;     // a mesh is uploaded
  bool drawn = false;  // and this frame draws it (prepare_far_land)
  double site[2] = {0, 0};
  double valley_y = 0, water_y = -1e30;
  const FarLandKind* kind = nullptr;
  // far_land.wgsl's `FarParams`, filled by prepare.
  struct Params {
    float eye[4];         // the eye from the site's middle (y the world's), m; w the time, s
    float site[4];        // the valley floor's y, the water's y (world m), its liquid, the glow
    float climate[4];     // the valley's temperature (warmed), the lapse, the tree line's, strata m
    float far[4];         // the far air's visual range m, the towers' share, the site's world x, z
    float colours[9][4];  // rock, rock2, grass, dry, forest, snow, sand, deep, shallow
  } staged{};
};

// The pipeline into the manifest and the uniform block; no mesh yet.
void create_far_land(gpu::Device& device, gpu::PipelineSet& pipelines, FarLand& land,
                     const wgpu::BindGroupLayout& frame);
// `mesh` (build_far_land's for `kind` round `site`) uploaded and drawn from the next frame (a
// load: a new map, a new kind).
void set_far_land(gpu::Device& device, FarLand& land, const FarLandKind& kind, const FarSite& site,
                  const FarMesh& mesh);
// No far land drawn until the next set_far_land.
void clear_far_land(FarLand& land);
// The frame's block: the eye, the climate warmed by `warm_c`; `drawn` false keeps it out of this
// frame (its pass empty).
void prepare_far_land(gpu::Device& device, FarLand& land, const FrameView& view, double warm_c,
                      bool drawn = true);
// The "far_land" pass. `clear`: it is the frame's first opaque pass (it clears the colour, the
// motion and the depth); otherwise it draws over what the world drew. With no land, an empty
// compute pass (the recorder's rows stay in place).
void encode_far_land(const FarLand& land, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                     const wgpu::BindGroup& frame, bool clear);

}  // namespace ge::render
