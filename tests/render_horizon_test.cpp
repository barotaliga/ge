// engine/render/horizon.hpp: the bearing index wraps and splits a turn into equal steps; the
// march runs from one cell to the range; the CPU reference sees a wall's top and the curvature
// of the ground; the bake decision keeps the two bearings either side of the sun, bakes both on
// the first frame and one each time the sun crosses into the next step, none while it stays;
// and on a device, the GPU's bake agrees with the CPU reference texel by texel.
//
// ge:summary Tests for the horizon map: bearings, the march, the reference, the bake decision,
// the GPU bake
// ge:keywords test render horizon-map bearing march bake-decision gpu-bake reference
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/horizon.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;
constexpr double kPi = 3.14159265358979323846;

// A flat field 4 m a sample with a wall 40 m high along x = 300 m.
ge::core::Heightfield wall_field() {
  ge::core::Heightfield f = ge::core::create_heightfield(0, 0, 4, 129, 129);
  for (int row = 0; row < f.rows; ++row) {
    for (int col = 0; col < f.cols; ++col) {
      f.data[static_cast<size_t>(row) * f.cols + col] = col * 4 >= 300 ? 40.0f : 0.0f;
    }
  }
  return f;
}

}  // namespace

TEST(bearing_index_wraps_and_splits_a_turn) {
  double f = -1;
  CHECK(r::bearing_index(0, &f) == 0 && f == 0);
  CHECK(r::bearing_index(r::kHorizonBearingStep * 2.5, &f) == 2);
  CHECK(std::fabs(f - 0.5) < 1e-9);
  CHECK(r::bearing_index(-r::kHorizonBearingStep * 0.5, &f) == r::kHorizonBearings - 1);
  CHECK(std::fabs(f - 0.5) < 1e-9);
  CHECK(r::bearing_index(2 * kPi + r::kHorizonBearingStep * 3.25, &f) == 3);
  CHECK(r::kHorizonBearings % 2 == 0);
}

TEST(march_runs_from_one_cell_to_the_range) {
  const r::HorizonMarch m = r::horizon_march(8, 8000);
  CHECK(m.first_m == 8);
  const double last = m.first_m * std::pow(m.ratio, r::kHorizonSteps - 1);
  CHECK(std::fabs(last - 8000) < 1e-6);
}

TEST(reference_sees_the_wall_and_the_curve) {
  const ge::core::Heightfield f = wall_field();
  const r::HorizonMarch m = r::horizon_march(4, 200);
  // From 100 m west of the wall, looking east (heading 90 degrees): the wall's top 40 m up at
  // 100 m or a little further (the march's steps straddle it).
  const double east = r::horizon_tan(f, 200, 256, kPi / 2, m);
  CHECK(east > 0.3 && east <= 0.4 + 1e-9);
  // Looking west over flat ground: nothing above, the curve's drop below zero.
  const double west = r::horizon_tan(f, 200, 256, 3 * kPi / 2, m);
  CHECK(west <= 0 && west > -1e-4);
  // On top of the wall looking west: down only.
  CHECK(r::horizon_tan(f, 400, 256, 3 * kPi / 2, m) < 0);
}

TEST(bake_decision_keeps_the_bearings_either_side) {
  r::Horizon h;
  r::FrameView v;
  const double step = r::kHorizonBearingStep;
  v.sun_azimuth = step * 10.3;
  r::prepare_horizon(nullptr, h, v);
  CHECK(h.bakes == 2);
  CHECK(h.held[0] == 10 && h.held[1] == 11);
  CHECK(std::fabs(h.blend[0] - 0.3f) < 1e-5f && h.blend[1] == 0.0f);
  v.sun_azimuth = step * 10.9;  // the same step: nothing to bake
  r::prepare_horizon(nullptr, h, v);
  CHECK(h.bakes == 2);
  v.sun_azimuth = step * 11.2;  // the next step: one new map (12), into slot 0
  r::prepare_horizon(nullptr, h, v);
  CHECK(h.bakes == 3);
  CHECK(h.held[0] == 12 && h.held[1] == 11);
  CHECK(h.blend[1] == 1.0f);
  // A sweep through a whole turn bakes one map a step.
  for (int k = 12; k < 12 + r::kHorizonBearings; ++k) {
    v.sun_azimuth = step * (k + 0.5);
    r::prepare_horizon(nullptr, h, v);
  }
  CHECK(h.bakes == 3 + r::kHorizonBearings);
}

TEST(gpu_bake_matches_the_reference) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  // Rolling hills with a ridge, so every texel has a horizon worth comparing.
  ge::core::Heightfield f = ge::core::create_heightfield(-100, 50, 4, 97, 81);
  for (int row = 0; row < f.rows; ++row) {
    for (int col = 0; col < f.cols; ++col) {
      f.data[static_cast<size_t>(row) * f.cols + col] =
          static_cast<float>(15 * std::sin(col * 0.11) * std::cos(row * 0.07) + 0.3 * col);
    }
  }
  std::vector<uint8_t> albedo(static_cast<size_t>(f.cols) * f.rows * 4, 200);
  ge::gpu::PipelineSet pipelines;
  const wgpu::BindGroupLayout frame = r::frame_layout(d);
  r::Terrain t;
  r::TerrainSource src;
  src.height = &f;
  src.albedo = &albedo;
  src.levels = 4;
  src.horizon.stride = 2;
  src.horizon.range_m = 600;
  r::create_terrain(d, pipelines, t, src, frame);
  CHECK(pipelines.finish(d));
  ge::gpu::end_load(d);
  r::FrameRecorder rec;
  rec.init(d, "test.timer", 1);
  r::FrameView v;
  v.sun_azimuth = 1.0;
  rec.begin_frame(d);
  r::prepare_terrain(d, t, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_terrain_horizon(t, pipelines, rec, e);
  rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  CHECK(t.horizon.width == 49 && t.horizon.height == 41);
  CHECK(rec.stats.dispatches() == 2);
  for (int slot = 0; slot < 2; ++slot) {
    const std::vector<float> map = r::read_horizon(d, t.horizon, slot);
    const double bearing = r::bearing_of(t.horizon.held[slot]);
    int worst = 0;
    for (int y = 0; y < t.horizon.height; y += 5) {
      for (int x = 0; x < t.horizon.width; x += 6) {
        const double want =
            r::horizon_tan(f, t.horizon.origin_x + x * t.horizon.cell,
                           t.horizon.origin_z + y * t.horizon.cell, bearing, t.horizon.march);
        const double got = map[static_cast<size_t>(y) * t.horizon.width + x];
        if (std::fabs(got - want) > 2e-3 + 2e-3 * std::fabs(want)) ++worst;
      }
    }
    CHECK(worst == 0);
  }
  CHECK(d.errors.load() == 0);
}
