// engine/render/precip.hpp on a device (the software adapter here and on CI), over a flat field:
// the first frame gathers every particle into the box round the eye; a frame carries them on the
// wind and down at a flake's pace; one the wind carries out of the box comes back in at its far
// side; a rain share turns that share of them to drops, which fall at the drop's pace; a fall of
// `f` draws that share of the particles and a splash share that share of the splashes; and a fall
// (snow, or rain with its splashes) changes the picture where nothing at all is drawn without
// one, with no validation error.
//
// ge:summary Tests for what falls: the box round the eye, the wind, the wrap, drops, splashes
// ge:keywords test render precip snowfall rain drops splashes flakes box wrap wind fall draw
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
#include "render/precip.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;
constexpr uint32_t kFlakes = 4096;
constexpr float kBox = 20.0f;
constexpr double kStill[3] = {0, 0, 0};

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
  r::Precip precip;
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
  r::PrecipSettings ss;
  ss.particles = kFlakes;
  ss.box = kBox;
  ss.size = 0.05f;  // big enough to see at this size
  r::create_precip(d, s.pipelines, s.precip, s.layout, s.terrain, ss);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  r::bind_precip_targets(d, s.precip, s.targets);
  s.rec.init(d, "test.timer", 12);
}

r::FrameView ground_view() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 2;
  v.eye[2] = 100;
  v.heading = 0;  // along +z
  v.pitch = -0.05;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  return v;
}

// One frame: the particles' step of `dt` s in `wind` at a fall of `fall` with a rain share of
// `rain` (and as many splashes), then the picture.
ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out, double dt,
                     double fall, const double wind[3], double rain = 0) {
  const r::FrameView v = ground_view();
  s.rec.begin_frame(d);
  s.history.valid = false;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::PrecipFrame at;
  for (int k = 0; k < 3; ++k) {
    at.eye[k] = v.eye[k];
    at.wind[k] = wind[k];
  }
  at.fall = fall;
  at.rain = rain;
  at.splash = rain;
  at.drop_speed = 6;
  at.time = 3.5;
  at.dt = dt;
  at.near_m = v.near_m;
  r::prepare_precip(d, s.precip, at);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_precip_sim(s.precip, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_precip(s.precip, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

}  // namespace

TEST(a_fall_draws_its_share_of_the_particles_and_the_splashes) {
  r::PrecipSettings s;
  s.particles = 1000;
  s.splashes = 400;
  CHECK(r::particles_drawn(s, 0) == 0);
  CHECK(r::particles_drawn(s, 0.25) == 250);
  CHECK(r::particles_drawn(s, 1) == 1000);
  CHECK(r::particles_drawn(s, 3) == 1000);
  CHECK(r::splashes_drawn(s, 0) == 0);
  CHECK(r::splashes_drawn(s, 0.5) == 200);
  CHECK(r::splashes_drawn(s, 2) == 400);
}

TEST(the_flakes_gather_round_the_eye_and_ride_the_wind) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const r::FrameView v = ground_view();
  frame(d, s, out, 0, 1, kStill);
  const std::vector<float> at = r::read_particles(d, s.precip);
  for (size_t i = 0; i < at.size(); i += 4) {
    for (int k = 0; k < 3; ++k) {
      CHECK(std::fabs(at[i + k] - static_cast<float>(v.eye[k])) <= kBox * 0.5f + 1e-3f);
    }
  }
  // A frame in a 6 m/s wind along +x: every flake carried about 0.1 m along it (its flutter
  // a few hundredths either way) and down by its own pace, 0.6 to 1.4 m/s.
  const double wind[3] = {6, 0, 0};
  frame(d, s, out, 1.0 / 60, 1, wind);
  const std::vector<float> next = r::read_particles(d, s.precip);
  double dx = 0, dy = 0;
  int wrapped = 0;
  for (size_t i = 0; i < at.size(); i += 4) {
    const double mx = next[i] - at[i];
    if (std::fabs(next[i + 1] - at[i + 1]) > kBox / 2) continue;  // fell out of the box's floor
    if (std::fabs(mx) > kBox / 2) {
      ++wrapped;  // carried out of the box's +x face and back in at its -x face
      CHECK(mx < 0);
      continue;
    }
    dx += mx;
    dy += next[i + 1] - at[i + 1];
    CHECK(mx > 0.1 - 0.02 && mx < 0.1 + 0.02);
  }
  double n = 0;
  for (size_t i = 0; i < at.size(); i += 4) {
    if (std::fabs(next[i] - at[i]) <= kBox / 2 && std::fabs(next[i + 1] - at[i + 1]) <= kBox / 2)
      ++n;
  }
  CHECK(std::fabs(dx / n - 0.1) < 0.005);
  CHECK(dy / n < -0.6 / 60 && dy / n > -1.4 / 60);
  CHECK(wrapped > 0);
  CHECK(d.errors.load() == 0);
}

TEST(a_fall_shows_and_none_draws_nothing) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image none = frame(d, s, out, 0, 0, kStill);
  const ge::gpu::Image again = frame(d, s, out, 0, 0, kStill);
  CHECK(none.rgba == again.rgba);
  const ge::gpu::Image full = frame(d, s, out, 0, 1, kStill);
  int changed = 0;
  for (size_t i = 0; i + 3 < full.rgba.size(); i += 4) {
    if (full.rgba[i] != none.rgba[i] || full.rgba[i + 1] != none.rgba[i + 1] ||
        full.rgba[i + 2] != none.rgba[i + 2]) {
      ++changed;
    }
  }
  CHECK(changed > 50);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}

TEST(the_rain_share_turns_particles_to_drops_that_fall_at_the_drops_pace) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  frame(d, s, out, 0, 1, kStill, 0.5);
  const std::vector<float> at = r::read_particles(d, s.precip);
  frame(d, s, out, 1.0 / 60, 1, kStill, 0.5);
  const std::vector<float> next = r::read_particles(d, s.precip);
  // Drops fall at 0.75 to 1.25 of 6 m/s, flakes at 0.6 to 1.4 m/s (and a few hundredths of
  // flutter and eddies): two separate bands, about half the particles in each.
  int drops = 0, flakes = 0;
  for (size_t i = 0; i < at.size(); i += 4) {
    const double dy = (next[i + 1] - at[i + 1]) * 60;
    if (std::fabs(dy) > kBox * 30) continue;  // wrapped through the box's floor
    if (dy < -4.4 && dy > -7.6) ++drops;
    if (dy < -0.3 && dy > -1.8) ++flakes;
  }
  const int n = static_cast<int>(at.size() / 4);
  CHECK(drops + flakes > n * 0.97);
  CHECK(std::abs(drops - n / 2) < n / 20);
  CHECK(d.errors.load() == 0);
}

TEST(rain_and_its_splashes_show_with_no_error) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image none = frame(d, s, out, 0, 0, kStill);
  const ge::gpu::Image rain = frame(d, s, out, 0, 1, kStill, 1);
  CHECK(s.precip.splashed > 0);
  int changed = 0;
  for (size_t i = 0; i + 3 < rain.rgba.size(); i += 4) {
    if (rain.rgba[i] != none.rgba[i] || rain.rgba[i + 1] != none.rgba[i + 1] ||
        rain.rgba[i + 2] != none.rgba[i + 2]) {
      ++changed;
    }
  }
  CHECK(changed > 20);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}
