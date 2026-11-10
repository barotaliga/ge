// The window's world frame (world_draw.hpp).
//
// ge:summary The window's world frame: the BSP map, the effects, sky, bloom, post, the overlay
// ge:keywords world-draw bsp frame create_world_draw draw_world resize_world particles
// ge:keywords enhanced-textures texture-cache relief normal-map parallax
// ge:keywords muzzle-fire explosions-pass hide-hands beams-pass lightning-bolt heat-haze
// ge:skill render brush shooter
#include "shooters_edge/client/world_draw.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_sky.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/light_probe.hpp"
#include "camera/pose.hpp"
#include "classicenhance/cached.hpp"
#include "core/output.hpp"
#include "prof/zones.hpp"
#include "render/beams.hpp"
#include "render/bloom.hpp"
#include "render/clouds.hpp"
#include "render/explosions.hpp"
#include "render/far_land.hpp"
#include "render/frame.hpp"
#include "render/haze.hpp"
#include "render/particles.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/ui_pass.hpp"
#include "render/viewmodel.hpp"
#include "shooters_edge/app/ladder.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/world_extras.hpp"
#include "viewmodel/light.hpp"

namespace shooters_edge::client {

namespace ba = ge::brush::app;
namespace gr = ge::render;

struct WorldDraw {
  const look::Picture* picture = nullptr;
  bool has_map = false;
  // This frame's stops of the quality ladder's rows (look.hpp's Stops: r_quality's pick, else the
  // switches and levels as set) and the switches as drawn at them, which the extras read.
  look::Stops stops;
  ge::classicenhance::Switches on;
  ba::Scene scene;
  ba::BspDraw bsp;
  gr::FarLand far;  // the land round the map, seen through its sky faces (r_farland)
  ba::SkyPick sky;
  ba::SkyPick night;  // the map's own classic night, for an sv_hour after dusk (bsp_sky.hpp)
  bool dark = false;  // its sky texture is dark: the map is seen under its night (dark_sky)
  double top_y = 0;   // the map's highest point, m: the clouds stand over it
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group[2];  // classic, modern
  gr::Sky skypass;
  gr::CloudPass clouds;
  gr::Bloom bloom;
  gr::Post post[2];
  // The classic look's grade to the palette (look.hpp's palette_grade; none: the identity), worked
  // once on the read's thread.
  std::vector<float> classic_grade;
  double gamma = 1;    // the gamma the grades were last made with
  look::Picture made;  // the picture as it was when the world was made
  gr::Particles particles;
  gr::Explosions explosions;   // the muzzle fire, the enhanced blasts, a rocket's flame, hits' dust
  gr::Beams beams;             // the arc guns' bolts (ce_beams)
  gr::Haze haze;               // the hot air behind a nail or a rocket (ce_trails)
  std::vector<float> casters;  // this frame's models as world triangles, for their shadows
  std::vector<ba::CasterBox> caster_boxes;  // and each one's box
  std::vector<gr::BeamShape> beam_shapes;   // kMaxBeamDraws, built each frame
  std::vector<gr::BeamDraw> beam_draws;
  // This frame's births, blasts and flames with the room's light on their smoke (light_at).
  std::vector<gr::Particle> lit_births;
  std::vector<gr::Blast> lit_puffs;
  std::vector<gr::MuzzleFire> lit_flames;
  WorldModels models;
  std::vector<ba::Mover> movers;  // this frame's (place_brushes)
  gr::HeldModels held;
  bool has_held = false;
  ge::viewmodel::Placement last_place;  // the weapon's placement a frame ago
  bool held_fresh = true;
  bool own_sky = false;  // the view's sky as the caller dressed it (the title's)
  ge::brush::LightProbe probe;
  WorldExtrasPtr extras;  // the modern look's water, liquids, flames and weather (a map's only)
  std::vector<gr::Particle> fire_births;  // this frame's sparks off the bonfires
  gr::UiPass ui;
  gr::FrameRecorder rec;
  uint32_t w = 0, h = 0;
  // The build in steps (begin_world, then a step a frame in world_compiled): the read it is made
  // from, the steps made, what they need and the first step's error.
  WorldReadPtr pending;
  int built = 0;
  int warm_cubes = -1;  // a warm-up frame's cube budget (warm_world); -1: a frame shown
  wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
  std::span<const ge::viewmodel::HeldModel> weapons;
  std::string build_error;
  look::KeptStops kept_stops;  // r_quality's pick, kept between frames
};

void WorldDrawDeleter::operator()(WorldDraw* w) const { delete w; }

namespace {

// The weapon's light off the floor's: the share that is all round and the share from above, and
// the brightest the floor's light is taken as (twice the light as baked).
constexpr float kHeldAmbient = 0.55f, kHeldDirect = 0.45f, kHeldMost = 1.6f;
constexpr float kParticleSoftM = 0.06f;  // the particles' softening against what is behind them

// THE LIGHT AT A POINT (engine metres), for the smoke there: the open sky's share (1 when a march
// straight up the map's tree meets a sky leaf before a solid one) and the lightmap's light on the
// floor under it, in whites (a luxel of 128 a surface as painted). No map: the open air's.
constexpr float kSkyStepU = 32;
constexpr int kSkySteps = 128;  // 4096 units up
constexpr int kSkyContents = -6, kSolidContents = -2;
gr::BlastLight light_at(const WorldDraw& f, const double e[3]) {
  gr::BlastLight l;
  if (!f.has_map) return l;
  const ge::brush::Map& m = f.scene.map;
  float p[3] = {static_cast<float>(e[0] / ge::brush::kUnitM),
                static_cast<float>(e[2] / ge::brush::kUnitM),
                static_cast<float>(e[1] / ge::brush::kUnitM)};
  float rgb[3];
  if (ge::brush::light_below(f.probe, m, p, rgb))
    for (int k = 0; k < 3; ++k) l.lit[k] = 2 * rgb[k];
  l.sky = 0;
  for (int i = 1; i < kSkySteps; ++i) {
    const float q[3] = {p[0], p[1], p[2] + kSkyStepU * static_cast<float>(i)};
    const uint32_t leaf = ge::brush::leaf_at(m, q);
    if (leaf >= m.leaves.size()) break;
    const int32_t c = m.leaves[leaf].contents;
    if (c == kSkyContents) l.sky = 1;
    if (c == kSkyContents || c == kSolidContents) break;
  }
  return l;
}

// The open sky's light on a smoke puff, in whites, as `light_at`'s share of it.
constexpr float kOpenWhites = 0.8f;

// Everything sized to the targets: made at create and again at a resize.
void make_targets(ge::gpu::Device& d, WorldDraw& f) {
  f.targets = gr::create_targets(d, "shooters_edge", f.w, f.h);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, f.w, f.h, inputs);
  for (int p = 0; p < 2; ++p)
    f.post_group[p] = gr::post_group(d, f.post[p], f.targets.hdr_view, f.bloom.view);
  gr::bind_particle_targets(d, f.particles, f.targets);
  gr::bind_explosion_targets(d, f.explosions, f.targets);
  gr::bind_beam_targets(d, f.beams, f.targets);
  gr::bind_haze_targets(d, f.haze, f.targets);
  if (f.has_map) ba::create_bsp_targets(d, f.bsp, f.targets);
  if (f.extras) bind_extras_targets(d, *f.extras, f.targets);
}

// The pack's textures as classic-enhance left them in its cache (`user` the pack's cache folder):
// upscaled, with their glow; the map's own where the cache has none.
bool from_cache(void* user, std::string_view name, uint32_t w, uint32_t h,
                std::span<const uint8_t> /*indexed*/, std::span<const uint8_t> /*palette*/,
                std::vector<uint8_t>* rgba) {
  return ge::classicenhance::cached_texture(*static_cast<const std::filesystem::path*>(user), name,
                                            w, h, rgba);
}

// Their relief (normal, height, roughness) beside them, for the modern look to light them by.
bool surface_from_cache(void* user, std::string_view name, uint32_t w, uint32_t h,
                        std::vector<uint8_t>* rgba) {
  return ge::classicenhance::cached_surface(*static_cast<const std::filesystem::path*>(user), name,
                                            w, h, rgba);
}

// The weapon's draw this frame: its model and placement, lit by the baked light on the floor under
// the eye (the lightmap's 0.5 the light as baked, from above) and by its own flash.
gr::HeldDraw held_draw(WorldDraw& f, const shooters_edge::view::HandFrame& h,
                       const gr::FrameView& view, bool engine_fire, float shown) {
  namespace vm = ge::viewmodel;
  gr::HeldDraw g;
  g.model = static_cast<uint32_t>(h.weapon);
  // A spawn assembles it muzzle first, along the held frame's forward.
  if (shown < 1 && g.model < f.weapons.size()) {
    float lo[3], hi[3];
    vm::held_bounds(f.weapons[g.model], lo, hi);
    g.warp.shown = shown;
    g.warp.axis[0] = g.warp.axis[1] = 0;
    g.warp.axis[2] = 1;
    g.warp.low = lo[2];
    g.warp.high = hi[2];
  }
  g.frame_a = h.frame_a;
  g.frame_b = h.frame_b;
  g.blend = static_cast<float>(h.frame_t);
  g.now = h.placement;
  g.last = f.held_fresh ? h.placement : f.last_place;
  f.last_place = h.placement;
  f.held_fresh = false;
  g.fov_y = h.placement.fov_y;
  g.lamps = false;
  g.hide_flame = !h.flashing || engine_fire;  // the engine's muzzle fire burns in its place
  g.hide_hands = !h.hands;
  g.lift = h.lift;
  g.opacity = h.opacity;
  vm::Probe probe;
  float rgb[3];
  const float at[3] = {static_cast<float>(view.eye[0] / ge::brush::kUnitM),
                       static_cast<float>(view.eye[2] / ge::brush::kUnitM),
                       static_cast<float>(view.eye[1] / ge::brush::kUnitM)};
  if (f.has_map && ge::brush::light_below(f.probe, f.scene.map, at, rgb)) {
    for (int k = 0; k < 3; ++k) {
      const float lit = std::min(2 * rgb[k], kHeldMost);
      probe.ambient[k] = kHeldAmbient * lit;
      probe.direct[k] = kHeldDirect * lit;
    }
  }
  ge::camera::Pose pose;
  pose.eye = {view.eye[0], view.eye[1], view.eye[2]};
  pose.yaw = view.heading;
  pose.pitch = view.pitch;
  pose.fov_y = view.fov_y;
  pose.near_m = view.near_m;
  const float grip[3] = {static_cast<float>(h.placement.m[3]), static_cast<float>(h.placement.m[7]),
                         static_cast<float>(h.placement.m[11])};
  const std::span<const vm::PointLight> lights =
      h.flashing ? std::span<const vm::PointLight>(&h.flash, 1) : std::span<const vm::PointLight>();
  g.light = vm::light_held(probe, lights, pose, grip);
  return g;
}

}  // namespace

