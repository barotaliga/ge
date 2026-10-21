// engine/render's passes on a device (the software adapter here and on CI): the sky's tables,
// the terrain's cull and draw and the post pass, run over a small synthetic heightfield in the
// pass table's order, draw a frame with no GPU error and no pipeline after the load; the sky
// fills the top of the frame (bluer than red), the ground the bottom; the GPU cull draws some
// tiles but not all of them, and none when the camera looks straight up at the sky.
//
// ge:summary Tests for the frame core on a device: sky, terrain cull and draw, post, offscreen
// ge:keywords test render gpu sky terrain post cull indirect offscreen readback
// ge:skill render testing
#include <cmath>
#include <cstring>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 64, kH = 48;
constexpr int kLevels = 6;

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  r::Sky sky;
  r::Terrain terrain;
  r::Post post;
  r::FrameRecorder rec;
};

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
  r::fill_uniforms(s.staged, v, kW, kH);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
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

  // Rolling hills 512 m across, 4 m a sample.
  ge::core::Heightfield field = ge::core::create_heightfield(0, 0, 4, 129, 129);
  for (int row = 0; row < field.rows; ++row) {
    for (int col = 0; col < field.cols; ++col) {
      field.data[static_cast<size_t>(row * field.cols + col)] =
          static_cast<float>(20 * std::sin(col * 0.1) * std::cos(row * 0.13));
    }
  }
  std::vector<uint8_t> albedo(static_cast<size_t>(field.cols * field.rows) * 4, 230);

  Scene s;
  s.targets = r::create_targets(d, "test", kW, kH);
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::TerrainSource src;
  src.height = &field;
  src.albedo = &albedo;
  src.base_spacing = 1.0;
  src.levels = kLevels;
  r::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets);
  s.rec.init(d, "test.timer", 5);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);

  r::FrameView v;
  v.eye[0] = 256;
  v.eye[2] = 100;
  v.eye[1] = 60;
  v.heading = 0;  // along +z, over the hills
  v.pitch = -0.05;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
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
