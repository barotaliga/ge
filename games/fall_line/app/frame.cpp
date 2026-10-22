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

#include "core/output.hpp"
#include "fall_line/app/look.hpp"
#include "fall_line/app/scene.hpp"
#include "prof/zones.hpp"

namespace fall_line::app {

namespace {

namespace r = ge::render;

// The pass table: the order `draw` runs, and what each pass reads and writes.
constexpr r::PassRow kPassTable[] = {
    {"sky.tables", "", "sky.view sky.aerial sky.light"},
    {"skinning", "", "figures.posed"},
    {"terrain.cull", "terrain.pyramid", "terrain.tiles terrain.indirect"},
    {"instances.cull", "instances figures.posed hiz.last", "instances.early"},
    {"terrain.horizon", "terrain.height", "terrain.horizon"},
    {"trails.fine", "", "trails.fine"},
    {"trails.coarse", "", "trails.coarse"},
    {"particles.sim", "terrain.height", "particles"},
    {"shadows.cull", "instances", "shadows.lists"},
    {"shadows.near", "instances shadows.lists", "shadows.maps"},
    {"shadows.far", "instances shadows.lists", "shadows.maps"},
    {"shadows.figures", "instances figures.posed shadows.lists", "shadows.maps"},
    {"terrain.depth", "terrain.tiles terrain.indirect terrain.height", "depth"},
    {"instances.depth", "instances figures.posed instances.early", "depth"},
    {"hiz", "depth", "hiz"},
    {"instances.cull.late", "instances hiz", "instances.late"},
    {"instances.depth.late", "instances figures.posed instances.late", "depth"},
    {"terrain",
     "terrain.tiles terrain.indirect terrain.height terrain.horizon shadows.maps sky.aerial "
     "sky.light snow.ground trails.fine trails.coarse depth",
     "hdr motion"},
    {"instances",
     "instances figures.posed instances.early instances.late terrain.horizon shadows.maps "
     "sky.aerial sky.light depth",
     "hdr motion"},
    {"sky", "sky.view depth", "hdr motion"},
    {"particles", "particles terrain.horizon shadows.maps sky.aerial sky.light depth",
     "hdr motion"},
    {"taa", "hdr motion depth taa.history sky.light", "taa.history"},
    {"bloom", "taa.history", "bloom"},
    {"post", "taa.history bloom sky.light", "final"},
};
// What is made at load rather than by a pass (the instances' meshes and streams: the figures'
// placements and palettes are written before the frame is encoded, their vertices by the skinning
// pass), and what is the last frame's (the TAA history, the
// depth pyramid the early cull reads).
constexpr std::string_view kLoadInputs =
    "terrain.height terrain.pyramid terrain.albedo snow.ground instances taa.history hiz.last";

constexpr int kPasses = static_cast<int>(std::size(kPassTable));

// The targets, the pyramid, the histories and the post pass's groups for outputs of w x h.
void make_targets(ge::gpu::Device& device, GameFrame& f, uint32_t w, uint32_t h) {
  f.output_width = w;
  f.output_height = h;
  const double scale = f.options.taa ? f.options.scale : 1.0;
  f.targets = r::create_targets(device, "frame", r::taa_render_size(w, scale),
                                r::taa_render_size(h, scale));
  r::create_hiz_targets(device, f.hiz, f.targets);
  for (r::InstanceFamily* family : f.families)
    r::bind_family_hiz(device, f.instancing, *family, f.hiz);
  r::bind_particle_targets(device, f.particles, f.targets);
  // The post pass (and the bloom) read TAA's two histories, or the HDR target for both.
  wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  if (f.options.taa) {
    r::create_taa_targets(device, f.taa, f.targets, w, h);
    inputs[0] = f.taa.history_views[0];
    inputs[1] = f.taa.history_views[1];
  }
  const bool glow = f.options.bloom > 0;
  if (glow) r::create_bloom_targets(device, f.bloom, w, h, inputs);
  for (int k = 0; k < 2; ++k) {
    f.post_groups[k] = r::post_group(device, f.post, inputs[k], glow ? f.bloom.view : nullptr);
  }
  f.history.valid = false;
}

// The racers' spray over the `dt` s since the last frame, handed to the pool as births.
void throw_spray(GameFrame& f, const FrameScene& scene, double dt) {
  if (!f.options.spray) return;
  for (size_t k = f.spray_pens.size(); k < scene.skiers.size(); ++k) {
    f.spray_pens.push_back(ge::skier::spray_pen(static_cast<uint32_t>(k)));
  }
  f.thrown.clear();
  for (size_t k = 0; k < scene.skiers.size(); ++k) {
    ge::skier::spray_grains(scene.skiers[k], dt, f.spray_pens[k], f.thrown);
  }
  f.births.resize(f.thrown.size());
  for (size_t i = 0; i < f.thrown.size(); ++i) {
    const ge::skier::SprayGrain& g = f.thrown[i];
    r::Particle& p = f.births[i];
    p = r::Particle{};
    p.p[0] = static_cast<float>(g.x);
    p.p[1] = static_cast<float>(g.y);
    p.p[2] = static_cast<float>(g.z);
    p.v[0] = static_cast<float>(g.vx);
    p.v[1] = static_cast<float>(g.vy);
    p.v[2] = static_cast<float>(g.vz);
    p.life = static_cast<float>(g.life);
    p.size = static_cast<float>(g.size);
    p.drag = static_cast<float>(g.drag);
    p.fall = static_cast<float>(g.fall);
    p.streak = static_cast<float>(g.streak);
    p.albedo = static_cast<float>(g.albedo);
    p.opacity = static_cast<float>(g.opacity);
  }
  r::emit_particles(f.particles, f.births);
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
  source.base_spacing = 1.0;
  source.levels = 8;
  source.overdraw_probe = options.overdraw_probe;
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
  r::create_hiz_pipelines(device, f->pipelines, f->hiz);
  r::create_instance_pipelines(device, f->pipelines, f->instancing, f->frame_layout,
                               options.overdraw_probe);
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
  r::PostSettings post;
  post.bloom = options.bloom;
  r::create_post(device, f->pipelines, f->post, f->frame_layout, output, post);
  r::set_grade(device, f->post, bake_grade());
  r::create_skin_pipelines(device, f->pipelines, f->skinning);
  if (!f->pipelines.finish(device)) return nullptr;
  ge::gpu::end_load(device);
  f->frame_group = r::frame_group(device, f->frame_layout, f->uniforms, f->sky);
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
                   model.meshes[0], ge::skier::pose_reach(), joints);
    f->skin_list[k] = &f->skins[k];
  }
  f->locals.resize(joints);
  f->globals.resize(joints);
  f->palette.resize(joints);
  f->staged_figures.resize(kMaxFigures);
  f->instance_light = r::instance_light_group(device, f->instancing, f->terrain);
  // The forest casts into the cascades (two caster slots), the figures into the atlas (one).
  r::ShadowSettings shadows;
  shadows.size = options.shadow_size;
  r::create_sun_shadows(device, f->shadows, shadows, f->frame_layout, f->sky);
  r::create_family_casters(device, f->instancing, f->forest, r::kCascades);
  r::create_family_casters(device, f->instancing, f->figures, 1);
  r::bind_terrain_shadows(f->terrain, f->shadows.group);
  if (options.snow) r::bind_terrain_material(f->terrain, f->snow.group);
  r::bind_instance_shadows(f->instancing, f->shadows.group);
  r::bind_particle_shadows(f->particles, f->shadows.group);
  make_targets(device, *f, width, height);
  f->recorder.init(device, "frame.timer", kPasses);
  return f;
}