struct WorldRead {
  look::Picture picture;  // as it was when the read began: the world is made in it
  bool has_map = false;
  ba::Scene scene;
  ba::SkyPick sky;
  ba::SkyPick night;  // the map's own classic night, for an sv_hour after dusk
  bool dark = false;
  double top_y = 0;
  std::vector<uint8_t> grade_palette;
  std::vector<float> classic_grade;  // the classic look's grade to the palette (look.hpp)
  ge::brush::LightProbe probe;
  ModelsRead models;
  // The far land (r_farland: render/far_land.hpp), its mesh built on the read's thread.
  const gr::FarLandKind* far_kind = nullptr;
  gr::FarSite far_site;
  gr::FarMesh far_mesh;
};

void WorldReadDeleter::operator()(WorldRead* r) const { delete r; }

WorldReadPtr read_world(const SearchPath& files, const LoadedMap& map, const look::Picture& picture,
                        const ModelLoad& models, std::string& error, const GameAdapter& game) {
  GE_ZONE("world.read");
  WorldReadPtr r(new WorldRead);
  r->picture = picture;
  r->classic_grade = look::palette_grade({});
  if (!map.from_file) return r;
  std::vector<uint8_t> store;
  std::span<const uint8_t> palette = find_file(files, "gfx/palette.lmp", store);
  std::vector<uint8_t> grey;
  if (palette.size() < 768) {  // no palette in the packs: a grey ramp, so the map still shows
    grey.resize(768);
    for (size_t i = 0; i < 768; i++) grey[i] = static_cast<uint8_t>(i / 3);
    palette = grey;
    ge::core::output::status("shooters_edge: no gfx/palette.lmp in the packs; drawing grey");
  }
  const std::vector<uint8_t> palette_copy(palette.begin(), palette.end());
  if (grey.empty()) r->grade_palette = palette_copy;
  r->classic_grade = look::palette_grade(r->grade_palette);  // a search of the palette: on here
  // The enhanced cache, when this pack's were enhanced (the in-game menu's ENHANCE): keyed by the
  // packs' bytes, looked up once for the textures, the rebake and the skins.
  const bool any = picture.ce.textures || picture.ce.rebake || picture.ce.models;
  const std::filesystem::path cache = any && !content_packs(files).empty()
                                          ? ge::classicenhance::find_cache(content_packs(files))
                                          : std::filesystem::path{};
  // ce_textures: the enhanced textures.
  ba::TextureSource source;
  if (picture.ce.textures && !cache.empty()) {
    source.replace = from_cache;
    source.surface = surface_from_cache;
    source.user = const_cast<std::filesystem::path*>(&cache);
    ge::core::output::status("shooters_edge: the enhanced textures from " + cache.string());
  }
  // The map's textures as the game's adapter reads them.
  std::vector<uint8_t> picture_store;
  const std::vector<ge::brush::MapPicture> pictures =
      game.map_textures(game, files, map.name, map.file, picture_store);
  source.pictures = &pictures;
  ba::Pack pack;  // the map file alone, as the scene reader takes it
  pack.bytes = map.file;
  pack.files.push_back({"maps/" + map.name + ".bsp", 0, static_cast<uint32_t>(map.file.size())});
  if (!ba::load_scene(pack, pack.files[0].name, palette_copy, &r->scene, &error, source)) {
    error = "the map " + map.name + " cannot be drawn: " + error;
    return nullptr;
  }
  // ce_rebake: the map's light rebaked into the cache (its lightmaps stage); the modern look
  // adds its bounce under the live lights (look.cpp).
  if (picture.ce.rebake && !cache.empty()) {
    ge::brush::Rebaked baked;
    if (ge::classicenhance::cached_rebake(cache, map.name, r->scene.map.vertices.size(), &baked)) {
      ba::attach_rebake(r->scene, std::move(baked));
      ge::core::output::status("shooters_edge: the rebaked light of " + map.name + " from " +
                               cache.string());
    }
  }
  r->has_map = true;
  r->sky = ba::pick_sky(r->scene.sky_colours, map.name);
  r->night = r->sky;
  ba::night_over(r->night, map.name);
  r->dark = ba::dark_sky(r->sky);
  r->top_y = r->scene.maxs[1];
  bool known = true;
  r->far_kind = ba::far_land_for(r->sky, map.name, picture.far_land, &known);
  if (!known) {
    ge::core::output::status("shooters_edge: r_farland " + picture.far_land +
                             " names no far land; next: set it to auto, off or a kind its help "
                             "lists");
  }
  if (r->far_kind != nullptr) {
    GE_ZONE("world.far_land");
    r->far_site = ba::far_site_of(r->scene, map.name);
    r->far_mesh = gr::build_far_land(*r->far_kind, r->far_site);
  }
  r->probe = ge::brush::make_light_probe(r->scene.map);
  // ce_models: the models' enhanced skins, when this pack's were enhanced.
  ModelLoad load = models;
  if (picture.ce.models) load.cache = cache;
  r->models = read_models(files, palette_copy, r->scene.map, load, game);
  return r;
}

