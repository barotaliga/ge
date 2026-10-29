// engine/render/explosion.hpp and explosions.hpp: the presets are found by their ids, and a
// knob set by name lands clamped in its field; a rocket and a grenade differ where they should; a
// fireball swells, rises, cools and turns to smoke, and is gone at the end of its life; its flash
// is white-hot then orange and dies with the fire; a blast throws the same sparks from the same
// seed (a hemisphere off a surface), and a scorch only off a surface; staging keeps the nearest
// kMaxExplosions alive on screen, far to near; and on a device (the software adapter here and on
// CI) a fireball lands in the picture where it is, the ground hides what is under it, and it leaves
// nothing once its life is over, with no validation error.
//
// ge:summary Tests for explosions: looks, knobs, the fireball, flash, births, scorch, the draw
// ge:keywords test render explosion fireball flash sparks scorch stage draw knobs preset
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

r::Blast ground_blast(double x, double z, uint32_t seed) {
  r::Blast b;
  b.at[0] = x;
  b.at[1] = 0;
  b.at[2] = z;
  b.on_surface = true;
  b.seed = seed;
  return b;
}

}  // namespace

TEST(looks_are_found_by_their_ids) {
  for (const char* id : {"explosion", "explosion.rocket", "explosion.grenade", "explosion.small",
                         "explosion.large"}) {
    const r::BlastLook* l = r::blast_look_by_id(id);
    CHECK(l != nullptr && l->id == id);
  }
  CHECK(r::blast_look_by_id("sprite") == nullptr);
  CHECK(&r::default_look() == r::blast_look_by_id("explosion"));
  CHECK(&r::look_of(r::Blast{}) == &r::default_look());
}

TEST(a_knob_set_by_name_lands_clamped) {
  r::BlastLook l = r::default_look();
  CHECK(r::set_knob(l, "fire_time", 1.2) && l.fire_s == 1.2);
  CHECK(r::set_knob(l, "sparks", 70.4) && l.sparks == 70);
  CHECK(r::set_knob(l, "smoke_albedo", 7) && l.smoke_albedo == 0.98);  // clamped
  CHECK(!r::set_knob(l, "colour", 1));
  // Every knob names one field, within its range, and its default sits in that range.
  for (const r::BlastKnob& k : r::blast_knobs()) {
    CHECK((k.number != nullptr) != (k.count != nullptr));
    CHECK(k.lo < k.hi && !k.what.empty());
    const r::BlastLook& d = r::default_look();
    const double v = k.number ? d.*k.number : d.*k.count;
    CHECK(v >= k.lo && v <= k.hi);
  }
}

TEST(a_rocket_and_a_grenade_differ_where_they_should) {
  const r::BlastLook& rocket = *r::blast_look_by_id("explosion.rocket");
  const r::BlastLook& grenade = *r::blast_look_by_id("explosion.grenade");
  r::Blast a = ground_blast(0, 0, 4), b = a;
  a.look = &rocket;
  b.look = &grenade;
  // The rocket burns hotter and brighter, throws more and faster sparks; the grenade throws
  // more debris and leaves paler, thicker, longer smoke.
  CHECK(r::fireball_at(a, 0.05).core_k > r::fireball_at(b, 0.05).core_k);
  CHECK(r::flash_at(a, 0.05).candela > r::flash_at(b, 0.05).candela);
  CHECK(rocket.sparks > grenade.sparks && rocket.spark_speed > grenade.spark_speed);
  CHECK(grenade.debris > rocket.debris);
  CHECK(r::fireball_at(b, 1).smoke > r::fireball_at(a, 1).smoke);
  CHECK(grenade.smoke_albedo > rocket.smoke_albedo);
  CHECK(r::blast_life(b) > r::blast_life(a));
  // A knob moves only what it names: more fire time, the same smoke.
  r::BlastLook slow = r::default_look();
  r::set_knob(slow, "fire_time", 1.5);
  r::Blast c = ground_blast(0, 0, 4), d = c;
  c.look = &slow;
  CHECK(r::fireball_at(c, 1).fire > 3 * r::fireball_at(d, 1).fire);
  CHECK_BITS(r::fireball_at(c, 1).smoke, r::fireball_at(d, 1).smoke);
}

