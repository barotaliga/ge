// The window's world frame (world_draw.hpp).
//
// ge:summary The window's world frame: the BSP map over the sky, bloom, post, the overlay
// ge:keywords world-draw bsp frame create_world_draw draw_world resize_world
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
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/ui_pass.hpp"

namespace shooters_edge::client {

namespace ba = ge::brush::app;
namespace gr = ge::render;

struct WorldDraw {
  WorldLook look;
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
  wgpu::BindGroup frame, post_group;
  gr::Sky skypass;
  gr::CloudPass clouds;
  gr::Bloom bloom;
  gr::Post post;
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
  f.post_group = gr::post_group(d, f.post, f.targets.hdr_view, f.bloom.view);
  if (f.has_map) ba::create_bsp_targets(d, f.bsp, f.targets);
}

}  // namespace

WorldDrawPtr create_world_draw(ge::gpu::Device& d, const SearchPath& files, const LoadedMap& map,
                               uint32_t width, uint32_t height, wgpu::TextureFormat format,
                               const WorldLook& look, std::string& error) {
  // A map's frame is made while the map loads, the first and every later one: its pipelines are
  // the load's, not a hitch mid-run.
  d.loading = true;
  WorldDrawPtr f(new WorldDraw);
  f->look = look;
  f->w = std::max(1u, width);
  f->h = std::max(1u, height);
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
    std::vector<uint8_t> palette_copy(palette.begin(), palette.end());
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
  bs.strength = 0.05;
  gr::create_bloom_pipelines(d, f->pipelines, f->bloom, bs);
  gr::PostSettings ps;
  ps.bloom = 0.05;
  gr::create_post(d, f->pipelines, f->post, f->layout, format, ps);
  gr::create_ui_pass(d, f->pipelines, f->ui, format);
  if (f->has_map) ba::create_bsp(d, f->pipelines, f->bsp, f->scene, f->layout);
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

void draw_world(ge::gpu::Device& d, WorldDraw& f, gr::FrameView view,
                const ge::ui::DrawList& overlay, const wgpu::TextureView& target) {
  // The map's own sky (its pick), keeping the eye and the look the caller set.
  ba::dress_sky(view, f.sky, f.top_y, view.time_s);
  view.altitude_m = 100;
  view.clouds.quarter = false;
  view.clouds.spread = false;
  f.rec.begin_frame(d);
  gr::fill_uniforms(f.staged, view, f.w, f.h);
  d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  gr::prepare_sky(f.skypass, view);
  ba::Options o;
  o.mode = f.look.v2 ? ba::Mode::v2 : ba::Mode::classic;
  o.lighting = f.look.v2 ? ba::Lighting::lights : ba::Lighting::lightmaps;
  o.sky_light = f.sky.sky_light;
  o.sky_seen = f.sky.sky_seen;
  for (int k = 0; k < 3; ++k) o.sky_tint[k] = f.sky.tint[k];
  if (f.has_map) ba::prepare_bsp(d, f.bsp, view, o);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(f.skypass, f.pipelines, f.rec, e);
  if (f.has_map) ba::encode_bsp(f.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_sky(f.skypass, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, view, f.staged);
  if (f.has_map) ba::encode_bsp_veil(f.bsp, f.pipelines, f.rec, e, f.targets);
  gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
  gr::encode_post(f.post, f.pipelines, f.rec, e, f.frame, f.post_group, target);
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
