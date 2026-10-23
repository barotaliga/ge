// engine/render/particles.hpp: the flight's CPU twin falls by its share of gravity and drags
// toward the air (still, or a wind); on a device (the software adapter here and on CI) the
// "particles.sim" pass flies a particle as the twin does, buries a grain that comes down on the
// ground, settles a fine one on it, lets one rising out of the snow fly on, leaves a dead slot dead
// and holds every particle still on a step of 0; births wrap the ring, the newest replacing the
// oldest; and the draw puts a cloud of grains into the picture where they are and nowhere else,
// with no validation error.
//
// ge:summary Tests for GPU particles: the flight and its twin, the ground, the ring, the draw
// ge:keywords test render particles step_particle ring births sim ground settle draw
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;
constexpr uint32_t kSlots = 256;

bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

r::Particle grain(float x, float y, float z, float vx, float vy, float vz) {
  r::Particle p{};
  p.p[0] = x;
  p.p[1] = y;
  p.p[2] = z;
  p.v[0] = vx;
  p.v[1] = vy;
  p.v[2] = vz;
  p.life = 2;
  p.size = 0.05f;
  p.drag = 0.9f;
  p.fall = 1;
  p.streak = 0.04f;
  p.albedo = 0.92f;
  p.opacity = 0.9f;
  return p;
}

r::Particle fine(float x, float y, float z, float vy) {
  r::Particle p = grain(x, y, z, 0.4f, vy, 0);
  p.fall = 0.08f;
  p.drag = 1.6f;
  p.size = 0.4f;
  p.opacity = 0.2f;
  return p;
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
  r::Particles particles;
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
  r::ParticleSettings ps;
  ps.capacity = kSlots;
  r::create_particles(d, s.pipelines, s.particles, s.layout, s.terrain, ps);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  r::bind_particle_targets(d, s.particles, s.targets);
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

// One frame: the particles' step of `dt` s, then the picture with them or without.
ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out, double dt,
                     bool draw_particles) {
  const r::FrameView v = ground_view();
  s.rec.begin_frame(d);
  s.history.valid = false;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::prepare_particles(d, s.particles, dt, v.near_m);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_particles_sim(s.particles, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  if (draw_particles) {
    r::encode_particles(s.particles, s.pipelines, s.rec, e, s.targets, s.frame);
  }
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

}  // namespace

TEST(the_twin_falls_by_its_share_of_gravity_and_drags) {
  r::Particle p = grain(0, 10, 0, 2, 0, 0);
  p.drag = 0;
  r::step_particle(p, 0.5f);
  CHECK(near(p.v[1], -9.81 * 0.5, 1e-5));
  CHECK(near(p.p[1], 10 - 9.81 * 0.25, 1e-4));  // semi-implicit: the new speed moves it
  CHECK(near(p.p[0], 1, 1e-6));
  CHECK(near(p.age, 0.5, 1e-7));
  r::Particle f = fine(0, 10, 0, 0);
  r::step_particle(f, 0.5f);
  CHECK(near(f.v[1], -9.81 * 0.08 * 0.5 * std::exp(-1.6 * 0.5), 1e-5));
  CHECK(near(f.v[0], 0.4 * std::exp(-1.6 * 0.5), 1e-6));  // drag toward the still air
  // In a wind the drag pulls toward the air's velocity instead: a fine drifts off downwind.
  const float air[3] = {5, 0, 0};
  r::Particle w = fine(0, 10, 0, 0);
  for (int i = 0; i < 300; ++i) r::step_particle(w, 1.0f / 60, air);
  CHECK(near(w.v[0], 5, 0.05));
}

TEST(the_sim_flies_as_the_twin_does) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const std::vector<r::Particle> born = {grain(250, 5, 120, 1, 2, -0.5f), fine(260, 3, 130, 0.3f)};
  r::emit_particles(s.particles, born);
  const float dt = 1.0f / 60;
  frame(d, s, out, dt, false);
  const std::vector<r::Particle> got = r::read_particles(d, s.particles);
  for (size_t i = 0; i < born.size(); ++i) {
    r::Particle want = born[i];
    r::step_particle(want, dt);
    for (int k = 0; k < 3; ++k) {
      CHECK(near(got[i].p[k], want.p[k], 1e-4));
      CHECK(near(got[i].v[k], want.v[k], 1e-4));
    }
    CHECK(near(got[i].age, dt, 1e-6));
  }
  CHECK(r::live_particles(got) == 2);
  // A step of 0 holds them where they are.
  frame(d, s, out, 0, false);
  const std::vector<r::Particle> held = r::read_particles(d, s.particles);
  CHECK(held[0].p[1] == got[0].p[1] && held[0].age == got[0].age);
  CHECK(d.errors.load() == 0);
}

