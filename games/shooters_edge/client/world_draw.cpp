// The window's world frame (world_draw.hpp).
//
// ge:summary The window's world frame: the BSP map, the effects, sky, bloom, post, the overlay
// ge:keywords world-draw bsp frame create_world_draw draw_world resize_world particles
// ge:skill render brush shooter
#include "shooters_edge/client/world_draw.hpp"

#include <algorithm>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_sky.hpp"
#include "brush/app/draw_data.hpp"
#include "core/output.hpp"
#include "render/bloom.hpp"
#include "render/clouds.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/ui_pass.hpp"

namespace shooters_edge::client {

namespace ba = ge::brush::app;
namespace gr = ge::render;

struct WorldDraw {
  const look::Picture* picture = nullptr;
  bool has_map = false;
  ba::Scene scene;
  ba::BspDraw bsp;
  ba::SkyPick sky;
  double top_y = 0;  // the map's highest point, m: the clouds stand over it
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
  gr::Particles particles;
  WorldModels models;
  gr::UiPass ui;
  gr::FrameRecorder rec;
  uint32_t w = 0, h = 0;
};

void WorldDrawDeleter::operator()(WorldDraw* w) const { delete w; }

namespace {

// Everything sized to the targets: made at create and again at a resize.
void make_targets(ge::gpu::Device& d, WorldDraw& f) {
  f.targets = gr::create_targets(d, "shooters_edge", f.w, f.h);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, f.w, f.h, inputs);
  for (int p = 0; p < 2; ++p)
    f.post_group[p] = gr::post_group(d, f.post[p], f.targets.hdr_view, f.bloom.view);
  gr::bind_particle_targets(d, f.particles, f.targets);
  if (f.has_map) ba::create_bsp_targets(d, f.bsp, f.targets);
}

}  // namespace

WorldDrawPtr create_world_draw(ge::gpu::Device& d, const SearchPath& files, const LoadedMap& map,
                               uint32_t width, uint32_t height, wgpu::TextureFormat format,
                               const look::Picture& picture, std::string& error,
                               const ModelLoad& models) {
  // A map's frame is made while the map loads, the first and every later one: its pipelines are
  // the load's, not a hitch mid-run.
  d.loading = true;
  WorldDrawPtr f(new WorldDraw);
  f->picture = &picture;
  f->w = std::max(1u, width);
  f->h = std::max(1u, height);
  std::vector<uint8_t> palette_copy;
  std::vector<uint8_t> grade_palette;  // the classic grade's colours (none: the identity)
  if (map.from_file) {
    std::vector<uint8_t> store;
    std::span<const uint8_t> palette = find_file(files, "gfx/palette.lmp", store);
    std::vector<uint8_t> grey;
    if (palette.size() < 768) {  // no palette in the packs: a grey ramp, so the map still shows
      grey.resize(768);
      for (size_t i = 0; i < 768; i++) grey[i] = static_cast<uint8_t>(i / 3);
      palette = grey;
      ge::core::output::status("shooters_edge: no gfx/palette.lmp in the packs; drawing grey");
    }
    palette_copy.assign(palette.begin(), palette.end());
    if (grey.empty()) grade_palette = palette_copy;
    ba::Pack pack;  // the map file alone, as the scene reader takes it
    pack.bytes = map.file;
    pack.files.push_back({"maps/" + map.name + ".bsp", 0, static_cast<uint32_t>(map.file.size())});
    if (!ba::load_scene(pack, pack.files[0].name, palette_copy, &f->scene, &error)) {
      error = "the map " + map.name + " cannot be drawn: " + error;
      return nullptr;
    }
    f->has_map = true;
    f->sky = ba::pick_sky(f->scene.sky_colours);
    f->top_y = f->scene.maxs[1];
  }
  f->uniforms = gr::make_buffer(d, "shooters_edge.uniforms", sizeof(gr::FrameUniforms),
                                wgpu::BufferUsage::Uniform);
  f->layout = gr::frame_layout(d);
  gr::create_sky(d, f->pipelines, f->skypass, f->uniforms, f->layout);
  gr::create_clouds(d, f->pipelines, f->clouds, f->layout);
  gr::BloomSettings bs;
  bs.strength = look::post_settings(look::Look::modern, picture).bloom;
  gr::create_bloom_pipelines(d, f->pipelines, f->bloom, bs);
  for (int p = 0; p < 2; ++p)
    gr::create_post(
        d, f->pipelines, f->post[p], f->layout, format,
        look::post_settings(p == 0 ? look::Look::classic : look::Look::modern, picture));
  gr::set_grade(d, f->post[0], look::palette_grade(grade_palette));
  gr::create_particles(d, f->pipelines, f->particles, f->layout);
  gr::create_ui_pass(d, f->pipelines, f->ui, format);
  if (f->has_map) {
    ba::create_bsp(d, f->pipelines, f->bsp, f->scene, f->layout);
    create_models(d, f->pipelines, f->models, f->layout, files, palette_copy, f->scene.map, models);
  }
  if (!f->pipelines.finish(d)) {
    error = "the world's pipelines did not build; next: read the gpu: lines above";
    return nullptr;
  }
  ge::gpu::end_load(d);
  f->frame = gr::frame_group(d, f->layout, f->uniforms, f->skypass);
  f->rec.init(d, "shooters_edge.timer", 16);
  make_targets(d, *f);
  return f;
}