namespace {

// The build's steps, each a few to twenty ms (the shaders parsed, the textures uploaded): the
// frame's passes; the effects, the weapon and the 2D layer; the map; its models; then the extras'
// parts.
constexpr int kBuildSteps = 4 + kExtrasParts;

void build_step(ge::gpu::Device& d, WorldDraw& f) {
  const look::Picture& made = f.made;
  WorldRead& read = *f.pending;
  const int step = f.built++;
  if (step == 0) {
    f.uniforms = gr::make_buffer(d, "shooters_edge.uniforms", sizeof(gr::FrameUniforms),
                                 wgpu::BufferUsage::Uniform);
    f.layout = gr::frame_layout(d);
    gr::create_sky(d, f.pipelines, f.skypass, f.uniforms, f.layout);
    gr::create_clouds(d, f.pipelines, f.clouds, f.layout);
    gr::BloomSettings bs;
    bs.strength = made.ce.bloom ? look::post_settings(look::Look::modern, made).bloom : 0;
    gr::create_bloom_pipelines(d, f.pipelines, f.bloom, bs);
    for (int p = 0; p < 2; ++p)
      gr::create_post(d, f.pipelines, f.post[p], f.layout, f.format,
                      look::post_settings(p == 0 ? look::Look::classic : look::Look::modern, made));
    f.gamma = f.picture->gamma;
    f.classic_grade = std::move(read.classic_grade);
    gr::set_grade(d, f.post[0], look::gamma_grade(f.classic_grade, f.gamma));
    gr::set_grade(d, f.post[1], look::gamma_grade(gr::identity_grade(), f.gamma));
  } else if (step == 1) {
    // Softened over a few centimetres only: a hit's dust and a scorch's smoke live a hand's width
    // off the wall, where a wider softening would fade them to nothing.
    gr::ParticleSettings ps;
    ps.soft_m = kParticleSoftM;
    ps.collide = true;  // sparks, chips and debris meet the map's brushes and its models
    gr::create_particles(d, f.pipelines, f.particles, f.layout, ps);
    gr::create_explosions(d, f.pipelines, f.explosions, f.layout);
    // The marks' burns too (world_fx.hpp's marks, render/marks.hpp).
    gr::create_beams(d, f.pipelines, f.beams, f.layout, gr::mark_beam_settings(kMarks));
    gr::create_haze(d, f.pipelines, f.haze, f.layout);
    f.beam_shapes.resize(gr::kMaxBeamDraws);
    f.beam_draws.reserve(gr::kMaxBeamDraws);
    if (!f.weapons.empty()) {
      if (gr::create_viewmodel(d, f.pipelines, f.held, f.layout, f.weapons))
        f.has_held = true;
      else
        f.build_error =
            "the weapons did not upload (their skins do not fit the atlas); next: check "
            "games/shooters_edge/content/models/view";
    }
    if (f.has_map) f.probe = std::move(read.probe);
    gr::create_ui_pass(d, f.pipelines, f.ui, f.format);
  } else if (!f.has_map) {
    f.built = kBuildSteps;  // no map: nothing more to make
  } else if (step == 2) {
    ba::create_bsp(d, f.pipelines, f.bsp, f.scene, f.layout);
    gr::create_far_land(d, f.pipelines, f.far, f.layout);
    if (read.far_kind != nullptr)
      gr::set_far_land(d, f.far, *read.far_kind, read.far_site, read.far_mesh);
  } else if (step == 3) {
    create_models(d, f.pipelines, f.models, f.layout, std::move(read.models));
  } else {
    // The live switches and weather (read every frame), not the snapshot the world was made in.
    if (!f.extras) f.extras = new_extras(f.scene, f.on, f.picture->weather, f.stops);
    create_extras_part(d, f.pipelines, f.layout, f.skypass, *f.extras, step - 4);
  }
  if (f.built >= kBuildSteps) f.pending.reset();
}

}  // namespace