TEST(the_ground_buries_a_grain_and_settles_a_fine) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const std::vector<r::Particle> born = {grain(250, 0.01f, 120, 0, -3, 0),
                                         fine(260, 0.005f, 130, -2),
                                         grain(270, -0.05f, 140, 0, 3, 0)};
  r::emit_particles(s.particles, born);
  frame(d, s, out, 1.0 / 60, false);
  const std::vector<r::Particle> got = r::read_particles(d, s.particles);
  CHECK(got[0].age >= got[0].life);  // the grain is gone
  CHECK(got[1].age < got[1].life);   // the fine lies on the snow, fading
  CHECK(near(got[1].p[1], 0, 1e-5));
  CHECK(got[1].v[1] == 0);
  CHECK(got[1].age >= got[1].life * 0.7f);
  CHECK(got[2].age < got[2].life);  // a grain rising out of the snow it was thrown from flies on
  CHECK(got[2].p[1] > -0.05f);
  CHECK(r::live_particles(got) == 2);
  CHECK(d.errors.load() == 0);
}

TEST(births_wrap_the_ring_and_a_clear_kills_every_slot) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  std::vector<r::Particle> born;
  for (uint32_t k = 0; k < kSlots + 10; ++k) {
    born.push_back(grain(200 + 0.1f * static_cast<float>(k), 20, 150, 0, 0, 0));
  }
  r::emit_particles(s.particles, born);
  frame(d, s, out, 0, false);
  std::vector<r::Particle> got = r::read_particles(d, s.particles);
  CHECK(r::live_particles(got) == kSlots);
  // The first 10 births were overwritten by the last 10 through the ring: slot 0 holds birth 10.
  CHECK(near(got[0].p[0], 201, 1e-4));
  CHECK(s.particles.head == 0);
  r::emit_particles(s.particles, std::vector<r::Particle>{grain(0, 20, 0, 0, 0, 0)});
  frame(d, s, out, 0, false);
  got = r::read_particles(d, s.particles);
  CHECK(got[0].p[0] == 0);  // the oldest slot went first
  CHECK(s.particles.head == 1);
  r::clear_particles(d, s.particles);
  got = r::read_particles(d, s.particles);
  CHECK(r::live_particles(got) == 0);
  CHECK(d.errors.load() == 0);
}

TEST(the_draw_puts_the_cloud_where_it_is) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image bare = frame(d, s, out, 0, true);
  // A cloud of grains 8 m ahead, at the eye's height, a metre across.
  std::vector<r::Particle> cloud;
  for (int i = 0; i < 12; ++i) {
    for (int j = 0; j < 12; ++j) {
      r::Particle p = grain(255.5f + 0.09f * static_cast<float>(i),
                            1.5f + 0.09f * static_cast<float>(j), 108, 0, 0, 0);
      p.size = 0.12f;
      p.age = 0.5f;  // past its fade-in
      cloud.push_back(p);
    }
  }
  r::emit_particles(s.particles, cloud);
  const ge::gpu::Image with = frame(d, s, out, 0, true);
  // The middle of the picture changed, its corners did not.
  int changed_middle = 0, changed_edge = 0;
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      const size_t i = (static_cast<size_t>(y) * kW + x) * 4;
      const bool diff = with.rgba[i] != bare.rgba[i] || with.rgba[i + 1] != bare.rgba[i + 1] ||
                        with.rgba[i + 2] != bare.rgba[i + 2];
      const bool middle = x > kW / 4 && x < kW * 3 / 4 && y > kH / 6 && y < kH * 5 / 6;
      if (diff) ++(middle ? changed_middle : changed_edge);
    }
  }
  CHECK(changed_middle > 20);
  CHECK(changed_edge == 0);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}
