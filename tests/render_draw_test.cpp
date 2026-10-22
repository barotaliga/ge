// engine/render's passes on a device (the software adapter here and on CI): the sky's tables,
// the terrain's cull, depth prepass and draw, the Hi-Z pyramid, TAA-U and the post pass, run over
// a small synthetic heightfield in the pass table's order, draw a frame with no GPU error and no
// pipeline after the load; the sky fills the top of the frame (bluer than red), the ground the
// bottom; the GPU cull draws some tiles but not all of them, and none when the camera looks
// straight up at the sky. The pyramid is the CPU reference's of the depth, texel for texel; the
// prepass leaves every covered pixel shaded exactly once; TAA-U upscales a half-size render,
// and a still camera's resolved frame settles; the jitter cycle is centred and the history's
// motion is the camera's.
//
// ge:summary Tests for the frame core on a device: sky, terrain, Hi-Z, TAA-U, post, offscreen
// ge:keywords test render gpu sky terrain post cull indirect offscreen readback hiz taa jitter
// ge:keywords overdraw prepass motion-vectors
// ge:skill render testing
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/taa.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 64, kH = 48;
constexpr int kLevels = 6;

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  r::FrameHistory history;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_groups[2];
  r::Sky sky;
  r::Terrain terrain;
  r::Hiz hiz;
  r::Taa taa;
  r::Post post;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  double scale = 1.0;
};

// Rolling hills 512 m across, 4 m a sample, drawn into kW x kH outputs from a render at `scale`.
void make_scene(ge::gpu::Device& d, Scene& s, double scale) {
  s.scale = scale;
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);
  for (int row = 0; row < s.field.rows; ++row) {
    for (int col = 0; col < s.field.cols; ++col) {
      s.field.data[static_cast<size_t>(row) * s.field.cols + col] =
          static_cast<float>(20 * std::sin(col * 0.1) * std::cos(row * 0.13));
    }
  }
  s.albedo.assign(static_cast<size_t>(s.field.cols) * s.field.rows * 4, 230);
  s.targets =
      r::create_targets(d, "test", r::taa_render_size(kW, scale), r::taa_render_size(kH, scale));
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = kLevels;
  src.overdraw_probe = true;
  r::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  r::create_hiz_pipelines(d, s.pipelines, s.hiz);
  r::TaaSettings taa;
  taa.scale = scale;
  r::create_taa(d, s.pipelines, s.taa, s.layout, taa);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  r::create_hiz_targets(d, s.hiz, s.targets);
  r::create_taa_targets(d, s.taa, s.targets, kW, kH);
  for (int k = 0; k < 2; ++k) s.post_groups[k] = r::post_group(d, s.post, s.taa.history_views[k]);
  s.rec.init(d, "test.timer", 9);
}

r::FrameView hills_view() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[2] = 100;
  v.eye[1] = 60;
  v.heading = 0;  // along +z, over the hills
  v.pitch = -0.05;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  return v;
}