WorldDrawPtr begin_world(ge::gpu::Device& d, WorldReadPtr read, uint32_t width, uint32_t height,
                         wgpu::TextureFormat format, const look::Picture& picture,
                         std::span<const ge::viewmodel::HeldModel> weapons, std::string& error) {
  // A map's frame is made while the map loads, the first and every later one: its pipelines are
  // the load's, not a hitch mid-run.
  d.loading = true;
  WorldDrawPtr f(new WorldDraw);
  f->picture = &picture;
  f->made = read->picture;
  f->w = std::max(1u, width);
  f->h = std::max(1u, height);
  f->has_map = read->has_map;
  f->scene = std::move(read->scene);
  f->sky = read->sky;
  f->night = read->night;
  f->dark = read->dark;
  f->top_y = read->top_y;
  f->format = format;
  f->weapons = weapons;
  f->pending = std::move(read);
  build_step(d, *f);
  if (!f->build_error.empty()) {
    error = f->build_error;
    return nullptr;
  }
  return f;
}

bool world_compiled(ge::gpu::Device& d, WorldDraw& f) {
  if (!f.build_error.empty()) return true;  // finish_world says it
  if (f.built < kBuildSteps) {
    build_step(d, f);
    return false;
  }
  return f.pipelines.ready(d);
}

