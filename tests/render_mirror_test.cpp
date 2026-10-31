// engine/render/mirror.hpp: on a device (the software adapter here and on CI), over a field with a
// block standing on it: the kept scene's alpha is each pixel's distance from the eye (0 where the
// sky is); a wet, pooled field mirrors the block once the scene is kept (the first frame, with
// nothing kept yet, is the frame drawn with no mirror bound, bit for bit); a dry field is the same
// with the mirror bound or not, bit for bit; with no validation error.
//
// ge:summary Tests for the mirror: the kept distances, the puddle's reflection, none where dry
// ge:keywords test render mirror reflection ssr puddle wet-ground mirror-keep scene_reflection
// ge:skill render testing
#include <cmath>
#include <cstdlib>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/mirror.hpp"
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
  wgpu::BindGroup plain, mirrored, post_group;
  r::Sky sky;
  r::Terrain terrain;
  r::Mirror mirror;
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
  r::create_mirror(d, s.pipelines, s.mirror);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  r::create_mirror_targets(d, s.mirror, s.targets, s.uniforms);
  s.plain = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.mirrored = r::frame_group(d, s.layout, s.uniforms, s.sky, nullptr, nullptr, nullptr, nullptr,
                              s.mirror.last_view);
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

// One frame drawn with `group` (the mirror bound or not), the scene kept after it when `keep`.
ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out, double wet,
                     const wgpu::BindGroup& group, bool keep) {
  const r::FrameView v = ground_view(wet);
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
  if (keep) r::encode_mirror(s.mirror, s.pipelines, s.rec, e);
  r::encode_post(s.post, s.pipelines, s.rec, e, group, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

bool same(const ge::gpu::Image& a, const ge::gpu::Image& b) { return a.rgba == b.rgba; }

// How many bytes of the rows y0..y1 differ by more than `by`.
int changed(const ge::gpu::Image& a, const ge::gpu::Image& b, uint32_t y0, uint32_t y1, int by) {
  int n = 0;
  for (size_t i = size_t{y0} * kW * 4; i < size_t{y1} * kW * 4; ++i) {
    if (std::abs(static_cast<int>(a.rgba[i]) - static_cast<int>(b.rgba[i])) > by) ++n;
  }
  return n;
}

}  // namespace

TEST(the_kept_scene_holds_each_pixels_distance_and_nothing_where_the_sky_is) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  frame(d, s, out, 0, s.plain, true);
  const std::vector<float> kept =
      r::read_red(d, s.mirror.last, 0, kW, kH, wgpu::TextureFormat::RGBA16Float, 3);
  const std::vector<float> want = r::read_distance(d, s.targets, ground_view(0));
  int ground = 0, sky = 0;
  for (size_t i = 0; i < want.size(); ++i) {
    if (std::isinf(want[i])) {
      CHECK(kept[i] == 0.0f);
      ++sky;
    } else {
      // A half's precision: a part in a thousand.
      CHECK(std::fabs(kept[i] - want[i]) <= 2e-3 * want[i] + 1e-3);
      ++ground;
    }
  }
  CHECK(ground > 0 && sky > 0);
  CHECK(d.errors.load() == 0);
}

TEST(a_pooled_field_mirrors_the_block_once_the_scene_is_kept) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image plain = frame(d, s, out, 1, s.plain, false);
  // Nothing kept yet: the mirror reads zeros and adds nothing.
  const ge::gpu::Image first = frame(d, s, out, 1, s.mirrored, true);
  CHECK(same(plain, first));
  const ge::gpu::Image second = frame(d, s, out, 1, s.mirrored, true);
  // The block mirrored in the pooled ground under the horizon.
  CHECK(changed(plain, second, kH / 2, kH, 8) > 20);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}

TEST(a_dry_field_is_the_same_with_the_mirror_bound_or_not) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image plain = frame(d, s, out, 0, s.plain, false);
  frame(d, s, out, 0, s.mirrored, true);
  const ge::gpu::Image kept = frame(d, s, out, 0, s.mirrored, true);
  CHECK(same(plain, kept));
  CHECK(d.errors.load() == 0);
}