TEST(a_fireball_swells_rises_cools_and_turns_to_smoke) {
  const r::Blast b = ground_blast(0, 0, 3);
  const r::Fireball before = r::fireball_at(b, -0.01);
  CHECK(!before.alive);
  const r::Fireball f0 = r::fireball_at(b, 0.01), f1 = r::fireball_at(b, 0.2),
                    f2 = r::fireball_at(b, 1.5);
  CHECK(f0.alive && f1.alive && f2.alive);
  CHECK(f0.radius_m < f1.radius_m && f1.radius_m < f2.radius_m);
  CHECK(f1.radius_m > 0.9 * r::look_of(b).radius_m);  // swollen in a fifth of a second
  CHECK(f0.centre[1] < f2.centre[1]);                 // it rises
  CHECK(f0.centre[1] > 0.4);                          // off the floor by a third of its radius
  CHECK(f0.core_k > f1.core_k && f1.core_k > f2.core_k);
  CHECK(f0.fire > f2.fire && f2.smoke > f0.smoke);
  CHECK(!r::fireball_at(b, r::blast_life(b) + 0.01).alive);
  // Pure: the same blast and age give the same bits.
  const r::Fireball again = r::fireball_at(b, 0.2);
  CHECK_BITS(again.radius_m, f1.radius_m);
  CHECK_BITS(again.core_k, f1.core_k);
}

TEST(the_flash_is_white_hot_then_orange_and_dies_with_the_fire) {
  const r::Blast b = ground_blast(0, 0, 3);
  const r::Flash early = r::flash_at(b, 0.03), late = r::flash_at(b, 0.6);
  CHECK(early.candela > 1e5);
  CHECK(late.candela < early.candela * 0.05);
  CHECK(early.colour[2] / early.colour[0] > late.colour[2] / std::max(late.colour[0], 1e-6f));
  CHECK(r::flash_at(b, r::blast_life(b) + 1).candela == 0);
  CHECK(early.range_m == r::look_of(b).light_range_m);
}

TEST(a_blast_throws_the_same_sparks_from_the_same_seed) {
  const r::Blast b = ground_blast(5, 7, 11);
  std::vector<r::Spark> a(r::blast_births_max(r::look_of(b), b.scale)), c(a.size());
  const size_t n = r::blast_births(b, a);
  CHECK(n == a.size());
  CHECK(r::blast_births(b, c) == n);
  int sparks = 0;
  for (size_t i = 0; i < n; ++i) {
    CHECK_BITS(a[i].v[0], c[i].v[0]);
    CHECK_BITS(a[i].p[2], c[i].p[2]);
    // Off the floor nothing is thrown into it.
    CHECK(a[i].v[1] > -1e-9);
    sparks += a[i].kind == r::SparkKind::spark ? 1 : 0;
  }
  CHECK(sparks == static_cast<int>(r::look_of(b).sparks));
  // In the open air: sparks every way.
  r::Blast air = b;
  air.on_surface = false;
  std::vector<r::Spark> o(r::blast_births_max(r::look_of(air), air.scale));
  const size_t m = r::blast_births(air, o);
  bool down = false;
  for (size_t i = 0; i < m; ++i) {
    down = down || o[i].v[1] < -1;
  }
  CHECK(down);
  // A different seed throws differently; a bigger blast throws more.
  r::Blast other = b;
  other.seed = 12;
  std::vector<r::Spark> d(a.size());
  r::blast_births(other, d);
  CHECK(d[0].v[0] != a[0].v[0]);
  CHECK(r::blast_births_max(r::default_look(), 2) > r::blast_births_max(r::default_look(), 1));
  // Into the particle pool: a spark glows; debris falls whole.
  const r::Particle spark = r::spark_particle(a[0]);
  CHECK(spark.kelvin > 1900 && spark.life > 0);
  const r::Particle chunk = r::spark_particle(a[n - 1]);
  CHECK(chunk.fall == 1 && chunk.opacity == 1);
}