bool finish_world(ge::gpu::Device& d, WorldDraw& f, uint32_t width, uint32_t height,
                  std::string& error) {
  while (f.built < kBuildSteps && f.build_error.empty()) build_step(d, f);
  if (!f.build_error.empty()) {
    error = f.build_error;
    return false;
  }
  if (!f.pipelines.finish(d)) {
    error = "the world's pipelines did not build; next: read the gpu: lines above";
    return false;
  }
  ge::gpu::end_load(d);
  f.frame = gr::frame_group(d, f.layout, f.uniforms, f.skypass);
  f.rec.init(d, "shooters_edge.timer", gr::kMaxPasses);  // every pass timed
  f.w = std::max(1u, width);
  f.h = std::max(1u, height);
  make_targets(d, f);
  return true;
}

WorldDrawPtr create_world_draw(ge::gpu::Device& d, const SearchPath& files, const LoadedMap& map,
                               uint32_t width, uint32_t height, wgpu::TextureFormat format,
                               const look::Picture& picture,
                               std::span<const ge::viewmodel::HeldModel> weapons,
                               std::string& error, const ModelLoad& models,
                               const GameAdapter& game) {
  WorldReadPtr read = read_world(files, map, picture, models, error, game);
  if (!read) return nullptr;
  WorldDrawPtr f = begin_world(d, std::move(read), width, height, format, picture, weapons, error);
  if (!f || !finish_world(d, *f, width, height, error)) return nullptr;
  return f;
}

