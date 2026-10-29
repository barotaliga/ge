// engine/render/valley_fog.hpp: the share above a height and its integral against a numeric
// one; a ray's depth through the fog from over its top, from inside it and level; the frame's
// rows (none with no fog, the noise's offset a share of its tile); and on a device (the software
// adapter here and on CI), over a flat field: a top under the ground leaves the ground and the
// sky as they were drawn with no fog, bit for bit; an absorbing fog over the eye darkens every
// pixel; a white one sends its light back; with no validation error.
//
// ge:summary Tests for the valley fog: its closed-form depth, its rows, and its frames on a device
// ge:keywords test render valley-fog sea-of-fog optical-depth valley_depth valley_rows
// ge:skill render testing
#include <cmath>
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
#include "render/valley_fog.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

// The depth along a ray summed in small steps.
double marched(double top, double edge, double density, double dy, double dist) {
  const int n = 200000;
  double sum = 0;
  for (int i = 0; i < n; ++i) {
    const double l = (i + 0.5) * dist / n;
    sum += r::valley_share(top, edge, dy * l);
  }
  return density * sum * dist / n;
}

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  r::FrameHistory history;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  r::Sky sky;
  r::Terrain terrain;
  r::Post post;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);  // flat at 0, 512 m
  s.albedo.assign(static_cast<size_t>(s.field.cols) * s.field.rows * 4, 230);
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
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "test.timer", 12);
}

r::FrameView ground_view(const r::ValleyFog& valley) {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 2;
  v.eye[2] = 30;
  v.heading = 0;  // along +z
  v.pitch = -0.05;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  v.valley = valley;
  return v;
}

ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out,
                     const r::ValleyFog& valley) {
  const r::FrameView v = ground_view(valley);
  s.rec.begin_frame(d);
  s.history.valid = false;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

// A row's mean luminance (8-bit, rgb summed).
double row_sum(const ge::gpu::Image& img, uint32_t y) {
  double sum = 0;
  for (uint32_t x = 0; x < kW; ++x) {
    const size_t i = (static_cast<size_t>(y) * kW + x) * 4;
    sum += img.rgba[i] + img.rgba[i + 1] + img.rgba[i + 2];
  }
  return sum / kW;
}

bool rows_equal(const ge::gpu::Image& a, const ge::gpu::Image& b, uint32_t y0, uint32_t y1) {
  for (uint32_t y = y0; y < y1; ++y) {
    for (uint32_t x = 0; x < kW * 4; ++x) {
      const size_t i = static_cast<size_t>(y) * kW * 4 + x;
      if (a.rgba[i] != b.rgba[i]) return false;
    }
  }
  return true;
}

}  // namespace

TEST(the_column_over_a_height_is_the_share_integrated) {
  const double top = 30, edge = 20;
  CHECK(r::valley_share(top, edge, 50) == 0);
  CHECK(r::valley_share(top, edge, 10) == 1);
  CHECK(std::fabs(r::valley_share(top, edge, 30) - 0.5) < 1e-12);
  CHECK(r::valley_column(top, edge, 40) == 0);
  CHECK(std::fabs(r::valley_column(top, edge, 20) - 10) < 1e-12);  // half the edge
  CHECK(std::fabs(r::valley_column(top, edge, -100) - 130) < 1e-12);
  // Its slope is minus the share at every height.
  for (int i = 0; i < 115; ++i) {
    const double y = -20 + 0.7 * i;
    const double h = 1e-4;
    const double slope =
        (r::valley_column(top, edge, y + h) - r::valley_column(top, edge, y - h)) / (2 * h);
    CHECK(std::fabs(slope + r::valley_share(top, edge, y)) < 1e-6);
  }
}

TEST(a_rays_depth_is_the_marched_one_from_over_the_top_inside_it_and_level) {
  struct Ray {
    double top, dy, dist;
  };
  const Ray rays[] = {
      {-200, -0.3, 2000},                      // from a peak down into the sea
      {-200, -0.05, 9000}, {-200, 0.2, 3000},  // up from over it: none
      {50, 0.4, 500},                          // up out of it from inside
      {50, -0.1, 800},                         // down from inside it
      {50, 0, 700},                            // level inside it
      {5, 0.00005, 900},                       // nearly level across the edge
  };
  for (const Ray& ray : rays) {
    const double got = r::valley_depth(ray.top, 20, 0.01, ray.dy, ray.dist);
    const double want = marched(ray.top, 20, 0.01, ray.dy, ray.dist);
    CHECK(std::fabs(got - want) <= 1e-4 * want + 1e-6);
  }
  CHECK(r::valley_depth(-200, 20, 0.01, 0.2, 3000) == 0);
}

TEST(the_rows_are_zero_with_no_fog_and_the_offset_is_a_share_of_the_tile) {
  r::FrameView v;
  float rows[2][4];
  r::valley_rows(v.valley, v, rows);
  for (const auto& row : rows) {
    for (const float f : row) CHECK(f == 0.0f);
  }
  v.eye[0] = 123456.7;
  v.eye[1] = 1500;
  v.eye[2] = -98765.4;
  v.time_s = 3600;
  v.valley.top_m = 1200;
  v.valley.density = 0.02;
  v.valley.drift[0] = 2;
  r::valley_rows(v.valley, v, rows);
  CHECK(rows[0][0] == -300.0f);
  CHECK(rows[0][1] == 0.02f);
  for (int k = 1; k < 3; ++k) CHECK(rows[1][k] >= 0.0f && rows[1][k] < 1.0f);
  const double x = (123456.7 - 2 * 3600) / 600.0;
  CHECK(std::fabs(rows[1][1] - (x - std::floor(x))) < 1e-6);
}

TEST(a_top_under_the_ground_leaves_the_frame_as_drawn_with_no_fog) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image none = frame(d, s, out, {});
  r::ValleyFog under;
  under.top_m = -60;
  under.density = 0.05;
  const ge::gpu::Image fog = frame(d, s, out, under);
  // The sky over the horizon and the near ground; the rows round the horizon see past the field's
  // edge, under the horizon, where the sea is.
  CHECK(rows_equal(none, fog, 0, kH / 4));
  CHECK(rows_equal(none, fog, kH * 3 / 4, kH));
  CHECK(d.errors.load() == 0);
}

TEST(an_absorbing_fog_over_the_eye_darkens_and_a_white_one_gives_its_light) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image none = frame(d, s, out, {});
  r::ValleyFog fog;
  fog.top_m = 40;
  fog.density = 0.02;
  fog.albedo = 0;
  const ge::gpu::Image dark = frame(d, s, out, fog);
  for (uint32_t y = 0; y < kH; y += 4) CHECK(row_sum(dark, y) < row_sum(none, y));
  // A white fog sends its light back where the black one only took it out.
  fog.albedo = 1;
  const ge::gpu::Image white = frame(d, s, out, fog);
  for (uint32_t y = 0; y < kH; y += 4) CHECK(row_sum(white, y) > row_sum(dark, y));
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}