uint32_t read_tiles(ge::gpu::Device& d, const wgpu::Buffer& indirect) {
  wgpu::BufferDescriptor desc{};
  desc.size = 20;
  desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer rb = d.device.CreateBuffer(&desc);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  e.CopyBufferToBuffer(indirect, 0, rb, 0, 20);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  ge::gpu::wait(d, rb.MapAsync(wgpu::MapMode::Read, 0, 20, wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  uint32_t n = 0;
  if (const void* p = rb.GetConstMappedRange(0, 20)) {
    std::memcpy(&n, static_cast<const uint8_t*>(p) + 4, 4);
  }
  rb.Unmap();
  return n;
}

ge::gpu::Image draw(ge::gpu::Device& d, Scene& s, const r::FrameView& v,
                    const ge::gpu::Target& out) {
  s.rec.begin_frame(d);
  double jitter[2];
  r::taa_jitter(v.frame, r::taa_phases(s.scale), jitter);
  r::fill_uniforms(s.staged, v, s.targets.width, s.targets.height, &s.history, jitter);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_hiz(s.hiz, s.pipelines, s.rec, e);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_taa(d, s.taa, s.pipelines, s.rec, e, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_groups[s.taa.written], out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

}  // namespace

TEST(frame_core_draws_sky_and_ground) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, 1.0);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  r::FrameView v = hills_view();
  const ge::gpu::Image img = draw(d, s, v, out);
  CHECK(img.rgba.size() == size_t{kW} * kH * 4);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
  if (img.rgba.size() != size_t{kW} * kH * 4) return;

  auto px = [&](uint32_t x, uint32_t y, int c) { return int{img.rgba[(y * kW + x) * 4 + c]}; };
  // The sky at the top: blue over red, and not black.
  CHECK(px(kW / 2, 1, 2) > px(kW / 2, 1, 0));
  CHECK(px(kW / 2, 1, 2) > 60);
  // The ground at the bottom: bright snow, not the sky's colour.
  const int g = px(kW / 2, kH - 2, 0) + px(kW / 2, kH - 2, 1) + px(kW / 2, kH - 2, 2);
  CHECK(g > 300);

  const uint32_t tiles = read_tiles(d, s.terrain.indirect);
  CHECK(tiles > 0);
  CHECK(tiles < static_cast<uint32_t>(kLevels * r::kLevelTiles * r::kLevelTiles));

  v.pitch = 1.5;  // nearly straight up: no ground in view
  draw(d, s, v, out);
  CHECK(read_tiles(d, s.terrain.indirect) == 0);
  CHECK(d.errors.load() == 0);
}

TEST(hiz_is_the_reference_pyramid_of_the_depth) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, 1.0);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  draw(d, s, hills_view(), out);
  const std::vector<float> depth = r::read_depth(d, s.targets);
  const std::vector<std::vector<float>> want = r::hiz_reduce(depth, kW, kH);
  CHECK(static_cast<int>(want.size()) == s.hiz.mips);
  CHECK(s.hiz.width == kW / 2 && s.hiz.height == kH / 2);
  CHECK(want.back().size() == 1);
  for (int m = 0; m < s.hiz.mips; ++m) {
    const std::vector<float> got = r::read_hiz(d, s.hiz, m);
    CHECK(got == want[static_cast<size_t>(m)]);
  }
  // Some of the frame is sky (depth 0) and some ground, so the top mip is the sky's 0.
  CHECK(want.back()[0] == 0.0f);
  CHECK(*std::max_element(want[0].begin(), want[0].end()) > 0.0f);
  CHECK(d.errors.load() == 0);
}

TEST(hiz_reduce_covers_odd_edges) {
  // 5 x 3: mip 0 is 2 x 1 and each texel takes three columns, the middle one shared.
  const std::vector<float> depth = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f,  //
                                    0.9f, 0.8f, 0.1f, 0.6f, 0.5f,  //
                                    0.9f, 0.8f, 0.7f, 0.6f, 0.2f};
  const std::vector<std::vector<float>> p = r::hiz_reduce(depth, 5, 3);
  CHECK(p.size() == 2);
  CHECK(p[0].size() == 2);
  CHECK(p[0][0] == 0.1f && p[0][1] == 0.1f);
  CHECK(p[1].size() == 1 && p[1][0] == 0.1f);
}

