// engine/render/grass.hpp: on a device (the software adapter here and on CI) the scatter grows
// exactly the blades engine/grass scatters (every one the GPU keeps is the CPU's, to the float's
// last bits but for rounding, and all of a cover in view are kept when nothing is thinned); the
// distance thins them and splits the fine list from the coarse; the GPU's blade in the wind is the
// CPU's `blade_shape` in the CPU's `wind_at_blade`, and a trampled one the CPU's under the same
// cells' `press_at`; the draw puts green blades into the picture where the cover is, with no
// validation error; and a blade that sways between frames carries its own motion to TAA.
//
// ge:summary Tests for the grass drawn: the GPU scatter against the CPU's, thinning, shapes, draw
// ge:keywords test render grass scatter thinning shape probe draw wind trample press motion
// ge:skill render grass testing
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "grass/cover.hpp"
#include "grass/sway.hpp"
#include "grass_golden.hpp"
#include "render/frame.hpp"
#include "render/grass.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"
#include "wind/field.hpp"

namespace {

namespace r = ge::render;
namespace gg = ge::grass;
using ge::test::grass_preset;

constexpr uint32_t kW = 96, kH = 64;

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
  r::Grass grass;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  gg::Cover cover;
};

// A flat field at y = 0, 512 m, a 12 m square of meadow at (250..262, 100..112).
void make_scene(ge::gpu::Device& d, Scene& s, const r::GrassSettings& gs) {
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);
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
  r::create_grass(d, s.pipelines, s.grass, s.layout, s.terrain, gs);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "test.timer", 12);
  s.cover.kinds.push_back(grass_preset("meadow"));
  s.cover.kinds[0].density = 60;
  gg::cover_heightfield(s.cover, s.field, {250, 100, 262, 112}, 2.0,
                        [](double, double) { return gg::CoverAt{0, 1}; });
  CHECK(r::set_grass_cover(d, s.grass, s.cover));
}

r::FrameView above() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 30;
  v.eye[2] = 94;
  v.heading = 0;
  v.pitch = -1.2;
  v.fov_y = 1.0;
  v.sun_elevation = 0.7;
  v.sun_azimuth = 2.5;
  v.altitude_m = 500;
  return v;
}

ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out,
                     const r::FrameView& v, const gg::GrassAir& air, double t, bool grass) {
  s.rec.begin_frame(d);
  r::fill_uniforms(s.staged, v, kW, kH);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::prepare_grass(d, s.grass, v, air, t);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_grass_scatter(s.grass, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  if (grass) r::encode_grass(s.grass, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

r::GrassSettings keep_all() {
  r::GrassSettings gs;
  gs.capacity = 1u << 15;
  gs.full_m = 1000;
  gs.near_m = 1000;
  gs.reach_m = 1000;
  gs.probe = true;
  return gs;
}

// The motion target read back now: each pixel's two half floats as one word.
std::vector<uint32_t> read_motion(ge::gpu::Device& d, const r::FrameTargets& t) {
  const uint32_t row = ((kW * 8 + 255) / 256) * 256;
  wgpu::BufferDescriptor bd;
  bd.size = uint64_t{row} * kH;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  const wgpu::Buffer buf = d.device.CreateBuffer(&bd);
  wgpu::TexelCopyTextureInfo src;
  src.texture = t.motion;
  wgpu::TexelCopyBufferInfo dst;
  dst.buffer = buf;
  dst.layout.bytesPerRow = row;
  dst.layout.rowsPerImage = kH;
  const wgpu::Extent3D size{kW, kH, 1};
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  e.CopyTextureToBuffer(&src, &dst, &size);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  bool ok = false;
  const wgpu::Future fut =
      buf.MapAsync(wgpu::MapMode::Read, 0, bd.size, wgpu::CallbackMode::AllowProcessEvents,
                   [&ok](wgpu::MapAsyncStatus st, wgpu::StringView) {
                     ok = st == wgpu::MapAsyncStatus::Success;
                   });
  ge::gpu::wait(d, fut);
  CHECK(ok);
  std::vector<uint32_t> out(size_t{kW} * kH, 0);
  if (!ok) return out;
  const auto* bytes = static_cast<const uint8_t*>(buf.GetConstMappedRange(0, bd.size));
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      uint16_t h[2];
      std::memcpy(h, bytes + size_t{y} * row + size_t{x} * 8, sizeof h);
      out[size_t{y} * kW + x] = h[0] | (uint32_t{h[1]} << 16u);
    }
  }
  buf.Unmap();
  return out;
}

}  // namespace