const WorldModels& world_models(const WorldDraw& w) { return w.models; }

std::span<const ba::PointLight> world_lights(const WorldDraw& w) { return w.scene.lights; }

std::vector<gr::Particle> world_particles(ge::gpu::Device& d, const WorldDraw& w) {
  return gr::read_particles(d, w.particles);
}

void resize_world(ge::gpu::Device& d, WorldDraw& f, uint32_t width, uint32_t height) {
  f.w = std::max(1u, width);
  f.h = std::max(1u, height);
  make_targets(d, f);
}

void own_sky(WorldDraw& f, bool own) { f.own_sky = own; }

bool world_stale(const WorldDraw& f) {
  const look::Picture& a = f.made;
  const look::Picture& b = *f.picture;
  return a.ce.textures != b.ce.textures || a.ce.models != b.ce.models ||
         a.ce.rebake != b.ce.rebake || a.ce.bloom != b.ce.bloom || a.bloom != b.bloom ||
         a.far_land != b.far_land;
}

namespace {

// The map's own sky (its pick) at the picture's sv_hour (the map's own at -1), dressed on `view`;
// the title dresses its own.
ba::SkyPick dress_world_sky(const WorldDraw& f, gr::FrameView& view) {
  // At the map's own hour (-1) a dark sky texture is the map's night, a bright one its day.
  ba::SkyPick sky = f.dark ? f.night : f.sky;
  if (f.picture->weather.hour >= 0) ba::at_hour(sky, f.night, f.picture->weather.hour);
  if (!f.own_sky) ba::dress_sky(view, sky, f.top_y, view.time_s);
  return sky;
}

}  // namespace

void world_sun(const WorldDraw& f, gr::FrameView view, double out[3]) {
  dress_world_sky(f, view);
  gr::sun_direction(view, out);
}

