// Fall Line's pass table, the frame's creation and its draw.
//
// ge:summary Fall Line's pass table, frame creation and draw
// ge:keywords fall-line pass-table frame draw submit
// ge:skill render
#include "fall_line/app/frame.hpp"

#include <algorithm>
#include <chrono>
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
    {"terrain.cull", "terrain.pyramid", "terrain.tiles terrain.indirect"},
    {"instances.cull", "instances hiz.last", "instances.early"},
    {"terrain.horizon", "terrain.height", "terrain.horizon"},
    {"shadows.cull", "instances", "shadows.lists"},
    {"shadows.near", "instances shadows.lists", "shadows.maps"},
    {"shadows.far", "instances shadows.lists", "shadows.maps"},
    {"shadows.figures", "instances shadows.lists", "shadows.maps"},
    {"terrain.depth", "terrain.tiles terrain.indirect terrain.height", "depth"},
    {"instances.depth", "instances instances.early", "depth"},
    {"hiz", "depth", "hiz"},
    {"instances.cull.late", "instances hiz", "instances.late"},
    {"instances.depth.late", "instances instances.late", "depth"},
    {"terrain",
     "terrain.tiles terrain.indirect terrain.height terrain.horizon shadows.maps sky.aerial "
     "sky.light snow.ground depth",
     "hdr motion"},
    {"instances",
     "instances instances.early instances.late terrain.horizon shadows.maps sky.aerial sky.light "
     "depth",
     "hdr motion"},
    {"sky", "sky.view depth", "hdr motion"},
    {"taa", "hdr motion depth taa.history sky.light", "taa.history"},
    {"bloom", "taa.history", "bloom"},
    {"post", "taa.history bloom sky.light", "final"},
};
// What is made at load rather than by a pass (the instances' meshes and streams: the figures'
// are written before the frame is encoded), and what is the last frame's (the TAA history, the
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
  auto f = std::make_unique<GameFrame>();
  f->options = options;
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
  if (options.snow) {
    ge::mountain::app::create_snow(device, f->snow, level);
    source.shader = ge::mountain::app::snow_shader();
    source.shader_label = "snow";
    source.material = f->snow.layout;
  }
  r::create_terrain(device, f->pipelines, f->terrain, source, f->frame_layout);
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
  if (!f->pipelines.finish(device)) return nullptr;
  ge::gpu::end_load(device);
  f->frame_group = r::frame_group(device, f->frame_layout, f->uniforms, f->sky);
  f->level = &level;
  const std::vector<r::Instance> trees = tree_instances(level);
  r::create_family(device, f->instancing, f->forest, "forest", forest_meshes(), forest_materials(),
                   static_cast<uint32_t>(std::max<size_t>(trees.size(), 1)));
  r::set_instances(device, f->forest, 0, trees);
  r::create_family(device, f->instancing, f->figures, "figures", figure_meshes(),
                   figure_materials(), kMaxFigures * ge::skier::kFigurePieces);
  f->staged_figures.resize(size_t{kMaxFigures} * ge::skier::kFigurePieces);
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
  const size_t racers = std::min<size_t>(scene.skiers.size(), kMaxFigures);
  {
    GE_ZONE("frame.figures");
    for (size_t k = 0; k < racers; ++k) {
      figure_instances(
          scene.skiers[k], *f.level, static_cast<int>(k),
          std::span<r::Instance, ge::skier::kFigurePieces>(
              f.staged_figures.data() + k * ge::skier::kFigurePieces, ge::skier::kFigurePieces));
    }
    r::set_instances(
        device, f.figures, 0,
        std::span<const r::Instance>(f.staged_figures.data(), racers * ge::skier::kFigurePieces));
    r::set_instance_count(f.figures, static_cast<uint32_t>(racers * ge::skier::kFigurePieces));
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
  const std::span<r::InstanceFamily* const> families(f.families);
  r::encode_terrain_cull(f.terrain, f.pipelines, f.recorder, encoder, f.frame_group);
  r::encode_instances_cull(families, f.instancing, f.pipelines, f.recorder, encoder, f.frame_group,
                           false);
  r::encode_terrain_horizon(f.terrain, f.pipelines, f.recorder, encoder);
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