TEST(the_gpu_grows_the_cpus_blades) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, keep_all());
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  frame(d, s, out, above(), {}, 0, false);
  const r::GrassLists got = r::read_grass_blades(d, s.grass);
  const std::vector<gg::Blade> want = gg::scatter_all(s.cover);
  CHECK(got.far.empty());
  // Every blade in view: the few the frustum's edge takes off the patch's near corners aside.
  CHECK_MSG(got.near.size() <= want.size() && got.near.size() * 100 >= want.size() * 99,
            "gpu " + std::to_string(got.near.size()) + " cpu " + std::to_string(want.size()));
  std::map<uint32_t, gg::Blade> by_seed;
  for (const auto& b : want) by_seed[b.seed] = b;
  size_t matched = 0;
  for (const auto& b : got.near) {
    const auto it = by_seed.find(b.seed);
    if (it == by_seed.end()) continue;
    const gg::Blade& w = it->second;
    bool same = std::fabs(b.height - w.height) < 1e-5f && std::fabs(b.width - w.width) < 1e-7f;
    for (int k = 0; k < 3; ++k) same = same && std::fabs(b.root[k] - w.root[k]) < 1e-4f;
    // A GPU's division and square root may be a few ulps off, which turns a lean by a hair.
    for (int k = 0; k < 2; ++k) same = same && std::fabs(b.lean[k] - w.lean[k]) < 2e-3f;
    matched += same ? 1 : 0;
  }
  // A blade all but equidistant from two clumps may join the other on the GPU: a handful.
  CHECK_MSG(matched * 1000 >= got.near.size() * 998, "matched " + std::to_string(matched));
  CHECK(d.errors.load() == 0);
}

TEST(the_distance_thins_and_splits_the_lists) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  r::GrassSettings gs = keep_all();
  gs.full_m = 31;
  gs.near_m = 33;
  make_scene(d, s, gs);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  frame(d, s, out, above(), {}, 0, false);
  const r::GrassLists got = r::read_grass_blades(d, s.grass);
  const size_t all = gg::scatter_all(s.cover).size();
  CHECK(!got.near.empty() && !got.far.empty());
  CHECK(got.near.size() + got.far.size() < all);
  for (const auto& b : got.near) {
    const float dx = b.root[0] - 256, dy = b.root[1] - 30, dz = b.root[2] - 94;
    CHECK(std::sqrt(dx * dx + dy * dy + dz * dz) < 33.001f);
  }
  CHECK(d.errors.load() == 0);
}

TEST(the_gpus_blade_bends_as_the_cpus) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, keep_all());
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  ge::wind::Field f;
  f.mean = 7;
  f.from = 1.2;
  ge::wind::lay_gusts(f, 9, 0.3);
  gg::GrassAir air;
  air.field = &f;
  air.ground = &s.field;
  const double t = 4.75;
  frame(d, s, out, above(), air, t, false);
  const r::GrassLists got = r::read_grass_blades(d, s.grass);
  const std::vector<float> shapes = r::probe_grass_shapes(d, s.pipelines, s.grass, s.frame);
  CHECK(!got.near.empty());
  float worst = 0, worst_air = 0;
  for (size_t i = 0; i < got.near.size(); i += 7) {
    const gg::Blade& b = got.near[i];
    float wind[2];
    gg::wind_at_blade(s.grass.grid, b.root[0], b.root[2], static_cast<float>(t), wind);
    const gg::Shape want =
        gg::blade_shape(s.cover.kinds[gg::kind_of(b)], b, wind, static_cast<float>(t));
    const float* g = &shapes[i * 12];
    worst_air = std::max({worst_air, std::fabs(g[3] - wind[0]), std::fabs(g[7] - wind[1])});
    const float* pts[3] = {want.v0, want.v1, want.v2};
    for (int p = 0; p < 3; ++p) {
      for (int k = 0; k < 3; ++k) worst = std::max(worst, std::fabs(g[p * 4 + k] - pts[p][k]));
    }
  }
  CHECK_MSG(worst_air < 1e-3f, "air off by " + std::to_string(worst_air));
  CHECK_MSG(worst < 2e-3f, "shape off by " + std::to_string(worst) + " m");
  CHECK(d.errors.load() == 0);
}

TEST(the_draw_puts_green_blades_where_the_cover_is) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, keep_all());
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image bare = frame(d, s, out, above(), {}, 0, false);
  const ge::gpu::Image grown = frame(d, s, out, above(), {}, 0, true);
  size_t changed = 0, greener = 0;
  for (size_t i = 0; i < bare.rgba.size(); i += 4) {
    if (bare.rgba[i + 1] != grown.rgba[i + 1]) {
      ++changed;
      greener += grown.rgba[i + 1] > grown.rgba[i] && grown.rgba[i + 1] > grown.rgba[i + 2];
    }
  }
  const size_t pixels = bare.rgba.size() / 4;
  CHECK_MSG(changed > pixels / 50 && changed < pixels / 2, "changed " + std::to_string(changed));
  CHECK_MSG(greener * 10 > changed * 8, "greener " + std::to_string(greener));
  CHECK(d.errors.load() == 0);
}

