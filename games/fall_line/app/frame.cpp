// Fall Line's pass table, the frame's creation and its draw.
//
// ge:summary Fall Line's pass table, frame creation and draw
// ge:keywords fall-line pass-table frame draw submit
// ge:skill render
#include "fall_line/app/frame.hpp"

#include <chrono>
#include <iterator>

#include "core/output.hpp"
#include "fall_line/app/look.hpp"

namespace fall_line::app {

namespace {

namespace r = ge::render;

// The pass table: the order `draw` runs, and what each pass reads and writes.
constexpr r::PassRow kPassTable[] = {
    {"sky.tables", "", "sky.view sky.aerial sky.light"},
    {"terrain.cull", "terrain.pyramid", "terrain.tiles terrain.indirect"},
    {"terrain.horizon", "terrain.height", "terrain.horizon"},
    {"terrain.depth", "terrain.tiles terrain.indirect terrain.height", "depth"},
    {"hiz", "depth", "hiz"},
    {"terrain",
     "terrain.tiles terrain.indirect terrain.height terrain.horizon sky.aerial sky.light depth",
     "hdr motion"},
    {"sky", "sky.view depth", "hdr motion"},
    {"taa", "hdr motion depth taa.history sky.light", "taa.history"},
    {"post", "taa.history sky.light", "final"},
};
// What is made at load rather than by a pass (the history is the last frame's).
constexpr std::string_view kLoadInputs =
    "terrain.height terrain.pyramid terrain.albedo taa.history";

constexpr int kPasses = static_cast<int>(std::size(kPassTable));

// The targets, the pyramid, the histories and the post pass's groups for outputs of w x h.
void make_targets(ge::gpu::Device& device, GameFrame& f, uint32_t w, uint32_t h) {
  f.output_width = w;
  f.output_height = h;
  const double scale = f.options.taa ? f.options.scale : 1.0;
  f.targets = r::create_targets(device, "frame", r::taa_render_size(w, scale),
                                r::taa_render_size(h, scale));
  r::create_hiz_targets(device, f.hiz, f.targets);
  if (f.options.taa) {
    r::create_taa_targets(device, f.taa, f.targets, w, h);
    for (int k = 0; k < 2; ++k) {
      f.post_groups[k] = r::post_group(device, f.post, f.taa.history_views[k]);
    }
  } else {
    f.post_groups[0] = f.post_groups[1] = r::post_group(device, f.post, f.targets.hdr_view);
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
  r::create_terrain(device, f->pipelines, f->terrain, source, f->frame_layout);
  r::create_hiz_pipelines(device, f->pipelines, f->hiz);
  if (options.taa) {
    r::TaaSettings taa;
    taa.scale = options.scale;
    r::create_taa(device, f->pipelines, f->taa, f->frame_layout, taa);
  }
  r::create_post(device, f->pipelines, f->post, f->frame_layout, output);
  if (!f->pipelines.finish(device)) return nullptr;
  ge::gpu::end_load(device);
  f->frame_group = r::frame_group(device, f->frame_layout, f->uniforms, f->sky);
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
}

void draw(ge::gpu::Device& device, GameFrame& f, const r::FrameView& view,
          const wgpu::TextureView& output) {
  const auto start = std::chrono::steady_clock::now();
  f.recorder.begin_frame(device);
  // Prepare: every decision of the frame, and the uploads.
  double jitter[2] = {0, 0};
  if (f.options.taa) r::taa_jitter(view.frame, r::taa_phases(f.options.scale), jitter);
  r::fill_uniforms(f.staged, view, f.targets.width, f.targets.height, &f.history,
                   f.options.taa ? jitter : nullptr);
  device.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  r::prepare_sky(f.sky, view);
  r::prepare_terrain(device, f.terrain, view);
  // Encode, in the pass table's order.
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  r::encode_sky_tables(f.sky, f.pipelines, f.recorder, encoder);
  r::encode_terrain_cull(f.terrain, f.pipelines, f.recorder, encoder, f.frame_group);
  r::encode_terrain_horizon(f.terrain, f.pipelines, f.recorder, encoder);
  r::encode_terrain_depth(f.terrain, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_hiz(f.hiz, f.pipelines, f.recorder, encoder);
  r::encode_terrain(f.terrain, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  r::encode_sky(f.sky, f.pipelines, f.recorder, encoder, f.targets, f.frame_group);
  if (f.options.taa) {
    r::encode_taa(device, f.taa, f.pipelines, f.recorder, encoder, f.frame_group);
  }
  r::encode_post(f.post, f.pipelines, f.recorder, encoder, f.frame_group,
                 f.post_groups[f.options.taa ? f.taa.written : 0], output);
  f.recorder.end_frame(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  f.recorder.stats.cpu_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  device.queue.Submit(1, &commands);
  f.recorder.after_submit(device);
}

}  // namespace fall_line::app
