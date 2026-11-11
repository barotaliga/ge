// engine/render/water.hpp: the detail band carries the spectrum on past the simulation's waves
// (its components shorter than the laid band's, its slope variance growing with the wind, the
// same components for the same seed); the chop runs every way with the slope asked; the surface
// over a flooded passage (render/underwater.hpp's level_over) slides from one pool's to the
// other's; and on a device, a sea drawn over a sea floor fills the lower half of the frame with
// water, darker than the sky over it, with no GPU error and no pipeline after the load; and the
// water's pace puts its motion on a slower clock (a sea at 0.7 of it ten seconds in is the sea
// seven seconds in).
//
// ge:summary Tests for the water's surface drawn: the detail band, a sea on a device
// ge:keywords test render water detail-waves spectrum-tail slope-variance gpu sea chop level_over
// ge:keywords water-pace set_water_pace
// ge:skill render water testing
#include <algorithm>
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
#include "render/underwater.hpp"
#include "render/water.hpp"
#include "water/world.hpp"
#include "wind/field.hpp"

namespace {

namespace r = ge::render;
namespace gw = ge::water;

gw::World sea(double wind) {
  ge::wind::Field field;
  field.mean = wind;
  gw::World world(field);
  gw::add_ocean(world, 0);
  return world;
}

}  // namespace

TEST(a_lake_is_drawn_only_inside_its_shore) {
  // An L-shaped lake: its box takes in the square's empty corner, which the mask leaves dry.
  ge::wind::Field field;
  gw::World world(field);
  const std::vector<gw::Point> shore = {{0, 0},     {200, 0},   {200, 100},
                                        {100, 100}, {100, 200}, {0, 200}};
  gw::add_lake(world, shore, 10);
  const gw::Body& body = world.bodies[0];
  const r::ShoreMask m = r::shore_mask(body);
  CHECK(m.cols >= 1 && m.rows >= 1 && m.cols <= r::kShoreMask && m.rows <= r::kShoreMask);
  CHECK(m.texels.size() == static_cast<size_t>(m.cols) * static_cast<size_t>(m.rows));
  const auto at = [&](double x, double z) {
    const int c = std::clamp(static_cast<int>((x - body.box[0]) / m.texel), 0, m.cols - 1);
    const int r = std::clamp(static_cast<int>((z - body.box[1]) / m.texel), 0, m.rows - 1);
    return m.texels[static_cast<size_t>(r) * static_cast<size_t>(m.cols) + static_cast<size_t>(c)];
  };
  CHECK(at(50, 50) == 255 && at(150, 50) == 255 && at(50, 150) == 255);
  CHECK(at(150, 150) == 0 && at(190, 190) == 0);
  CHECK(at(100 + 0.5 * r::kShoreReach, 120) > 0);
  // Grown by a texel round the outline: just past the inner corner is still drawn.
  CHECK(at(100 + 0.5 * m.texel, 100 + 0.5 * m.texel) == 255);
  // Past it the texels tell how near the water the ground is (the wet band's reach), always under
  // the half that draws, and nothing far from it.
  const uint8_t near = at(100 + 3 * m.texel, 100 + 3 * m.texel);
  CHECK(near > 0 && near < 128);
  CHECK(at(100 + 2 * r::kShoreReach, 100 + 2 * r::kShoreReach) == 0);
  // A body with no outline is drawn everywhere.
  const gw::World seas = sea(5);
  const r::ShoreMask open = r::shore_mask(seas.bodies[0]);
  CHECK(open.cols == 1 && open.rows == 1 && open.texels[0] == 255);
}