void resize(ge::gpu::Device& device, GameFrame& f, uint32_t width, uint32_t height) {
  make_targets(device, f, width, height);
}

void cut(GameFrame& f) {
  f.history.valid = false;
  r::reset_taa(f.taa);
  r::cut_family(f.figures);
  for (ge::skier::RigPoser& p : f.posers) p = {};
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

void draw(ge::gpu::Device& device, GameFrame& f, const r::FrameView& view, const FrameScene& scene,
          const wgpu::TextureView& output) {
  GE_ZONE("frame");
  const auto start = std::chrono::steady_clock::now();
  f.recorder.begin_frame(device);
  // Prepare: every decision of the frame, and the uploads.
  // The last frame's pyramid culls only when there is a last frame of this camera.
  const bool occlusion = f.options.occlusion && f.history.valid;
  double jitter[2] = {0, 0};
  if (f.options.taa) r::taa_jitter(view.frame, r::taa_phases(f.options.scale), jitter);
  r::fill_uniforms(f.staged, view, f.targets.width, f.targets.height, &f.history,
                   f.options.taa ? jitter : nullptr);
  device.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  {
    GE_ZONE("frame.sky");
    r::prepare_sky(f.sky, view);
  }
  {
    GE_ZONE("frame.terrain");
    r::prepare_terrain(device, f.terrain, view);
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
    r::prepare_particles(device, f.particles, dt, view.near_m);
  }
  const size_t racers = std::min<size_t>(scene.skiers.size(), kMaxFigures);
  {
    GE_ZONE("frame.figures");
    for (size_t k = 0; k < racers; ++k) {
      // Settled: a fresh poser keeps no lag of the last frame.
      if (scene.settle) f.posers[k] = {};
      f.posers[k].pose(scene.skiers[k], scene.dt, f.rig_pose);
      ge::skier::rig_locals(*f.skier, f.binding, f.rig_pose, f.locals);
      ge::assets::pose_globals(*f.skier, f.locals, f.globals);
      ge::assets::skin_palette(*f.skier, f.globals, f.palette);
      r::set_palette(device, f.skins[k], f.palette);
      f.staged_figures[k] =
          figure_instance(scene.skiers[k], *f.level, static_cast<int>(k), f.materials_per_racer);
    }
    r::set_instances(device, f.figures, 0,
                     std::span<const r::Instance>(f.staged_figures.data(), racers));
    r::set_instance_count(f.figures, static_cast<uint32_t>(racers));
  }
  {
    GE_ZONE("frame.instances");
    for (r::InstanceFamily* family : f.families) {
      r::prepare_family(device, *family, view, f.targets.height, occlusion);
    }
  }
  // Each figure's tile is centred a metre over its skis.
  f.figure_centres.resize(racers);
  for (size_t k = 0; k < racers; ++k) {
    const ge::skier::SkierState& s = scene.skiers[k];
    f.figure_centres[k] = {s.x, s.y + 1.0, s.z};
  }
  r::InstanceFamily* still[] = {&f.forest};
  r::InstanceFamily* moving[] = {&f.figures};
  {
    GE_ZONE("frame.shadows");
    r::prepare_sun_shadows(device, f.shadows, view, f.figure_centres, still, moving);
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
  r::encode_sun_shadows(f.shadows, still, moving, f.instancing, f.pipelines, f.recorder, encoder);
  r::encode_terrain_depth(f.terrain, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_instances_depth(families, f.instancing, f.pipelines, f.recorder, encoder, f.targets,
                            f.frame_group, f.instance_light, false);
  r::encode_hiz(f.hiz, f.pipelines, f.recorder, encoder);
  r::encode_instances_cull(families, f.instancing, f.pipelines, f.recorder, encoder, f.frame_group,
                           true);
  r::encode_instances_depth(families, f.instancing, f.pipelines, f.recorder, encoder, f.targets,
                            f.frame_group, f.instance_light, true);
  r::encode_terrain(f.terrain, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_instances(families, f.instancing, f.pipelines, f.recorder, encoder, f.targets,
                      f.frame_group, f.instance_light);
  r::encode_sky(f.sky, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_particles(f.particles, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
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
  r::encode_post(f.post, f.pipelines, f.recorder, encoder, f.frame_group, f.post_groups[input],
                 output);
  f.recorder.end_frame(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  f.recorder.stats.cpu_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  device.queue.Submit(1, &commands);
  f.recorder.after_submit(device);
}

}  // namespace fall_line::app