void resize_world(ge::gpu::Device& d, WorldDraw& f, uint32_t width, uint32_t height) {
  f.w = std::max(1u, width);
  f.h = std::max(1u, height);
  make_targets(d, f);
}

void draw_world(ge::gpu::Device& d, WorldDraw& f, gr::FrameView view, const FxFrame& fx, double dt,
                const ge::ui::DrawList& overlay, const wgpu::TextureView& target) {
  const look::Look lk = look::look_of(*f.picture);
  const int p = lk == look::Look::classic ? 0 : 1;
  // The map's own sky (its pick), keeping the eye and the look the caller set.
  ba::dress_sky(view, f.sky, f.top_y, view.time_s);
  view.altitude_m = 100;
  view.clouds.quarter = false;
  view.clouds.spread = false;
  f.rec.begin_frame(d);
  gr::fill_uniforms(f.staged, view, f.w, f.h);
  d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  gr::prepare_sky(f.skypass, view);
  ba::Options o = look::world_options(lk, {}, *f.picture);
  o.sky_light = f.sky.sky_light;
  o.sky_seen = f.sky.sky_seen;
  for (int k = 0; k < 3; ++k) o.sky_tint[k] = f.sky.tint[k];
  if (f.has_map) {
    ba::set_dynamic_lights(f.bsp, fx.lights);
    ba::prepare_bsp(d, f.bsp, view, o);
  }
  gr::emit_particles(f.particles, fx.births);
  f.particles.settings.glow = static_cast<float>(look::effects_glow(lk, *f.picture));
  gr::prepare_particles(d, f.particles, dt, view.near_m);
  if (f.has_map)
    prepare_models(d, f.models, f.scene.map, fx.models, view, o.brightness, o.minlight, !o.crisp);
  gr::set_post_blend(d, f.post[p], fx.blend);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_particles_sim(f.particles, f.pipelines, f.rec, e);
  gr::encode_sky_tables(f.skypass, f.pipelines, f.rec, e);
  if (f.has_map) ba::encode_bsp(f.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
  encode_models(f.models, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_sky(f.skypass, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, view, f.staged);
  if (f.has_map) ba::encode_bsp_veil(f.bsp, f.pipelines, f.rec, e, f.targets);
  gr::encode_particles(f.particles, f.pipelines, f.rec, e, f.targets, f.frame);
  encode_model_sprites(f.models, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
  gr::encode_post(f.post[p], f.pipelines, f.rec, e, f.frame, f.post_group[p], target);
  if (overlay.size() > 0) {
    gr::prepare_ui(d, f.ui, overlay, f.w, f.h);
    gr::encode_ui(f.ui, f.pipelines, f.rec, e, target);
  }
  f.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  f.rec.after_submit(d);
}

}  // namespace shooters_edge::client