TEST(the_underwater_presets_are_named_and_differ_where_they_should) {
  r::UnderwaterLook open, mask, murky, classic, pool, kept;
  CHECK(r::underwater_look("eyes-open", open));
  CHECK(r::underwater_look("mask", mask));
  CHECK(r::underwater_look("murky", murky));
  CHECK(r::underwater_look("classic", classic));
  CHECK(r::underwater_look("pool", pool));
  kept.fog = 7;
  CHECK(!r::underwater_look("no-such-look", kept));
  CHECK(kept.fog == 7);                                // an unknown name leaves it alone
  CHECK(mask.blur < open.blur && mask.magnify > 1.3);  // a mask: sharp and a third larger
  CHECK(murky.fog > open.fog);                         // murky: the view closes in
  CHECK(classic.window == 0 && classic.tint_mix > 0);  // classic: no window, a flat tint
  // The real surface's caustics and shafts in every natural look; the classic tint keeps none.
  CHECK(open.caustics == 1 && open.shafts == 1 && classic.caustics == 0 && classic.shafts == 0);
  // A pool under a lit room: lit by the room; the open look is lit by the day. Both see the
  // surface from below (the water pass's underside).
  CHECK(pool.ambient > 0 && pool.window == 1);
  CHECK(open.ambient == 0 && open.window == 1);
}

TEST(the_detail_band_carries_the_spectrum_past_the_simulations_waves) {
  double last = 0;
  for (const double wind : {3.0, 10.0, 20.0}) {
    const gw::World world = sea(wind);
    const gw::Waves& laid = world.bodies[0].waves;
    const r::WaterDetail a = r::detail_waves(laid, r::kWaterDetailWaves, 7);
    const r::WaterDetail b = r::detail_waves(laid, r::kWaterDetailWaves, 7);
    CHECK(static_cast<int>(a.waves.size()) == r::kWaterDetailWaves);
    double longest_sim = 0;
    for (int i = 0; i < laid.count; ++i) longest_sim = std::fmax(longest_sim, laid.c[i].k);
    for (const gw::Component& c : a.waves) CHECK(c.k > laid.c[0].k * 0.5 && c.a >= 0);
    CHECK(a.waves.back().k > longest_sim);  // past the simulation's shortest
    CHECK(a.mss > last);                    // rougher in a stronger wind
    CHECK(a.tail_mss > 0);
    CHECK(a.waves[3].phase == b.waves[3].phase && a.waves[3].kx == b.waves[3].kx);
    last = a.mss;
  }
}

TEST(the_chop_runs_every_way_with_the_slope_asked) {
  const std::vector<gw::Component> chop = r::chop_waves(0.05, r::kWaterChopWaves, 3);
  const std::vector<gw::Component> again = r::chop_waves(0.05, r::kWaterChopWaves, 3);
  CHECK(static_cast<int>(chop.size()) == r::kWaterChopWaves);
  CHECK(r::chop_waves(0, r::kWaterChopWaves, 3).empty());  // a pool no one swims in: none
  double mss = 0, sx = 0, sz = 0;
  for (size_t i = 0; i < chop.size(); ++i) {
    const gw::Component& c = chop[i];
    mss += 0.5 * (c.k * c.a) * (c.k * c.a);
    sx += c.kx / c.k;
    sz += c.kz / c.k;
    // From 1.2 m down to 5 cm, each faster than gravity alone runs it (the ripples' tension).
    CHECK(c.k >= 2 * 3.14159 / 1.2 && c.k <= 2 * 3.14159 / 0.05);
    CHECK(c.w > std::sqrt(9.80665 * c.k));
    CHECK(c.phase == again[i].phase && c.kx == again[i].kx);
  }
  CHECK(std::fabs(mss - 0.05 * 0.05) < 1e-9);
  // Every way at once: the headings' mean is near nothing (one way would sum to 16).
  CHECK(std::sqrt(sx * sx + sz * sz) < 2.0);
}

TEST(the_surface_over_a_flooded_passage_slides_from_one_pool_to_the_other) {
  // Two pools' tops 6 m apart along z, their levels 3 m apart, a passage between them.
  const float tops[2][4] = {{0, 0, 10, 10}, {0, 16, 10, 20}};
  const float levels[2] = {-5, -8};
  CHECK(r::level_over(tops, levels, 2, 5, 5) > -5.001);  // in a top's box: its level
  CHECK(r::level_over(tops, levels, 2, 5, 18) < -7.999);
  CHECK(std::fabs(r::level_over(tops, levels, 2, 5, 13) + 6.5) < 1e-9);  // halfway: between
  double last = r::level_over(tops, levels, 2, 5, 10);
  for (int step = 1; step <= 24; ++step) {
    const double now = r::level_over(tops, levels, 2, 5, 10 + 0.25 * step);
    CHECK(now <= last && last - now < 0.5);  // falling toward the far pool, never at once
    last = now;
  }
}