TEST(a_scorch_only_off_a_surface) {
  r::Blast b = ground_blast(1, 2, 9);
  b.scale = 2;
  r::Scorch s{};
  CHECK(r::scorch_of(b, s));
  CHECK(s.size_m == 2 * r::look_of(b).scorch_m);
  CHECK(s.variant < r::kScorchVariants);
  CHECK(s.normal[1] == 1 && s.at[0] == 1);
  r::BlastLook clean = r::default_look();
  r::set_knob(clean, "scorch", 0);
  b.look = &clean;
  CHECK(!r::scorch_of(b, s));  // a look that leaves none
  b.look = nullptr;
  b.on_surface = false;
  CHECK(!r::scorch_of(b, s));
}

TEST(staging_keeps_the_nearest_on_screen_far_to_near) {
  r::Explosions ex;
  ex.staged.assign(r::kMaxExplosions, r::GpuBlast{});
  ex.order.reserve(256);
  r::FrameView v;
  v.eye[1] = 2;  // looking along +z
  std::vector<r::Blast> blasts;
  blasts.reserve(42);
  for (int i = 0; i < 40; ++i) blasts.push_back(ground_blast(0, 10 + 2.0 * i, i));
  blasts.push_back(ground_blast(0, -30, 99));  // behind the eye
  r::Blast later = ground_blast(0, 5, 100);
  later.born_s = 1;  // not yet
  blasts.push_back(later);
  v.time_s = 0.2;
  r::stage_explosions(ex, blasts, v, 160, 90);
  CHECK(ex.drawn == r::kMaxExplosions);
  CHECK(ex.culled == 41 - r::kMaxExplosions);  // the one behind and the farthest eight
  for (uint32_t i = 1; i < ex.drawn; ++i)
    CHECK(ex.staged[i].centre[2] < ex.staged[i - 1].centre[2]);
  CHECK(ex.staged[ex.drawn - 1].centre[2] < 11);  // the nearest is drawn last
  // An eye inside a ball draws it on the whole screen.
  v.eye[2] = 10;
  v.eye[1] = 1;
  r::stage_explosions(ex, std::vector<r::Blast>{ground_blast(0, 10, 1)}, v, 160, 90);
  CHECK(ex.drawn == 1 && ex.staged[0].more[0] == 1.0f);
}

namespace {

constexpr uint32_t kW = 96, kH = 64;

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
  r::Explosions explosions;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);  // flat at 0, 512 m
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
  r::create_explosions(d, s.pipelines, s.explosions, s.layout);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  r::bind_explosion_targets(d, s.explosions, s.targets);
  s.rec.init(d, "test.timer", 12);
}

ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out,
                     const std::vector<r::Blast>& blasts, double t) {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 2;
  v.eye[2] = 100;
  v.pitch = -0.05;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  v.time_s = t;
  s.rec.begin_frame(d);
  s.history.valid = false;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::prepare_explosions(d, s.explosions, blasts, v, kW, kH);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_explosions(s.explosions, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

double luma(const ge::gpu::Image& im, uint32_t x, uint32_t y) {
  const size_t i = (static_cast<size_t>(y) * im.width + x) * 4;
  return 0.2126 * im.rgba[i] + 0.7152 * im.rgba[i + 1] + 0.0722 * im.rgba[i + 2];
}

}  // namespace

TEST(a_fireball_lands_where_it_is_and_is_gone_after_its_life) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const std::vector<r::Blast> one = {ground_blast(256, 112, 5)};
  const ge::gpu::Image bare = frame(d, s, out, {}, 0.1);
  const ge::gpu::Image fire = frame(d, s, out, one, 0.1);
  // Brighter in the middle of the picture, where a ball 12 m ahead stands on the ground.
  double changed_mid = 0, changed_edge = 0;
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      const double dl = std::fabs(luma(fire, x, y) - luma(bare, x, y));
      if (x > kW / 2 - 8 && x < kW / 2 + 8) changed_mid += dl;
      if (x < 8 || x > kW - 8) changed_edge += dl;
    }
  }
  CHECK(changed_mid > 200);
  CHECK(changed_edge < 1);
  CHECK(luma(fire, kW / 2, kH / 2 + 4) > luma(bare, kW / 2, kH / 2 + 4) + 20);
  // Past its life: the same picture as none at all.
  const ge::gpu::Image after = frame(d, s, out, one, 10);
  CHECK(s.explosions.drawn == 0);
  CHECK(after.rgba == bare.rgba);
  CHECK(d.errors.load() == 0);
}