TEST(the_gpus_trampled_blade_lies_as_the_cpus) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, keep_all());
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  // A path walked across the patch, then a wheel's line down it later.
  gg::Trample tr;
  gg::init_trample(tr, 0.25f, 128);
  for (int i = 0; i < 24; ++i)
    gg::stamp(tr, 250.5f + 0.5f * static_cast<float>(i), 106.0f, 0.4f, 1, 0,
              0.2f * static_cast<float>(i));
  for (int i = 0; i < 30; ++i)
    gg::stamp(tr, 255.0f, 100.5f + 0.4f * static_cast<float>(i), 0.25f, 0, 1, 5.0f);
  r::set_grass_trample(d, s.grass, tr);
  CHECK(tr.dirty_hi <= tr.dirty_lo);
  ge::wind::Field f;
  f.mean = 4;
  f.from = 0.4;
  gg::GrassAir air;
  air.field = &f;
  air.ground = &s.field;
  const double t = 6.5;
  frame(d, s, out, above(), air, t, false);
  const r::GrassLists got = r::read_grass_blades(d, s.grass);
  const std::vector<float> shapes = r::probe_grass_shapes(d, s.pipelines, s.grass, s.frame);
  float worst = 0, worst_press = 0;
  size_t pressed = 0;
  for (size_t i = 0; i < got.near.size(); i += 3) {
    const gg::Blade& b = got.near[i];
    const gg::Kind& k = s.cover.kinds[gg::kind_of(b)];
    float wind[2];
    gg::wind_at_blade(s.grass.grid, b.root[0], b.root[2], static_cast<float>(t), wind);
    const gg::Press p = gg::press_at(tr, k, b.root[0], b.root[2], static_cast<float>(t));
    const gg::Shape want = gg::blade_shape(k, b, wind, static_cast<float>(t), p);
    const float* g = &shapes[i * 12];
    pressed += p.amount > 0.1f ? 1 : 0;
    worst_press = std::max(worst_press, std::fabs(g[11] - p.amount));
    const float* pts[3] = {want.v0, want.v1, want.v2};
    for (int q = 0; q < 3; ++q) {
      for (int k3 = 0; k3 < 3; ++k3) worst = std::max(worst, std::fabs(g[q * 4 + k3] - pts[q][k3]));
    }
  }
  CHECK_MSG(pressed > 50, "pressed " + std::to_string(pressed));
  CHECK_MSG(worst_press < 1e-4f, "press off by " + std::to_string(worst_press));
  CHECK_MSG(worst < 2e-3f, "shape off by " + std::to_string(worst) + " m");
  CHECK(d.errors.load() == 0);
}

TEST(a_swaying_blade_draws_with_motion_and_a_still_one_without) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s, keep_all());
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  ge::wind::Field f;
  f.mean = 6;
  f.from = 1.0;
  ge::wind::lay_gusts(f, 3, 0.4);
  gg::GrassAir air;
  air.field = &f;
  air.ground = &s.field;
  // The camera is still: the clock alone moves the blades, so a frame's motion differs from a
  // frame at the same time's only where a blade is drawn, and there it does.
  const ge::gpu::Image bare = frame(d, s, out, above(), air, 2.0, false);
  const ge::gpu::Image grown = frame(d, s, out, above(), air, 2.0, true);
  const std::vector<uint32_t> held = read_motion(d, s.targets);
  frame(d, s, out, above(), air, 2.0, true);
  const std::vector<uint32_t> again = read_motion(d, s.targets);
  frame(d, s, out, above(), air, 2.05, true);
  const std::vector<uint32_t> moving = read_motion(d, s.targets);
  size_t moved = 0, off_grass = 0;
  for (size_t i = 0; i < held.size(); ++i) {
    if (held[i] == moving[i]) continue;
    ++moved;
    const bool grass = std::memcmp(&bare.rgba[i * 4], &grown.rgba[i * 4], 3) != 0;
    off_grass += grass ? 0 : 1;
  }
  CHECK(held == again);
  CHECK_MSG(moved > 20, "moved " + std::to_string(moved));
  // A blade swaying onto a pixel it did not cover at 2.0 s is the only way off the grass.
  CHECK_MSG(off_grass * 20 < moved, "off the grass " + std::to_string(off_grass));
  CHECK(d.errors.load() == 0);
}