TEST(the_foam_answers_the_wind_that_grew_the_sea) {
  // A light air whitens next to nothing; past it the cover is the whitecap law's at the sea's own
  // wind, the fold's threshold rises toward 1 (more of the surface folds), and the foam drifts
  // faster.
  const r::WaterFoam calm = r::foam_of(sea(3).bodies[0].waves);
  CHECK(calm.cover < 1e-3 && calm.runup > 0.03);
  double fold = -1e9, drift = 0;
  for (const double wind : {10.0, 15.0, 22.0}) {
    const gw::World world = sea(wind);
    const r::WaterFoam f = r::foam_of(world.bodies[0].waves);
    CHECK(std::fabs(f.wind - wind) < 0.05 * wind);
    CHECK(std::fabs(f.cover - gw::whitecap_cover(f.wind)) < 1e-12);
    CHECK(f.fold > fold && f.fold < 1);
    CHECK(f.drift > drift);
    CHECK(f.hs > 0 && f.runup > 0.7 * f.hs && f.fold_length > 0);
    fold = f.fold;
    drift = f.drift;
  }
  // A lake with no waves laid laps its shore and makes no foam.
  const r::WaterFoam still = r::foam_of(gw::Waves{});
  CHECK(still.cover == 0 && still.fold < -1 && std::fabs(still.runup - 0.03) < 1e-12);
}

TEST(a_sea_draws_under_the_sky) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  constexpr uint32_t kW = 64, kH = 48;
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets = r::create_targets(d, "test", kW, kH);
  wgpu::Buffer uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  wgpu::BindGroupLayout layout = r::frame_layout(d);
  r::Sky sky;
  r::create_sky(d, pipelines, sky, uniforms, layout);
  ge::core::Heightfield field = ge::core::create_heightfield(0, 0, 8, 65, 65);
  for (float& h : field.data) h = -30;
  std::vector<uint8_t> albedo(size_t{65} * 65 * 4, 180);
  r::TerrainSource src;
  src.height = &field;
  src.albedo = &albedo;
  src.levels = 6;
  r::Terrain terrain;
  r::create_terrain(d, pipelines, terrain, src, layout);
  r::Water water;
  r::create_water(d, pipelines, water, layout);
  r::Post post;
  r::create_post(d, pipelines, post, layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(pipelines.finish(d));
  ge::gpu::end_load(d);
  wgpu::BindGroup frame = r::frame_group(d, layout, uniforms, sky);
  r::create_water_targets(d, water, targets);
  const gw::World world = sea(8);
  r::add_water_body(d, water, world.bodies[0]);
  wgpu::BindGroup pg = r::post_group(d, post, targets.hdr_view);
  r::FrameRecorder rec;
  rec.init(d, "test.timer", 9);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);

  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 10;
  v.eye[2] = 256;
  v.pitch = -0.1;
  v.sun_elevation = 0.5;
  v.time_s = 2;
  r::FrameUniforms staged{};
  rec.begin_frame(d);
  r::fill_uniforms(staged, v, kW, kH);
  d.queue.WriteBuffer(uniforms, 0, &staged, sizeof staged);
  r::prepare_sky(sky, v);
  r::prepare_terrain(d, terrain, v);
  r::prepare_water(d, water, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(sky, pipelines, rec, e);
  r::encode_terrain_cull(terrain, pipelines, rec, e, frame);
  r::encode_terrain_horizon(terrain, pipelines, rec, e);
  r::encode_terrain_depth(terrain, pipelines, rec, e, targets, frame);
  r::encode_terrain(terrain, pipelines, rec, e, targets, frame);
  r::encode_sky(sky, pipelines, rec, e, targets, frame);
  r::encode_water(water, pipelines, rec, e, targets, frame);
  r::encode_post(post, pipelines, rec, e, frame, pg, out.view);
  rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, out);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
  if (img.rgba.size() != size_t{kW} * kH * 4) return;
  auto lum = [&](uint32_t x, uint32_t y) {
    const size_t i = (size_t{y} * kW + x) * 4;
    return int{img.rgba[i]} + img.rgba[i + 1] + img.rgba[i + 2];
  };
  // The water below the horizon is darker than the sky above it, and bluer than red.
  CHECK(lum(kW / 2, kH - 4) < lum(kW / 2, 2));
  const size_t b = (size_t{kH - 4} * kW + kW / 2) * 4;
  CHECK(img.rgba[b + 2] > img.rgba[b]);
  // The water's depth is in the depth buffer: the distance under the frame's centre-bottom is the
  // sea's, not the sea floor's 30 m further down.
  const std::vector<float> dist = r::read_distance(d, targets, v);
  const float near = dist[size_t{kH - 4} * kW + kW / 2];
  CHECK(near < 60);
}