void draw_world(ge::gpu::Device& d, WorldDraw& f, gr::FrameView view, const FxFrame& fx,
                const shooters_edge::view::HandFrame* held, double dt,
                const ge::ui::DrawList& overlay, const wgpu::TextureView& target) {
  GE_ZONE("world");
  const look::Look lk = look::look_of(*f.picture);
  const int p = lk == look::Look::classic ? 0 : 1;
  {
    std::string refused;
    f.stops = look::picture_stops(*f.picture, f.w, f.h, f.kept_stops, refused);
    f.on = look::drawn_switches(f.picture->ce, f.stops);
    if (!refused.empty())
      ge::core::output::warn("shooters_edge: the picture's quality word was refused: " + refused +
                             "; next: set r_quality to custom, low, medium, high, top, auto or "
                             "0..1 (see docs/shooters-edge.md)");
  }
  if (f.picture->gamma != f.gamma) {  // the brightness changed (the menu's slider, `gamma`)
    f.gamma = f.picture->gamma;
    gr::set_grade(d, f.post[0], look::gamma_grade(f.classic_grade, f.gamma));
    gr::set_grade(d, f.post[1], look::gamma_grade(gr::identity_grade(), f.gamma));
  }
  // The map's own sky (its pick), keeping the eye and the look the caller set.
  const ba::SkyPick sky = dress_world_sky(f, view);
  view.altitude_m = 100;
  if (!f.own_sky) {  // a map's cloud seen through its sky faces: every pixel, every frame
    view.clouds.quarter = false;
    view.clouds.spread = false;
  }
  ba::Options o = look::world_options(lk, {}, *f.picture, f.stops);
  if (f.warm_cubes >= 0) {  // warmed up over frames: the first frame's cubes under a budget too
    o.first_all = false;
    o.fresh_cubes = f.warm_cubes;
  }
  o.sky_light = sky.sky_light;
  o.sky_seen = sky.sky_seen;
  for (int k = 0; k < 3; ++k) o.sky_tint[k] = sky.tint[k];
  // The switches on the options and the weather on the view, before the uniforms take the view.
  if (f.extras) {
    GE_ZONE("world.dress");
    dress_extras(*f.extras, view, o, dt);
  }
  f.rec.begin_frame(d);
  gr::fill_uniforms(f.staged, view, f.w, f.h);
  d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  gr::prepare_sky(f.skypass, view);
  if (f.has_map) {
    GE_ZONE("world.bsp");
    ba::set_dynamic_lights(f.bsp, fx.lights);
    // The moving models' shadows (M54): every model near the eye and the player himself.
    shadow_casters(f.models, fx.models, fx.has_self ? &fx.self : nullptr, view, f.casters,
                   f.caster_boxes);
    ba::set_shadow_casters(f.bsp, f.casters, f.caster_boxes);
    place_brushes(f.models, fx.models, f.movers);
    ba::set_movers(f.bsp, f.movers);
    ba::prepare_bsp(d, f.bsp, view, o);
    // The far land shows through the sky faces where our sky does (the modern look's own sky), at
    // the skies' top stop.
    const bool far =
        o.mode == ba::Mode::v2 && !o.pack_sky && !f.own_sky && look::far_land_at(f.stops.skies);
    gr::prepare_far_land(d, f.far, view, f.picture->far_warm, far);
  }
  if (f.warm_cubes >= 0) {  // a warm-up (warm_world): the sky's tables and the new shadows alone
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    gr::encode_sky_tables(f.skypass, f.pipelines, f.rec, e);
    if (f.has_map) ba::encode_bsp_shadows(f.bsp, f.pipelines, f.rec, e);
    f.rec.end_frame(e);
    const wgpu::CommandBuffer c = e.Finish();
    d.queue.Submit(1, &c);
    f.rec.after_submit(d);
    return;
  }
  if (f.extras) {
    GE_ZONE("world.extras");
    // The players wading in lava or slime part its surface (ce_wade).
    wade_extras(*f.extras, fx.models, fx.has_self ? &fx.self : nullptr, view.time_s);
    prepare_extras(d, *f.extras, f.bsp, view, o, f.staged);
  }
  {
    GE_ZONE("world.effects");
    // The smoke lit as the room round it is: a puff (a particle that spreads) shines the light
    // where it was born, the blasts' and the flames' smoke the light where they are.
    f.lit_births.assign(fx.births.begin(), fx.births.end());
    for (gr::Particle& q : f.lit_births) {
      if (q.grow <= 0 || q.kelvin != 0) continue;
      const double at[3] = {q.p[0], q.p[1], q.p[2]};
      const gr::BlastLight l = light_at(f, at);
      for (int k = 0; k < 3; ++k)
        q.tint[k] *= q.albedo * (l.lit[k] + kOpenWhites * static_cast<float>(l.sky));
      q.albedo = 0;
      q.kelvin = -1;
      q.cool_s = 0;
    }
    f.lit_puffs.assign(fx.puffs.begin(), fx.puffs.end());
    for (gr::Blast& b : f.lit_puffs) b.light = light_at(f, b.at);
    f.lit_flames.assign(fx.flames.begin(), fx.flames.end());
    for (gr::MuzzleFire& m : f.lit_flames) m.light = light_at(f, m.pose.at);
    gr::emit_particles(f.particles, f.lit_births);
    if (f.extras) {
      f.fire_births.clear();
      extras_births(*f.extras, view.time_s, f.fire_births);
      gr::emit_particles(f.particles, f.fire_births);
    }
    f.particles.settings.glow = static_cast<float>(look::effects_glow(lk, *f.picture));
    gr::prepare_particles(d, f.particles, dt, view, f.w, f.h);
    gr::prepare_explosions(d, f.explosions, f.lit_puffs, view, f.w, f.h, f.lit_flames);
    f.beam_draws.clear();
    for (const HeldBeam& b : fx.beams) {
      if (f.beam_draws.size() >= f.beam_shapes.size()) break;
      gr::BeamShape& shape = f.beam_shapes[f.beam_draws.size()];
      const gr::BeamLook* look = b.across ? &effects::prong_arc_look() : &effects::bolt_look();
      gr::build_beam(*look, b.shot, view.time_s, shape);
      if (shape.count > 0) f.beam_draws.push_back({&shape, look, b.shot.seed});
    }
    gr::prepare_beams(d, f.beams, f.beam_draws, fx.burns, view, gr::kHotEmberCd);
    gr::prepare_haze(d, f.haze, fx.haze, view);
  }
  if (f.has_map) {
    GE_ZONE("world.models");
    const FxSwitches on = fx_switches(*f.picture);
    const InPlace in_place{.flames = o.mode == ba::Mode::v2 && f.picture->ce.flames,
                           .flares = on.flares,
                           .bubbles = on.bubbles};
    prepare_models(d, f.models, f.scene.map, fx.models, view, o.brightness, o.minlight, !o.crisp,
                   in_place, fx.lights, o.dynamic_strength);
  }
  // Under a lava's or a slime's level the modern look draws the liquid round the eye itself.
  const bool round = f.extras && liquid_round_eye(*f.extras);
  gr::set_post_blend(d, f.post[p], round ? fx.blend_dry : fx.blend);
  // The local player's own arrival opens the whole view out of a flash and a warp (r_arrival).
  gr::set_post_warp(
      d, f.post[p],
      gr::arrival_warp(fx.arrival_s, fx_switches(*f.picture).arrival, fx.arrival_tint));
  const bool weapon = f.has_held && held != nullptr && held->drawn;
  if (weapon) {
    // The engine's fire burns in place of the file's flash, and its arc across a lightning gun's
    // prongs in place of the file's.
    const FxSwitches on = fx_switches(*f.picture);
    gr::HeldDraw draw =
        held_draw(f, *held, view, on.muzzles || (held->arc && on.beams), fx.held_shown);
    gr::prepare_viewmodel(d, f.held, std::span<const gr::HeldDraw>(&draw, 1), view, f.w, f.h);
  } else {
    f.held_fresh = true;
  }
  wgpu::CommandBuffer c;
  {
    GE_ZONE("world.encode");
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    gr::encode_particles_sim(f.particles, f.pipelines, f.rec, e);
    gr::encode_sky_tables(f.skypass, f.pipelines, f.rec, e);
    if (f.extras) encode_extras_sim(*f.extras, f.pipelines, f.rec, e);
    if (f.has_map) ba::encode_bsp(f.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
    encode_models(f.models, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_far_land(f.far, f.pipelines, f.rec, e, f.targets, f.frame, false);
    gr::encode_sky(f.skypass, f.pipelines, f.rec, e, f.targets, f.frame);
    // The map's cloud from the skies' medium stop up (none to fill under the pack's own sky).
    if (f.picture->ce.clouds && (f.own_sky || look::cloud_at(f.stops.skies)))
      gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, view, f.staged);
    if (f.has_map) ba::encode_bsp_veil(f.bsp, f.pipelines, f.rec, e, f.targets);
    if (f.extras) encode_extras(*f.extras, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_particles(f.particles, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_explosions(f.explosions, f.pipelines, f.rec, e, f.targets, f.frame);
    if (!f.beam_draws.empty() || !fx.burns.empty())
      gr::encode_beams(f.beams, f.pipelines, f.rec, e, f.targets, f.frame);
    // After everything the hot air is in front of: it bends the smoke and the fire behind it too.
    gr::encode_haze(f.haze, f.pipelines, f.rec, e, f.targets, f.frame);
    encode_model_sprites(f.models, f.pipelines, f.rec, e, f.targets, f.frame);
    if (weapon) gr::encode_viewmodel(f.held, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
    gr::encode_post(f.post[p], f.pipelines, f.rec, e, f.frame, f.post_group[p], target);
    if (overlay.size() > 0) {
      gr::prepare_ui(d, f.ui, overlay, f.w, f.h);
      gr::encode_ui(f.ui, f.pipelines, f.rec, e, target);
    }
    f.rec.end_frame(e);
    c = e.Finish();
  }
  GE_ZONE("world.submit");
  d.queue.Submit(1, &c);
  f.rec.after_submit(d);
}

bool warm_world(ge::gpu::Device& d, WorldDraw& f, const gr::FrameView& view, int cubes) {
  f.warm_cubes = std::max(cubes, 0);
  draw_world(d, f, view, FxFrame{}, nullptr, 0, ge::ui::DrawList{}, {});
  f.warm_cubes = -1;
  return !f.has_map || f.bsp.shadows_waiting == 0;
}

void record_span(const WorldDraw& f, PassTimes& p) {
  span_add(p, f.rec.stats.gpu_span_ms());
  for (int i = 0; i < f.rec.stats.count; i++) {
    const gr::PassStat& s = f.rec.stats.passes[size_t(i)];
    if (s.draws > 0 || s.dispatches > 0) pass_add(p, s.name, s.gpu_ms);
  }
}

}  // namespace shooters_edge::client
