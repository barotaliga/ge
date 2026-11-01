// Fall Line's pass table, the frame's creation and its draw.
//
// ge:summary Fall Line's pass table, frame creation and draw
// ge:keywords fall-line pass-table frame draw submit
// ge:skill render
#include "fall_line/app/frame.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>

#include "core/num.hpp"
#include "core/output.hpp"
#include "fall_line/app/ladder.hpp"
#include "fall_line/app/lakes.hpp"
#include "fall_line/app/lamps.hpp"
#include "fall_line/app/look.hpp"
#include "fall_line/app/mists.hpp"
#include "fall_line/app/scene.hpp"
#include "fall_line/app/sky_type.hpp"
#include "fall_line/app/thrown.hpp"
#include "mountain/air/air.hpp"
#include "mountain/air/marks.hpp"
#include "mountain/app/building_look.hpp"
#include "mountain/app/cloud_field.hpp"
#include "mountain/app/ice_look.hpp"
#include "mountain/app/village_look.hpp"
#include "mountain/app/weather_look.hpp"
#include "prof/zones.hpp"
#include "wind/cold.hpp"
#include "wind/field.hpp"
#include "wind/precip.hpp"

namespace fall_line::app {

namespace {

namespace r = ge::render;

// The pass table: the order `draw` runs, and what each pass reads and writes.
constexpr r::PassRow kPassTable[] = {
    {"sky.tables", "", "sky.view sky.moon_view sky.aerial sky.light"},
    {"skinning", "", "figures.posed"},
    {"terrain.cull", "terrain.pyramid", "terrain.tiles terrain.indirect"},
    {"instances.cull", "instances figures.posed hiz.last", "instances.early"},
    {"terrain.horizon", "terrain.height", "terrain.horizon"},
    {"trails.fine", "", "trails.fine"},
    {"trails.coarse", "", "trails.coarse"},
    {"particles.sim", "terrain.height", "particles"},
    {"precip.sim", "", "precip"},
    {"shadows.cull", "instances", "shadows.lists"},
    {"shadows.near", "instances shadows.lists", "shadows.maps"},
    {"shadows.far", "instances shadows.lists", "shadows.maps"},
    {"shadows.figures", "instances figures.posed shadows.lists", "shadows.maps"},
    {"lamps.cull", "instances", "lamps.lists"},
    {"lamps.shadows", "instances figures.posed lamps.lists", "lamps.atlas"},
    {"fog.inject", "shadows.maps terrain.horizon sky.aerial sky.light fog.history lamps.atlas",
     "fog.media"},
    {"fog.integrate", "fog.media", "fog.volume"},
    {"terrain.depth", "terrain.tiles terrain.indirect terrain.height", "depth"},
    {"instances.depth", "instances figures.posed instances.early", "depth"},
    {"hiz", "depth", "hiz"},
    {"instances.cull.late", "instances hiz", "instances.late"},
    {"instances.depth.late", "instances figures.posed instances.late", "depth"},
    {"ao", "depth", "ao"},
    {"terrain",
     "terrain.tiles terrain.indirect terrain.height terrain.horizon shadows.maps sky.aerial "
     "sky.light snow.ground trails.fine trails.coarse depth fog.volume ao lamps.atlas mirror.last",
     "hdr motion"},
    {"instances",
     "instances figures.posed instances.early instances.late terrain.horizon shadows.maps "
     "sky.aerial sky.light depth fog.volume ao lamps.atlas mirror.last",
     "hdr motion"},
    {"sky", "sky.view sky.moon_view depth fog.volume", "hdr motion"},
    {"clouds.fresh", "depth sky.aerial sky.light", "clouds.fresh"},
    {"clouds.march", "depth clouds.history clouds.fresh sky.aerial sky.light", "clouds.quarter"},
    {"clouds", "depth clouds.quarter sky.aerial sky.light fog.volume", "hdr"},
    {"water.copy", "hdr depth", "water.scene"},
    {"water", "water.scene sky.view sky.aerial sky.light depth fog.volume", "hdr motion depth"},
    {"water.under", "water.scene sky.light", "hdr"},
    {"particles", "particles terrain.horizon shadows.maps sky.aerial sky.light depth fog.volume",
     "hdr motion"},
    {"flames", "depth fog.volume", "hdr"},
    {"precip", "precip terrain.horizon sky.aerial sky.light depth fog.volume", "hdr motion"},
    {"bolts", "depth", "hdr motion"},
    {"mirror.keep", "hdr depth", "mirror.last"},
    {"taa", "hdr motion depth taa.history sky.light", "taa.history"},
    {"bloom", "taa.history", "bloom"},
    {"lens", "taa.history depth sky.light", "lens"},
    {"dof", "taa.history depth", "dof"},
    {"motion_blur.tiles", "motion", "motion_blur.near"},
    {"post", "taa.history bloom lens dof motion depth motion_blur.near sky.light", "final"},
    {"ui", "final", "final"},
};
// What is made at load rather than by a pass (the instances' meshes and streams: the figures'
// placements and palettes are written before the frame is encoded, their vertices by the skinning
// pass), and what is the last frame's (the TAA history, the depth pyramid the early cull reads,
// the cloud's half-size picture the spread reprojects, the fog's lit froxels it blends with, the
// scene the wet ground mirrors).
constexpr std::string_view kLoadInputs =
    "terrain.height terrain.pyramid terrain.albedo snow.ground instances taa.history hiz.last "
    "clouds.history fog.history mirror.last";

constexpr int kPasses = static_cast<int>(std::size(kPassTable));

// The lamps' tiles each casting family carries slots for (none without the lamps' shadows).
int lamp_slots(const GameFrame& f) {
  return f.options.lights && f.options.light_shadows
             ? static_cast<int>(std::min(f.options.lamp_tiles, r::kMaxLampTiles))
             : 0;
}

// `lamps`' posts stood as `family` (dark: `draw` lights their glass), casting with the forest.
void stand_posts(ge::gpu::Device& device, GameFrame& f, r::InstanceFamily& family,
                 std::string_view label, std::span<const ge::mountain::app::Lamp> lamps) {
  if (lamps.empty()) return;
  r::create_family(device, f.instancing, family, label, ge::mountain::app::lamp_post_meshes(),
                   ge::mountain::app::lamp_post_surfaces(), static_cast<uint32_t>(lamps.size()));
  r::create_family_casters(device, f.instancing, family, r::kCascades);
  if (f.hiz.view) r::bind_family_hiz(device, f.instancing, family, f.hiz);
  std::vector<r::Instance> stood;
  stood.reserve(lamps.size());
  for (const ge::mountain::app::Lamp& l : lamps)
    stood.push_back(ge::mountain::app::lamp_instance(l, f.lamps_glow));
  r::set_instances(device, family, 0, stood);
  f.families.push_back(&family);
  f.still.push_back(&family);
}

// The targets, the pyramid, the histories and the post pass's groups for outputs of w x h.
void make_targets(ge::gpu::Device& device, GameFrame& f, uint32_t w, uint32_t h) {
  f.output_width = w;
  f.output_height = h;
  const double scale = f.options.taa ? f.options.scale : 1.0;
  f.targets = r::create_targets(device, "frame", r::taa_render_size(w, scale),
                                r::taa_render_size(h, scale));
  r::create_hiz_targets(device, f.hiz, f.targets);
  if (f.options.mirror) r::create_mirror_targets(device, f.mirror, f.targets, f.uniforms);
  // The ambient occlusion's target, bound in the frame group (the blank with it off).
  const bool ao = r::ao_on(f.options.ao);
  if (ao) {
    r::create_ao_targets(device, f.ao, f.targets.width, f.targets.height, f.uniforms,
                         f.targets.depth_view);
  }
  f.frame_group =
      r::frame_group(device, f.frame_layout, f.uniforms, f.sky, f.lights.buffer, f.fog.volume_view,
                     ao ? f.ao.view : nullptr, f.lamp_shadows.atlas_view, f.mirror.last_view);
  for (r::InstanceFamily* family : f.families)
    r::bind_family_hiz(device, f.instancing, *family, f.hiz);
  r::bind_particle_targets(device, f.particles, f.targets);
  r::bind_flame_targets(device, f.flames, f.targets);
  r::bind_precip_targets(device, f.precip, f.targets);
  r::bind_cloud_targets(device, f.clouds, f.targets);
  r::create_water_targets(device, f.water, f.targets);
  r::bind_underwater(device, f.under, f.water);
  // The post pass (and the bloom) read TAA's two histories, or the HDR target for both.
  wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  if (f.options.taa) {
    r::create_taa_targets(device, f.taa, f.targets, w, h);
    inputs[0] = f.taa.history_views[0];
    inputs[1] = f.taa.history_views[1];
  }
  const bool glow = f.options.bloom > 0;
  if (glow) r::create_bloom_targets(device, f.bloom, w, h, inputs);
  const bool lens = r::lens_on(f.options.lens);
  if (lens) r::create_lens_targets(device, f.lens, w, h, inputs, f.targets.depth_view);
  r::PostReads reads;
  if (f.options.look.motion_blur > 0) {
    r::create_motion_blur_targets(device, f.blur, f.targets.width, f.targets.height,
                                  f.targets.motion_view);
    reads = {f.targets.motion_view, f.targets.depth_view, f.blur.near_view, nullptr};
  }
  if (r::dof_on(f.options.dof)) {
    r::create_dof_targets(device, f.dof, w, h, inputs, f.targets.depth_view);
    reads.depth = f.targets.depth_view;
    reads.dof = f.dof.view;
  }
  for (int k = 0; k < 2; ++k) {
    f.post_groups[k] = r::post_group(device, f.post, inputs[k], glow ? f.bloom.view : nullptr,
                                     lens ? f.lens.view : nullptr, reads);
  }
  f.history.valid = false;
}

// THE COLD ON THE VILLAGE (mountain/air/marks.hpp): the night's frost on what stood out in it and
// a freezing rain's glaze on its streets at the sky's solar `hour`, worked out again every 30 s
// of either clock and written into the families' materials when a step is crossed.
void chill_village(ge::gpu::Device& device, GameFrame& f, double t, double hour) {
  namespace ma = ge::mountain::app;
  namespace air = ge::mountain::air;
  if (std::fabs(t - f.cold_t) < 30 && std::fabs(hour - f.cold_hour) < 30.0 / 3600) return;
  f.cold_t = t;
  f.cold_hour = hour;
  const sim::Village& v = *f.shown_village;
  const double x = 0.5 * (v.box[0] + v.box[2]), z = 0.5 * (v.box[1] + v.box[3]);
  const ge::wind::Site site{.y = f.level->ground_at(x, z), .above = 1.5};
  const ma::VillageCold cold =
      ma::cold_steps({air::frost_cover(air::frost_on(*f.level, site, hour)),
                      air::glaze_at(*f.level, site, t, hour)});
  if (cold.frost == f.village_frost && cold.glaze == f.village_glaze) return;
  f.village_frost = cold.frost;
  f.village_glaze = cold.glaze;
  // The traffic was driven: its glass scraped, its engine warm.
  const std::vector<r::Surface> still = ma::village_surfaces(cold);
  if (f.streets.capacity > 0) r::set_family_surfaces(device, f.streets, still);
  if (f.parked.capacity > 0) r::set_family_surfaces(device, f.parked, still);
  if (f.traffic.capacity > 0)
    r::set_family_surfaces(device, f.traffic, ma::village_surfaces({0, cold.glaze}));
}

}  // namespace

std::string pass_table_faults() { return r::check_pass_table(kPassTable, kLoadInputs); }

std::unique_ptr<GameFrame> create_frame(ge::gpu::Device& device,
                                        const ge::mountain::mapgen::Level& level, uint32_t width,
                                        uint32_t height, wgpu::TextureFormat output,
                                        const FrameOptions& options) {
  const std::string faults = pass_table_faults();
  if (!faults.empty()) {
    ge::core::output::error(faults);
    return nullptr;
  }
  const ge::assets::LoadResult& skier = skier_model();
  if (!skier.ok) {
    ge::core::output::error(
        "fall_line: the skier model compiled into the game does not load: " + skier.error +
        "; rebuild it (`ge asset build skier`) and commit the .glb (skill: assets)");
    return nullptr;
  }
  auto f = std::make_unique<GameFrame>();
  f->options = options;
  f->skier = &skier.model;
  f->binding = ge::skier::bind_rig(skier.model);
  if (!f->binding.complete) {
    ge::core::output::error(
        "fall_line: the skier model lacks a rig joint (skier/app/rig.hpp's names); rebuild it "
        "(`ge asset build skier`) and check its builder's joint names (skill: assets)");
    return nullptr;
  }
  f->uniforms = r::make_buffer(device, "frame.uniforms", sizeof(r::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  f->frame_layout = r::frame_layout(device);
  r::create_sky(device, f->pipelines, f->sky, f->uniforms, f->frame_layout);
  f->albedo = bake_albedo(level);
  r::TerrainSource source;
  source.height = &level.ground;
  source.albedo = &f->albedo;
  source.base_spacing = kGroundSpacing;
  source.levels = kGroundLevels;
  source.overdraw_probe = options.overdraw_probe;
  source.wire_probe = options.wire_probe;
  f->tracks = options.snow && options.trails;
  if (f->tracks) {
    ge::mountain::app::create_trails(device, f->pipelines, f->trails, level, options.trail_maps);
  }
  if (options.snow) {
    ge::mountain::app::create_snow(device, f->snow, level, {}, f->tracks ? &f->trails : nullptr);
    source.shader = ge::mountain::app::snow_shader();
    source.shader_label = "snow";
    source.material = f->snow.layout;
  }
  r::create_terrain(device, f->pipelines, f->terrain, source, f->frame_layout);
  r::create_particles(device, f->pipelines, f->particles, f->frame_layout, f->terrain,
                      options.particles);
  r::create_precip(device, f->pipelines, f->precip, f->frame_layout, f->terrain, options.fall);
  r::create_bolts(device, f->pipelines, f->bolts, f->frame_layout);
  r::create_flames(device, f->pipelines, f->flames, f->frame_layout);
  r::create_clouds(device, f->pipelines, f->clouds, f->frame_layout);
  r::create_water(device, f->pipelines, f->water, f->frame_layout);
  r::create_underwater(device, f->pipelines, f->under, f->frame_layout);
  r::create_fog(device, f->pipelines, f->fog, f->frame_layout, &f->terrain, options.froxels);
  r::create_hiz_pipelines(device, f->pipelines, f->hiz);
  if (options.mirror) r::create_mirror(device, f->pipelines, f->mirror);
  r::create_instance_pipelines(device, f->pipelines, f->instancing, f->frame_layout,
                               options.overdraw_probe, options.wire_probe);
  if (options.taa) {
    r::TaaSettings taa;
    taa.scale = options.scale;
    r::create_taa(device, f->pipelines, f->taa, f->frame_layout, taa);
  }
  if (options.bloom > 0) {
    r::BloomSettings bloom;
    bloom.strength = options.bloom;
    r::create_bloom_pipelines(device, f->pipelines, f->bloom, bloom);
  }
  if (r::lens_on(options.lens)) {
    r::create_lens_pipelines(device, f->pipelines, f->lens, f->frame_layout, options.lens);
  }
  if (options.look.motion_blur > 0) r::create_motion_blur_pipelines(device, f->pipelines, f->blur);
  if (r::dof_on(options.dof)) {
    r::create_dof_pipelines(device, f->pipelines, f->dof, f->frame_layout);
  }
  if (r::ao_on(options.ao)) {
    r::create_ao_pipelines(device, f->pipelines, f->ao);
    r::set_ao(device, f->ao, options.ao);
  }
  r::PostSettings post;
  post.bloom = options.bloom;
  post.look = options.look;
  post.lens = r::lens_on(options.lens);
  post.dof = r::dof_on(options.dof);
  r::create_post(device, f->pipelines, f->post, f->frame_layout, output, post);
  std::vector<float> grade = bake_grade();
  r::apply_grade_knobs(grade, options.grade);
  r::set_grade(device, f->post, grade);
  r::create_skin_pipelines(device, f->pipelines, f->skinning);
  r::create_ui_pass(device, f->pipelines, f->ui, output);
  if (options.lights && options.light_shadows) {
    r::LampShadowSettings lamp;
    lamp.tiles = std::min(options.lamp_tiles, r::kMaxLampTiles);
    r::create_lamp_shadows(device, f->pipelines, f->lamp_shadows, lamp, f->frame_layout, f->sky);
  }
  if (!f->pipelines.finish(device)) return nullptr;
  ge::gpu::end_load(device);
  if (options.lights) r::create_lights(device, f->lights);
  f->frame_group = r::frame_group(device, f->frame_layout, f->uniforms, f->sky, f->lights.buffer,
                                  f->fog.volume_view, nullptr, f->lamp_shadows.atlas_view);
  f->level = &level;
  const std::vector<r::Instance> trees = tree_instances(level);
  r::create_family(device, f->instancing, f->forest, "forest", forest_meshes(), forest_materials(),
                   static_cast<uint32_t>(std::max<size_t>(trees.size(), 1)));
  r::set_instances(device, f->forest, 0, trees);
  const ge::assets::Model& model = *f->skier;
  r::create_family(device, f->instancing, f->figures, "figures", figure_meshes(model),
                   figure_materials(model), kMaxFigures);
  f->materials_per_racer = static_cast<uint32_t>(model.materials.size());
  const auto joints = static_cast<uint32_t>(model.joints.size());
  for (int k = 0; k < kMaxFigures; ++k) {
    r::create_skin(device, f->skinning, f->skins[k], f->figures, static_cast<uint32_t>(k), 0,
                   model.meshes[skier_lods(model)[0]], ge::skier::pose_reach(), joints);
    f->skin_list[k] = &f->skins[k];
  }
  f->locals.resize(joints);
  f->globals.resize(joints);
  f->palette.resize(joints);
  f->staged_figures.resize(kMaxFigures);
  r::create_family(device, f->instancing, f->crowd, "crowd", crowd_meshes(model),
                   crowd_materials(model), kMaxCrowd, joints);
  f->instance_light = r::instance_light_group(device, f->instancing, f->terrain);
  // The forest casts into the cascades (two caster slots), the figures and the crowd into the
  // atlas (a slot a tile).
  r::ShadowSettings shadows;
  shadows.size = options.shadow_size;
  r::create_sun_shadows(device, f->shadows, shadows, f->frame_layout, f->sky);
  // With the lamps' shadows, each casts into the lamps' tiles too (a slot a tile after those).
  const int lamp_tiles = lamp_slots(*f);
  r::create_family_casters(device, f->instancing, f->forest, r::kCascades, lamp_tiles);
  r::create_family_casters(device, f->instancing, f->figures, r::kMaxShadowFigures, lamp_tiles);
  r::create_family_casters(device, f->instancing, f->crowd, r::kMaxShadowFigures, lamp_tiles);
  if (lamp_tiles > 0) f->lamp_casters = {&f->forest, &f->figures, &f->crowd};
  // The piste's floodlight masts (mountain/app/night_lamps.hpp), standing on every map.
  f->piste_lamps = ge::mountain::app::piste_lamps(level);
  stand_posts(device, *f, f->masts, "lamps.masts", f->piste_lamps);
  r::bind_terrain_shadows(f->terrain, f->shadows.group);
  if (options.snow) r::bind_terrain_material(f->terrain, f->snow.group);
  r::bind_instance_shadows(f->instancing, f->shadows.group);
  r::bind_particle_shadows(f->particles, f->shadows.group);
  r::bind_fog_shadows(f->fog, f->shadows.group);
  make_targets(device, *f, width, height);
  show_lakes(device, *f);  // the lakes a game laid on the map (lakes.hpp)
  f->recorder.init(device, "frame.timer", kPasses);
  return f;
}

int add_water(ge::gpu::Device& device, GameFrame& f, const ge::water::Body& body,
              const r::WaterDraw& draw) {
  return r::add_water_body(device, f.water, body, draw);
}

void set_water(ge::gpu::Device& device, GameFrame& f, int index, const ge::water::Body& body) {
  r::set_water_body(device, f.water, index, body);
}

void freeze_water(ge::gpu::Device& device, GameFrame& f, int index, const ge::water::Body& body,
                  double h, bool swept_only, double moat, double thaw) {
  namespace ma = ge::mountain::app;
  if (index < 0 || static_cast<size_t>(index) >= f.water.bodies.size()) return;
  const bool frozen = h >= ma::kIceShows;
  // A moat of open water round thawing ice: the water stays drawn, the sheet over it.
  f.water.bodies[static_cast<size_t>(index)].hidden = frozen && moat <= 0;
  const auto made = std::find(f.ice_bodies.begin(), f.ice_bodies.end(), index);
  size_t k = static_cast<size_t>(made - f.ice_bodies.begin());
  if (made == f.ice_bodies.end()) {
    if (!frozen) return;
    r::MeshSource sheet;
    sheet.lods.push_back({ma::ice_mesh(body.shore, body.level,
                                       ge::core::num::to_uint32(f.level->seed), swept_only, moat),
                          0.0f});
    r::InstanceFamily& ice = f.ices.emplace_back();
    r::create_family(device, f.instancing, ice, "water.ice",
                     std::span<const r::MeshSource>(&sheet, 1), ma::ice_surfaces(h, thaw), 1);
    r::bind_family_hiz(device, f.instancing, ice, f.hiz);
    f.families.push_back(&ice);
    f.ice_bodies.push_back(index);
    k = f.ice_bodies.size() - 1;
  }
  r::InstanceFamily& ice = f.ices[k];
  r::set_family_surfaces(device, ice, ma::ice_surfaces(h, thaw));
  r::Instance at;
  if (!frozen) at.scale[0] = at.scale[1] = at.scale[2] = 0;
  r::set_instances(device, ice, 0, std::span<const r::Instance>(&at, 1));
}

void show_village(ge::gpu::Device& device, GameFrame& f,
                  std::shared_ptr<const sim::Village> shown) {
  namespace ma = ge::mountain::app;
  namespace vg = ge::mountain::village;
  if (f.shown_village != nullptr || shown == nullptr) return;
  f.shown_village = std::move(shown);
  const sim::Village& village = *f.shown_village;
  // The forest without the felled trees: their instances dropped, the count cut to the rest.
  const std::vector<r::Instance> trees = tree_instances(*f.level, village.felled);
  r::set_instances(device, f.forest, 0, trees);
  r::set_instance_count(f.forest, static_cast<uint32_t>(trees.size()));
  // The cascades drew the trees that stood: drawn again, without the felled ones' shadows.
  r::reset_sun_shadows(f.shadows);
  // Four families: the streets (one instance, casting nothing: a sheet on the snow), the buildings
  // and the parked cars (still: cast into the cascades) and the traffic (moving: cast into the
  // atlas's tiles).
  r::MeshSource sheet;
  sheet.lods.push_back({ma::street_mesh(*f.level, village.ground), 0.0f});
  if (sheet.lods.front().mesh.vertex_count() > 0) {
    r::create_family(device, f.instancing, f.streets, "village.streets",
                     std::span<const r::MeshSource>(&sheet, 1), ma::village_surfaces({}), 1);
    r::bind_family_hiz(device, f.instancing, f.streets, f.hiz);
    r::Instance at;
    at.position[0] = village.ground.x0;
    at.position[2] = village.ground.z0;
    r::set_instances(device, f.streets, 0, std::span<const r::Instance>(&at, 1));
    f.families.push_back(&f.streets);
  }
  if (!village.buildings.empty()) {
    // Each building and its plinth: still, casting into the cascades.
    std::vector<r::Instance> stood;
    stood.reserve(village.buildings.size() * 2);
    for (const vg::Building& b : village.buildings) ma::building_instances(b, stood);
    r::create_family(device, f.instancing, f.buildings, "village.buildings", ma::building_meshes(),
                     ma::building_surfaces(), static_cast<uint32_t>(stood.size()));
    r::create_family_casters(device, f.instancing, f.buildings, r::kCascades, lamp_slots(f));
    if (lamp_slots(f) > 0) f.lamp_casters.push_back(&f.buildings);
    r::bind_family_hiz(device, f.instancing, f.buildings, f.hiz);
    r::set_instances(device, f.buildings, 0, stood);
    f.buildings_lit.assign(village.buildings.size(), 0);
    f.families.push_back(&f.buildings);
    f.still.push_back(&f.buildings);
  }
  // The street lamps along its streets.
  f.street_lamps = ma::street_lamps(*f.level, village.streets);
  stand_posts(device, f, f.posts, "lamps.posts", f.street_lamps);
  if (!village.traffic) return;
  const vg::TrafficPlan& plan = *village.traffic;
  if (!plan.parked.empty()) {
    r::create_family(device, f.instancing, f.parked, "village.parked", ma::vehicle_meshes(),
                     ma::village_surfaces({}), static_cast<uint32_t>(plan.parked.size()));
    r::create_family_casters(device, f.instancing, f.parked, r::kCascades, lamp_slots(f));
    if (lamp_slots(f) > 0) f.lamp_casters.push_back(&f.parked);
    r::bind_family_hiz(device, f.instancing, f.parked, f.hiz);
    std::vector<r::Instance> cars;
    cars.reserve(plan.parked.size());
    for (const vg::Parked& car : plan.parked) cars.push_back(ma::parked_instance(car));
    r::set_instances(device, f.parked, 0, cars);
    f.families.push_back(&f.parked);
    f.still.push_back(&f.parked);
  }
  if (!plan.vehicles.empty()) {
    r::create_family(device, f.instancing, f.traffic, "village.traffic", ma::vehicle_meshes(),
                     ma::village_surfaces({}), static_cast<uint32_t>(plan.vehicles.size()));
    r::create_family_casters(device, f.instancing, f.traffic, r::kMaxShadowFigures, lamp_slots(f));
    if (lamp_slots(f) > 0) f.lamp_casters.push_back(&f.traffic);
    r::bind_family_hiz(device, f.instancing, f.traffic, f.hiz);
    f.staged_traffic.assign(plan.vehicles.size(), r::Instance{});
    f.families.push_back(&f.traffic);
    f.moving.push_back(&f.traffic);
  }
}

void resize(ge::gpu::Device& device, GameFrame& f, uint32_t width, uint32_t height) {
  make_targets(device, f, width, height);
}

void cut(GameFrame& f) {
  f.history.valid = false;
  r::reset_taa(f.taa);
  // The cascades were centred on the last camera: drawn again round the new one.
  r::reset_sun_shadows(f.shadows);
  r::cut_family(f.figures);
  r::cut_family(f.crowd);
  if (!f.staged_traffic.empty()) r::cut_family(f.traffic);
  for (ge::skier::RigPoser& p : f.posers) p = {};
  for (ge::skier::RigPoser& p : f.crowd_posers) p = {};
  for (r::Skin& s : f.skins) r::settle_skin(s);
}

void track(GameFrame& f, const FrameScene& scene) {
  if (!f.tracks) return;
  if (f.pens.size() < scene.skiers.size()) f.pens.resize(scene.skiers.size());
  f.laid.clear();
  for (size_t k = 0; k < scene.skiers.size(); ++k) {
    ge::skier::SkiPrint prints[2]{};
    const int n =
        ge::skier::ski_prints(scene.skiers[k], ge::skier::kReferenceSkis, f.pens[k], prints);
    for (int i = 0; i < n; ++i) {
      const ge::skier::SkiPrint& p = prints[i];
      f.laid.push_back({static_cast<float>(p.ax), static_cast<float>(p.az),
                        static_cast<float>(p.bx), static_cast<float>(p.bz),
                        static_cast<float>(p.half_width), static_cast<float>(p.depth),
                        static_cast<float>(p.berm), static_cast<float>(p.wall)});
    }
  }
  ge::mountain::app::add_trail_stamps(f.trails, f.laid);
}

void forget_tracks(GameFrame& f) {
  f.spray_pens.clear();
  f.spray_placed = false;
  f.spray_forget = true;
  if (!f.tracks) return;
  ge::mountain::app::clear_trails(f.trails);
  f.pens.clear();
}

namespace {

// Every amateur posed and placed: his poser's lags run on over the scene's `dt` (settled: none),
// his palette into the crowd's rows at his id, his instance at his id.
void pose_crowd(ge::gpu::Device& device, GameFrame& f, const FrameScene& scene) {
  const size_t n = std::min<size_t>(scene.crowd.size(), kMaxCrowd);
  const size_t joints = f.skier->joints.size();
  if (f.crowd_posers.size() < n || scene.settle) f.crowd_posers.assign(n, {});
  f.crowd_palettes.resize(n * joints);
  f.staged_crowd.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const sim::Amateur& a = scene.crowd[i];
    f.staged_crowd[i] = crowd_instance(a, *f.level, f.materials_per_racer);
    if (!crowd_drawn(a)) {
      f.crowd_posers[i] = {};
      continue;
    }
    f.crowd_posers[i].pose(crowd_skier(a), scene.dt, f.rig_pose);
    ge::skier::rig_locals(*f.skier, f.binding, f.rig_pose, f.locals);
    ge::assets::pose_globals(*f.skier, f.locals, f.globals);
    ge::assets::skin_palette(*f.skier, f.globals,
                             std::span<ge::assets::Affine>(&f.crowd_palettes[i * joints], joints));
  }
  if (scene.settle) r::cut_family(f.crowd);
  r::set_instances(device, f.crowd, 0, f.staged_crowd);
  r::set_instance_count(f.crowd, static_cast<uint32_t>(n));
  r::set_palettes(device, f.crowd, f.crowd_palettes);
}

// The map's thunderstorm, laid again only when the map or its strength changed; null without one.
const ge::wind::Storm* storm_over(GameFrame& f) {
  const ge::mountain::mapgen::Level& l = *f.level;
  if (f.storm_level != &l || f.storm_thunder != l.weather.thunder) {
    f.storm = ge::mountain::air::storm_of(l);
    f.storm_level = &l;
    f.storm_thunder = l.weather.thunder;
    f.lightning.kept.clear();
  }
  return f.storm.cells > 0 ? &f.storm : nullptr;
}

}  // namespace

void draw(ge::gpu::Device& device, GameFrame& f, const r::FrameView& seen, const FrameScene& scene,
          const wgpu::TextureView& output) {
  GE_ZONE("frame");
  // The map's sky at the view's moment: the cloud, the deck and the fog it is seen under.
  r::FrameView view = seen;
  view.sun_size = f.options.sun_size;
  view.moon_size = f.options.moon_size;
  const ge::wind::Storm* storm = f.options.weather ? storm_over(f) : nullptr;
  const ge::wind::Storm none;
  const ge::wind::Storm& storm_air = storm != nullptr ? *storm : none;
  // The sky's solar hour: the snow line and the breath read the day's air at it.
  const double sky_hour =
      scene.hour >= 0 ? scene.hour : ge::mountain::air::hour_at(*f.level, view.time_s);
  if (f.options.weather) {
    // How far dusk has fallen under the view's sun (the evening's jet and its scud; with the moon
    // the key light, Night's sun), and the height and the speed picked by hand.
    const double sun = view.night.moon_key ? view.night.sun_elevation : view.sun_elevation;
    const ge::mountain::app::Aloft aloft{.dusk = ge::mountain::app::dusk_of(sun),
                                         .speed = f.options.cloud_speed,
                                         .base = f.options.cloud_height,
                                         .thickness = f.options.cloud_thickness};
    ge::mountain::app::sky_at(*f.level, view.time_s, view.weather, view.clouds,
                              f.options.clouds > 0, view.eye[1], aloft);
    // A thunderstorm's towers in place of the map's own cloud, and the sky under them.
    if (storm != nullptr) {
      ge::mountain::app::storm_sky(*f.level, *storm, view.time_s, view.eye, f.options.clouds > 0,
                                   aloft, view.weather, view.clouds);
    }
    // Kinds of cloud picked by hand (sv_clouds) in place of the map's, dealt off its seed.
    if (f.options.cloud_count >= 0 && f.options.clouds > 0) {
      const ge::mountain::mapgen::Level& l = *f.level;
      view.clouds = ge::mountain::app::cloud_layers_of(f.options.cloud_kinds, f.options.cloud_count,
                                                       l.weather, l.seed, l.mountain.base.y,
                                                       l.sun.latitude, view.time_s, aloft);
      view.weather = ge::mountain::app::weather_look(
          l.weather, ge::mountain::air::fall_at(l, view.time_s, view.eye[1], sky_hour),
          view.clouds.count > 0);
      ge::mountain::app::lid_under(f.options.cloud_kinds, f.options.cloud_count, view.weather);
    }
    // A cover picked by hand: the lowest layer's, the layers over it in the same proportion.
    if (f.options.cover >= 0 && view.clouds.count > 0 && view.clouds.layers[0].cover > 0) {
      const double k = f.options.cover / view.clouds.layers[0].cover;
      for (int i = 0; i < view.clouds.count; ++i)
        view.clouds.layers[i].cover = std::min(1.0, view.clouds.layers[i].cover * k);
    }
    // A thickness picked by hand: every layer's, between its kind's thin patches and its cores.
    if (f.options.thick >= 0) {
      for (int i = 0; i < view.clouds.count; ++i)
        view.clouds.layers[i].thick_bias = 2 * std::min(f.options.thick, 1.0) - 1;
    }
    view.clouds.steps = f.options.clouds;
    view.clouds.quarter = f.options.cloud_quarter;
    view.clouds.spread = f.options.cloud_spread;
  }
  f.drawn_clouds = view.clouds;
  // THE WET GROUND the rain leaves (the eye's own: one wet ground a frame), stepped by the frames'
  // clock from the steady state its rain and drying would leave, before the uniforms take it.
  if (f.options.wet && f.options.weather) {
    const ge::mountain::air::Fall fall = ge::mountain::air::fall_at(
        *f.level, storm_air, view.time_s, view.eye[0], view.eye[1], view.eye[2], sky_hour);
    const ge::wind::Precip here{fall.fall, fall.rain};
    const double rate = ge::wind::rate_of(here) * here.rain;
    const double sun = view.weather.sun_pass * std::max(0.0, std::sin(view.sun_elevation));
    const double dry = ge::wind::dry_rate(
        ge::mountain::air::wind_at(*f.level, storm_air, view.time_s, view.eye[0], view.eye[2])
            .speed,
        sun);
    const double since = view.time_s - f.wet_time;
    f.wet = f.wet_placed && since >= 0 && since < 60 ? ge::wind::step_wet(f.wet, rate, dry, since)
                                                     : ge::wind::wet_steady(rate, dry);
    f.wet_time = view.time_s;
    f.wet_placed = true;
    view.weather.wet = f.wet.wet;
    view.weather.puddles = f.wet.puddles;
    view.weather.rain = rate;  // the lakes' rings
  }
  // THE STORM'S LIGHTNING since the last drawn frame (a sixtieth on the first, or after the clock
  // went back): its lamps, the sky's flash and the strikes' channels, unless the caller lit it.
  std::span<const r::BoltSegment> bolts = scene.bolts;
  if (storm != nullptr && f.options.lightning && view.flashes.count == 0 && scene.bolts.empty()) {
    GE_ZONE("frame.lightning");
    const double since = view.time_s - f.lightning_time;
    const double window = f.lightning_placed && since > 0 && since < 0.25 ? since : 1.0 / 60;
    const double eye[3] = {view.eye[0], view.eye[1], view.eye[2]};
    ge::mountain::app::storm_lights(f.lightning, *storm, eye, view.time_s - window, view.time_s);
    view.flashes = f.lightning.lamps;
    view.weather.flash = f.lightning.sky;
    bolts = f.lightning.bolts;
  }
  f.lightning_time = view.time_s;
  f.lightning_placed = true;
  view.reach_m = f.options.reach_m;
  const auto start = std::chrono::steady_clock::now();
  f.recorder.begin_frame(device);
  // Prepare: every decision of the frame, and the uploads.
  // The last frame's pyramid culls only when there is a last frame of this camera.
  const bool occlusion = f.options.occlusion && f.history.valid;
  double jitter[2] = {0, 0};
  if (f.options.taa) r::taa_jitter(view.frame, r::taa_phases(f.options.scale), jitter);
  const bool fresh = !f.history.valid;
  // The racers posed first: their headlamps are where their heads are.
  const size_t racers = std::min<size_t>(scene.skiers.size(), kMaxFigures);
  {
    GE_ZONE("frame.figures");
    for (size_t k = 0; k < racers; ++k) {
      // Settled: a fresh poser keeps no lag of the last frame, and the skin no motion.
      if (scene.settle) {
        f.posers[k] = {};
        r::settle_skin(f.skins[k]);
      }
      f.posers[k].pose(scene.skiers[k], scene.dt, f.rig_pose);
      ge::skier::rig_locals(*f.skier, f.binding, f.rig_pose, f.locals);
      ge::assets::pose_globals(*f.skier, f.locals, f.globals);
      ge::assets::skin_palette(*f.skier, f.globals, f.palette);
      r::set_palette(device, f.skins[k], f.palette);
      f.staged_figures[k] =
          figure_instance(scene.skiers[k], *f.level, static_cast<int>(k), f.materials_per_racer);
      // His headlamp where the posed head is.
      f.headlamps[k] = headlamp_of(
          ge::skier::figure_frame(scene.skiers[k], ge::skier::World{f.level, nullptr}), f.rig_pose);
    }
    // A slot drawn again after a gap starts settled.
    for (size_t k = racers; k < f.skins.size(); ++k) r::settle_skin(f.skins[k]);
    r::set_instances(device, f.figures, 0,
                     std::span<const r::Instance>(f.staged_figures.data(), racers));
    r::set_instance_count(f.figures, static_cast<uint32_t>(racers));
  }
  // THE LAMPS, on with the dark: the windows lit at the sky's hour (their glass written again only
  // when one building's changes), this frame's lights (app/lamps.hpp) and the eye's light by them.
  const double sun = view.night.moon_key ? view.night.sun_elevation : view.sun_elevation;
  const double lit = ge::mountain::app::lamps_lit(sun);
  if (f.shown_village != nullptr && !f.buildings_lit.empty()) {
    GE_ZONE("frame.windows");
    const double hour = scene.hour < 0 ? 20.0 : scene.hour;
    const std::vector<ge::mountain::village::Building>& buildings = f.shown_village->buildings;
    std::vector<r::Instance> stood;
    for (size_t k = 0; k < buildings.size(); ++k) {
      const auto on = static_cast<uint8_t>(ge::mountain::app::lights_on(buildings[k], hour, sun));
      if (on == f.buildings_lit[k]) continue;
      f.buildings_lit[k] = on;
      stood.clear();
      ge::mountain::app::building_instances(buildings[k], stood, on != 0);
      r::set_instances(device, f.buildings, static_cast<uint32_t>(k * 2), stood);
    }
  }
  if (f.shown_village != nullptr && f.options.cold) chill_village(device, f, view.time_s, sky_hour);
  dress_mists(*f.level, f.options.fog, scene, view, f.mists);  // the valley's sea in the uniforms
  if (f.options.lights) {
    GE_ZONE("frame.lamps");
    namespace ma = ge::mountain::app;
    const bool shadows = f.options.light_shadows;
    f.lamp_list.clear();
    for (size_t k = 0; k < racers; ++k) {
      headlamp_lights(f.headlamps[k], kHeadlampKeys + static_cast<uint32_t>(k), lit, shadows,
                      f.lamp_list);
    }
    ma::lamp_lights(f.piste_lamps, lit, shadows, ma::kFloodKeys, f.lamp_list);
    ma::lamp_lights(f.street_lamps, lit, shadows, ma::kStreetKeys, f.lamp_list);
    if (f.shown_village != nullptr) {
      ma::window_lights(f.shown_village->buildings, f.buildings_lit, f.lamp_list);
    }
    glow_lights(*f.level, f.mists, scene.flames, f.lamp_list);
    view.lamp_lux = eye_lamp_lux(f.lamp_list, view, *f.level);
    f.lamp_lux = view.lamp_lux;
  }
  // The posts' glass, lit or dark with the lamps.
  if (const bool glow = lit > 0.5; glow != f.lamps_glow) {
    f.lamps_glow = glow;
    std::vector<r::Instance> stood;
    for (auto [family, lamps] :
         {std::pair{&f.masts, &f.piste_lamps}, {&f.posts, &f.street_lamps}}) {
      if (lamps->empty()) continue;
      stood.clear();
      for (const ge::mountain::app::Lamp& l : *lamps)
        stood.push_back(ge::mountain::app::lamp_instance(l, glow));
      r::set_instances(device, *family, 0, stood);
    }
  }
  r::fill_uniforms(f.staged, view, f.targets.width, f.targets.height, &f.history,
                   f.options.taa ? jitter : nullptr);
  {
    GE_ZONE("frame.fog");
    r::prepare_fog(device, f.fog, view, f.mists, fresh);
    r::fog_uniforms(f.fog, f.staged);
  }
  device.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  {
    GE_ZONE("frame.sky");
    r::prepare_sky(f.sky, view);
  }
  {
    GE_ZONE("frame.terrain");
    r::prepare_terrain(device, f.terrain, view);
  }
  {
    GE_ZONE("frame.water");
    r::prepare_water(device, f.water, view);
    r::prepare_underwater(device, f.under, f.water, view, f.targets.height);
  }
  if (f.tracks) {
    GE_ZONE("frame.trails");
    track(f, scene);
    // The fine window is kept round the ground a quarter of its span ahead of the eye, where the
    // camera looks.
    const double ahead = f.options.trail_maps.fine_span / 4;
    ge::mountain::app::prepare_trails(device, f.pipelines, f.trails,
                                      view.eye[0] + ahead * std::sin(view.heading),
                                      view.eye[2] + ahead * std::cos(view.heading));
  }
  {
    GE_ZONE("frame.spray");
    if (f.spray_forget) {
      r::clear_particles(device, f.particles);
      f.spray_forget = false;
    }
    // The time since the last frame: none on the first, nor when the clock went back (a replay
    // rewound), nor on a still drawn again at the same time.
    const double dt = f.spray_placed ? std::max(view.time_s - f.spray_time, 0.0) : 0.0;
    f.spray_time = view.time_s;
    f.spray_placed = true;
    throw_spray(f, scene, dt);
    breathe(f, scene, sky_hour, dt);
    emit_areas(f, scene, view.time_s - dt, view.time_s);
    // The map's air at this moment: the wind carries the spray and the flakes.
    const ge::mountain::air::Wind wind =
        ge::mountain::air::wind_at(*f.level, storm_air, view.time_s, view.eye[0], view.eye[2]);
    const double carried[3] = {wind.x, 0.0, wind.z};
    r::prepare_particles(device, f.particles, dt, view.near_m, carried);
    r::prepare_flames(device, f.flames, scene.flames, view);
    // What falls at the eye: snow above the map's snow line, rain under it, sleet between, and a
    // storm's rain.
    const ge::mountain::air::Fall fall = ge::mountain::air::fall_at(
        *f.level, storm_air, view.time_s, view.eye[0], view.eye[1], view.eye[2], sky_hour);
    r::PrecipFrame at;
    for (int k = 0; k < 3; ++k) {
      at.eye[k] = view.eye[k];
      at.wind[k] = carried[k];
    }
    at.fall = f.options.precip ? fall.fall : 0.0;
    at.rain = fall.rain;
    at.drop_speed = ge::wind::fall_speed({fall.fall, 1});
    at.swirl = ge::wind::swirl_of(wind.speed, ge::wind::turbulence_for(ge::wind::kRoughness::snow));
    at.splash = f.options.splashes ? fall.rain * std::sqrt(fall.fall) : 0.0;
    at.dt = dt;
    at.time = view.time_s;
    at.near_m = view.near_m;
    r::prepare_precip(device, f.precip, at);
  }
  r::prepare_bolts(device, f.bolts, bolts, view, f.targets.height);
  if (!scene.crowd.empty()) {
    GE_ZONE("frame.crowd");
    pose_crowd(device, f, scene);
  } else {
    r::set_instance_count(f.crowd, 0);
  }
  const std::optional<ge::mountain::village::TrafficPlan>* traffic =
      f.shown_village != nullptr ? &f.shown_village->traffic : nullptr;
  if (traffic != nullptr && traffic->has_value() && !f.staged_traffic.empty()) {
    GE_ZONE("frame.traffic");
    // The traffic where the run's clock has it.
    const ge::mountain::village::TrafficPlan& plan = traffic->value();
    for (size_t k = 0; k < f.staged_traffic.size(); ++k) {
      const auto id = static_cast<int32_t>(k);
      f.staged_traffic[k] = ge::mountain::app::vehicle_instance(
          ge::mountain::village::vehicle_at(*f.level, plan, id, scene.run_time), id);
    }
    if (scene.settle) r::cut_family(f.traffic);
    r::set_instances(device, f.traffic, 0, f.staged_traffic);
  }
  {
    GE_ZONE("frame.instances");
    r::SeeThrough see;
    if (scene.subject) {
      see.on = true;
      for (int k = 0; k < 3; ++k) see.subject[k] = (*scene.subject)[static_cast<size_t>(k)];
    }
    r::set_see_through(f.forest, see);
    for (r::InstanceFamily* family : f.families) {
      r::prepare_family(device, *family, view, f.targets.height, occlusion);
    }
  }
  // The atlas's tiles: the racers', then the nearest amateurs'. Each tile culls the figures and
  // the crowd by its own box, so an amateur is drawn only into the tiles he can shade.
  shadow_centres(std::span<const ge::skier::SkierState>(scene.skiers.data(), racers), scene.crowd,
                 view.eye, r::kMaxShadowFigures, f.figure_centres);
  {
    GE_ZONE("frame.shadows");
    r::prepare_sun_shadows(device, f.shadows, view, f.figure_centres, f.still, f.moving);
  }
  if (f.options.lights) {
    GE_ZONE("frame.lights");
    r::prepare_lights(device, f.lights, f.lamp_list, view, f.targets.width, f.targets.height);
    if (f.options.light_shadows) {
      // What moves through a lamp's view draws its tiles again: the figures and the traffic.
      f.lamp_movers.clear();
      for (const auto& c : f.figure_centres) f.lamp_movers.push_back({c[0], c[1], c[2], 1.2});
      for (const r::Instance& car : f.staged_traffic) {
        if (car.scale[0] > 0) {
          f.lamp_movers.push_back({car.position[0], car.position[1], car.position[2], 3.0});
        }
      }
      r::prepare_lamp_shadows(device, f.lamp_shadows, f.lights, f.lamp_list, view, f.lamp_movers,
                              f.lamp_casters);
    }
  }
  // Depth of field's focus: the one asked for, or how far the camera's subject is.
  if (r::dof_on(f.options.dof)) {
    r::DofSettings lens = f.options.dof;
    if (lens.focus_m <= 0) {
      lens.focus_m = 20;
      if (scene.subject) {
        const auto& s = *scene.subject;
        const double dx = s[0] - view.eye[0], dy = s[1] - view.eye[1], dz = s[2] - view.eye[2];
        lens.focus_m = std::sqrt(dx * dx + dy * dy + dz * dz);
      }
    }
    r::set_dof(device, f.dof, lens);
    r::set_post_focus(device, f.post, lens);
  }
  // Encode, in the pass table's order (the recorder times each pass's encoding).
  GE_ZONE("frame.encode");
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  r::encode_sky_tables(f.sky, f.pipelines, f.recorder, encoder);
  // Only the racers drawn this frame are posed.
  r::encode_skinning(std::span<r::Skin* const>(f.skin_list.data(), racers), f.skinning, f.pipelines,
                     f.recorder, encoder);
  const std::span<r::InstanceFamily* const> families(f.families);
  r::encode_terrain_cull(f.terrain, f.pipelines, f.recorder, encoder, f.frame_group);
  r::encode_instances_cull(families, f.instancing, f.pipelines, f.recorder, encoder, f.frame_group,
                           false);
  r::encode_terrain_horizon(f.terrain, f.pipelines, f.recorder, encoder);
  if (f.tracks) {
    ge::mountain::app::encode_trails(f.trails, f.pipelines, f.recorder, encoder);
  } else {
    // The pass table's rows run empty, so every later pass keeps its timing row.
    f.recorder.compute(encoder, "trails.fine").End();
    f.recorder.compute(encoder, "trails.coarse").End();
  }
  r::encode_particles_sim(f.particles, f.pipelines, f.recorder, encoder);
  r::encode_precip_sim(f.precip, f.pipelines, f.recorder, encoder);
  r::encode_sun_shadows(f.shadows, f.still, f.moving, f.instancing, f.pipelines, f.recorder,
                        encoder);
  if (f.options.lights && f.options.light_shadows) {
    r::encode_lamp_shadows(f.lamp_shadows, f.lamp_casters, f.instancing, f.pipelines, f.recorder,
                           encoder);
  } else {
    // The pass table's rows run empty, so every later pass keeps its timing row.
    f.recorder.compute(encoder, "lamps.cull").End();
    f.recorder.compute(encoder, "lamps.shadows").End();
  }
  r::encode_fog(f.fog, f.pipelines, f.recorder, encoder, f.frame_group);
  r::encode_terrain_depth(f.terrain, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_instances_depth(families, f.instancing, f.pipelines, f.recorder, encoder, f.targets,
                            f.frame_group, f.instance_light, false);
  r::encode_hiz(f.hiz, f.pipelines, f.recorder, encoder);
  r::encode_instances_cull(families, f.instancing, f.pipelines, f.recorder, encoder, f.frame_group,
                           true);
  r::encode_instances_depth(families, f.instancing, f.pipelines, f.recorder, encoder, f.targets,
                            f.frame_group, f.instance_light, true);
  if (r::ao_on(f.options.ao)) {
    r::encode_ao(f.ao, f.pipelines, f.recorder, encoder);
  } else {
    f.recorder.compute(encoder, "ao").End();
  }
  r::encode_terrain(f.terrain, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_instances(families, f.instancing, f.pipelines, f.recorder, encoder, f.targets,
                      f.frame_group, f.instance_light);
  r::encode_sky(f.sky, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_clouds(f.clouds, f.pipelines, f.recorder, encoder, f.targets, f.frame_group, view,
                   f.staged);
  r::encode_water(f.water, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_underwater(f.under, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_particles(f.particles, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_flames(f.flames, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_precip(f.precip, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_bolts(f.bolts, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  if (f.options.mirror) {
    r::encode_mirror(f.mirror, f.pipelines, f.recorder, encoder);
  } else {
    f.recorder.compute(encoder, "mirror.keep").End();
  }
  if (f.options.taa) {
    r::encode_taa(device, f.taa, f.pipelines, f.recorder, encoder, f.frame_group);
  }
  const int input = f.options.taa ? f.taa.written : 0;
  if (f.options.bloom > 0) {
    r::encode_bloom(f.bloom, f.pipelines, f.recorder, encoder, input);
  } else {
    // The pass table's row runs empty, so every later pass keeps its timing row.
    f.recorder.compute(encoder, "bloom").End();
  }
  if (r::lens_on(f.options.lens)) {
    r::encode_lens(f.lens, f.pipelines, f.recorder, encoder, f.frame_group, input);
  } else {
    f.recorder.compute(encoder, "lens").End();
  }
  if (r::dof_on(f.options.dof)) {
    r::encode_dof(f.dof, f.pipelines, f.recorder, encoder, f.frame_group, input);
  } else {
    f.recorder.compute(encoder, "dof").End();
  }
  if (f.options.look.motion_blur > 0) {
    r::encode_motion_blur(f.blur, f.pipelines, f.recorder, encoder);
  } else {
    f.recorder.compute(encoder, "motion_blur.tiles").End();
  }
  r::encode_post(f.post, f.pipelines, f.recorder, encoder, f.frame_group, f.post_groups[input],
                 output);
  if (scene.overlay != nullptr && scene.overlay->size() > 0) {
    r::prepare_ui(device, f.ui, *scene.overlay, f.output_width, f.output_height);
    r::encode_ui(f.ui, f.pipelines, f.recorder, encoder, output);
  } else {
    f.recorder.compute(encoder, "ui").End();  // the row runs empty, as bloom's does
  }
  f.recorder.end_frame(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  f.recorder.stats.cpu_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  device.queue.Submit(1, &commands);
  f.recorder.after_submit(device);
}

void draw_wire(ge::gpu::Device& device, GameFrame& f, const wgpu::TextureView& colour) {
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  r::encode_terrain_wire(f.terrain, f.pipelines, encoder, f.targets, f.frame_group, colour, true);
  r::encode_instances_wire(f.families, f.instancing, f.pipelines, encoder, f.targets, f.frame_group,
                           f.instance_light, colour);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
}

}  // namespace fall_line::app