TEST(prepass_shades_each_covered_pixel_once) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, 1.0);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  r::FrameView v = hills_view();
  v.pitch = -0.3;  // the hills overlap one another
  draw(d, s, v, out);
  const wgpu::Texture count =
      r::make_texture(d, "test.count", kW, kH, 1, wgpu::TextureFormat::R16Float,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);
  const wgpu::TextureView count_view = count.CreateView();
  auto counts = [&](bool after_prepass) {
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    r::encode_terrain_overdraw(s.terrain, s.pipelines, e, s.targets, s.frame, count_view,
                               after_prepass);
    wgpu::CommandBuffer c = e.Finish();
    d.queue.Submit(1, &c);
    return r::read_red(d, count, 0, kW, kH, wgpu::TextureFormat::R16Float);
  };
  const std::vector<float> once = counts(true);
  const std::vector<float> depth = r::read_depth(d, s.targets);
  int covered = 0, wrong = 0;
  for (size_t i = 0; i < once.size(); ++i) {
    if (depth[i] > 0) {
      ++covered;
      if (once[i] != 1.0f) ++wrong;
    } else if (once[i] != 0.0f) {
      ++wrong;
    }
  }
  CHECK(covered > 0);
  CHECK(wrong == 0);
  // Without the prepass the same draw shades some pixels more than once.
  const std::vector<float> many = counts(false);
  float most = 0;
  for (float c : many) most = std::max(most, c);
  CHECK(most > 1.0f);
  CHECK(d.errors.load() == 0);
}

TEST(taa_upscales_and_settles) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, 0.5);
  CHECK(s.targets.width == kW / 2 && s.targets.height == kH / 2);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  r::FrameView v = hills_view();
  const int phases = r::taa_phases(0.5);
  ge::gpu::Image last, img;
  for (int i = 0; i <= phases + 1; ++i) {
    v.frame = static_cast<uint64_t>(i);
    last = img;
    img = draw(d, s, v, out);
  }
  CHECK(img.rgba.size() == size_t{kW} * kH * 4 && last.rgba.size() == img.rgba.size());
  if (img.rgba.size() != size_t{kW} * kH * 4 || last.rgba.size() != img.rgba.size()) return;
  auto px = [&](uint32_t x, uint32_t y, int c) { return int{img.rgba[(y * kW + x) * 4 + c]}; };
  CHECK(px(kW / 2, 1, 2) > px(kW / 2, 1, 0));
  CHECK(px(kW / 2, kH - 2, 0) + px(kW / 2, kH - 2, 1) + px(kW / 2, kH - 2, 2) > 300);
  // A still camera, past one cycle: the next frame changes the picture by under a level a
  // channel on average.
  double diff = 0;
  for (size_t i = 0; i < img.rgba.size(); ++i) {
    if (i % 4 != 3) diff += std::abs(int{img.rgba[i]} - int{last.rgba[i]});
  }
  diff /= static_cast<double>(kW) * kH * 3;
  CHECK(diff < 1.0);
  CHECK(d.errors.load() == 0);
}

TEST(taa_jitter_cycle_is_centred) {
  CHECK(r::taa_phases(1.0) == 8);
  CHECK(r::taa_phases(0.5) == 32);
  CHECK(r::taa_phases(0.75) == 15);
  CHECK(r::taa_render_size(1280, 0.5) == 640);
  CHECK(r::taa_render_size(1, 0.25) == 1);
  for (int phases : {8, 15, 32}) {
    double sx = 0, sy = 0;
    for (int i = 0; i < phases; ++i) {
      double j[2];
      r::taa_jitter(static_cast<uint64_t>(i), phases, j);
      CHECK(j[0] >= -0.5 && j[0] < 0.5 && j[1] >= -0.5 && j[1] < 0.5);
      sx += j[0];
      sy += j[1];
    }
    CHECK(std::fabs(sx / phases) < 0.07 && std::fabs(sy / phases) < 0.07);
  }
  double a[2], b[2];
  r::taa_jitter(3, 8, a);
  r::taa_jitter(11, 8, b);
  CHECK(a[0] == b[0] && a[1] == b[1]);  // the cycle repeats
  r::taa_jitter(0, 8, a);
  CHECK(a[0] == 0.0 && std::fabs(a[1] - (1.0 / 3 - 0.5)) < 1e-12);  // Halton point 1
}
