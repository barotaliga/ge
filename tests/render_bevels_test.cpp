// engine/render/bevels.hpp: on a device (the software adapter here and on CI), over a field with a
// block standing on it: bevels a few metres wide change the picture round the block's convex
// edges and nowhere on the open field; bevels under a pixel leave the picture as it was, bit for
// bit; with no validation error.
//
// ge:summary Tests for the bevels: the block's edges changed, the flat field and sub-pixel
// ge:summary bevels not
// ge:keywords test render bevels bevel edge crease convex screen-space ce_bevels
// ge:skill render testing
#include <cstdlib>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/bevels.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  r::FrameHistory history;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  ::ge::render::FrameGroup plain;
  wgpu::BindGroup post_group;
  r::Sky sky;
  r::Terrain terrain;
  r::Bevels bevels;
  r::Post post;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  // Flat at 0, 512 m a side, with a block 40 m tall standing 60 m ahead of the eye.
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);
  for (int row = 22; row < 26; ++row) {
    for (int col = 58; col < 70; ++col) s.field.data[static_cast<size_t>(row) * 129 + col] = 40;
  }
  s.albedo.assign(static_cast<size_t>(s.field.cols) * s.field.rows * 4, 120);
  s.targets = r::create_targets(d, "test", kW, kH);
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.levels = 6;
  r::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  r::create_bevels(d, s.pipelines, s.bevels);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  r::create_bevel_targets(d, s.bevels, s.targets, s.uniforms);
  s.plain = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "test.timer", 12);
}

r::FrameView ground_view(double wet) {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 3;
  v.eye[2] = 30;
  v.heading = 0;  // along +z, toward the block
  v.pitch = -0.08;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  v.weather.wet = wet;
  v.weather.puddles = wet;
  return v;
}

// One frame drawn, its edges bevelled when `bevel`.
ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out, bool bevel) {
  const r::FrameView v = ground_view(0);
  const r::FrameGroup& group = s.plain;
  s.rec.begin_frame(d);
  s.history.valid = false;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, group);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, group);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, group);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, group);
  if (bevel) r::encode_bevels(s.bevels, s.pipelines, s.rec, e, s.targets);
  r::encode_post(s.post, s.pipelines, s.rec, e, group, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

// How many bytes differ by more than `by`.
int changed(const ge::gpu::Image& a, const ge::gpu::Image& b, int by) {
  int n = 0;
  for (size_t i = 0; i < a.rgba.size(); ++i) {
    if (std::abs(static_cast<int>(a.rgba[i]) - static_cast<int>(b.rgba[i])) > by) ++n;
  }
  return n;
}

}  // namespace

TEST(wide_bevels_light_the_blocks_edges_and_leave_the_field) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  r::set_bevels(d, s.bevels, {.radius_m = 3, .strength = 1});
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image plain = frame(d, s, out, false);
  const ge::gpu::Image bevelled = frame(d, s, out, true);
  CHECK(changed(plain, bevelled, 2) > 10);
  // The field's near rows (the open ground at the eye's feet) are as they were.
  for (size_t i = size_t{kH - 4} * kW * 4; i < plain.rgba.size(); ++i)
    CHECK(plain.rgba[i] == bevelled.rgba[i]);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}

TEST(bevels_under_a_pixel_leave_the_picture_bit_for_bit) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  r::set_bevels(d, s.bevels, {.radius_m = 0.001, .strength = 1});
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image plain = frame(d, s, out, false);
  const ge::gpu::Image bevelled = frame(d, s, out, true);
  CHECK(plain.rgba == bevelled.rgba);
  CHECK(d.errors.load() == 0);
}