// The water's pace (WaterDraw::pace) puts its own motion on a slower clock: a sea at 0.7 of its
// pace ten seconds in is the sea at its real pace seven seconds in (the waves' phases, the gusts'
// and the stir's drift all scaled alike), and not the sea at its real pace ten seconds in.
TEST(the_waters_pace_puts_its_motion_on_a_slower_clock) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  constexpr uint32_t kW = 64, kH = 48;
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets = r::create_targets(d, "test", kW, kH);
  wgpu::Buffer uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  wgpu::BindGroupLayout layout = r::frame_layout(d);
  r::Sky sky;
  r::create_sky(d, pipelines, sky, uniforms, layout);
  r::Water water;
  r::create_water(d, pipelines, water, layout);
  r::Post post;
  r::create_post(d, pipelines, post, layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(pipelines.finish(d));
  ge::gpu::end_load(d);
  wgpu::BindGroup frame = r::frame_group(d, layout, uniforms, sky);
  r::create_water_targets(d, water, targets);
  const gw::World world = sea(8);
  r::WaterDraw draw;
  draw.stir = 0.02;
  draw.gusts = 1;
  r::add_water_body(d, water, world.bodies[0], draw);
  wgpu::BindGroup pg = r::post_group(d, post, targets.hdr_view);
  r::FrameRecorder rec;
  rec.init(d, "test.timer", 9);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const auto shot = [&](double pace, double time) {
    r::set_water_pace(d, water, 0, world.bodies[0], pace);
    r::FrameView v;
    v.eye[0] = 256;
    v.eye[1] = 4;
    v.eye[2] = 256;
    v.pitch = -0.5;
    v.sun_elevation = 0.5;
    v.time_s = time;
    r::FrameUniforms staged{};
    rec.begin_frame(d);
    r::fill_uniforms(staged, v, kW, kH);
    d.queue.WriteBuffer(uniforms, 0, &staged, sizeof staged);
    r::prepare_sky(sky, v);
    r::prepare_water(d, water, v);
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    r::encode_sky_tables(sky, pipelines, rec, e);
    r::encode_sky(sky, pipelines, rec, e, targets, frame);
    r::encode_water(water, pipelines, rec, e, targets, frame);
    r::encode_post(post, pipelines, rec, e, frame, pg, out.view);
    rec.end_frame(e);
    wgpu::CommandBuffer c = e.Finish();
    d.queue.Submit(1, &c);
    rec.after_submit(d);
    return ge::gpu::read_now(d, out).rgba;
  };
  const std::vector<uint8_t> slow = shot(0.7, 10), early = shot(1, 7), real = shot(1, 10);
  if (slow.size() != size_t{kW} * kH * 4 || early.size() != slow.size()) return;
  const auto apart = [&](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    long sum = 0;
    for (size_t i = 0; i < a.size(); ++i) sum += std::abs(int{a[i]} - int{b[i]});
    return sum;
  };
  // What the sky's own clock moves between 7 s and 10 s (the water held still at pace 0) is in
  // both differences; past it, the slowed sea is the early one and far from the real one.
  const long frozen = apart(shot(0, 7), shot(0, 10));
  const long same = apart(slow, early) - frozen, other = apart(slow, real) - frozen;
  CHECK(other > 0);
  CHECK(same * 8 < other);
  CHECK(d.errors.load() == 0);
}
