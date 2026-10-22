// plugins/mountain/app/trails.hpp: the trail map. The fine window centres on its grid and moves
// once the camera leaves its middle half; a stamp's reach is tested against a square. On a device
// (skipped without one): a stamp lands in the fine map with its cross-section (the root of its
// depth on the axis, falling off across it, the berm beside it, the floor pressed), two stamps
// over one texel keep the deeper, a narrow stamp lands in the coarse map faint, more stamps than a
// draw carries are drawn all the same, and a window moved away and back is redrawn from the
// history to the same texels.
//
// ge:summary Tests for the trail map: window moves, stamp reach, max blend, coarse, history redraw
// ge:keywords test trails trail-map window stamp max-blend history redraw coarse fine readback
// ge:skill mountain testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "mountain/app/trails.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/frame.hpp"

namespace {

namespace app = ge::mountain::app;

bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

// A flat 64 m square at 2 m a cell.
ge::mountain::mapgen::Level flat_map() {
  ge::mountain::mapgen::Level level;
  level.cell = 2;
  level.size = 64;
  level.ground = ge::core::create_heightfield(0, 0, 2, 33, 33);
  return level;
}

// One frame of the maps for a camera at (x, z).
void frame_at(ge::gpu::Device& d, const ge::gpu::PipelineSet& pipelines, app::Trails& t,
              ge::render::FrameRecorder& rec, double x, double z) {
  rec.begin_frame(d);
  app::prepare_trails(d, pipelines, t, x, z);
  wgpu::CommandEncoder encoder = d.device.CreateCommandEncoder();
  app::encode_trails(t, pipelines, rec, encoder);
  rec.end_frame(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  d.queue.Submit(1, &commands);
  rec.after_submit(d);
}

// A channel of the read-back map at the texel whose centre is nearest (x, z), 0..255.
int texel(const ge::gpu::Image& img, const app::TrailRect& r, double x, double z, int k) {
  const auto c = static_cast<size_t>(std::floor((x - r.x0) / r.texel));
  const auto w = static_cast<size_t>(std::floor((z - r.z0) / r.texel));
  return img.rgba[(w * img.width + c) * 4 + static_cast<size_t>(k)];
}

int encoded(double depth) { return static_cast<int>(std::lround(std::sqrt(depth) * 255)); }

}  // namespace

TEST(the_window_centres_on_its_grid_and_moves_past_its_middle) {
  const app::TrailWindow w = app::trail_window_at(96, 100, 200);
  CHECK(w.x0 == 48 && w.z0 == 156);
  CHECK(!app::trail_window_moves(w, 96, 100, 200));
  CHECK(!app::trail_window_moves(w, 96, 96 + 23, 204 - 23));
  CHECK(app::trail_window_moves(w, 96, 96 + 25, 204));
  CHECK(app::trail_window_moves(w, 96, 96, 204 - 25));
}

TEST(a_stamp_meets_a_square_with_its_berm) {
  const app::TrailStamp s{10, 10, 12, 10, 0.1f, 0.05f, 0, 2};
  CHECK(app::trail_stamp_meets(s, 0, 0, 16));
  CHECK(app::trail_stamp_meets(s, 12.2, 5, 16));  // its berm reaches past b
  CHECK(!app::trail_stamp_meets(s, 13, 5, 16));
  CHECK(!app::trail_stamp_meets(s, 0, 20, 16));
}

TEST(stamps_land_keep_the_deeper_and_come_back_from_the_history) {
  auto device = ge::test::test_device();
  if (!device) return;
  const ge::mountain::mapgen::Level level = flat_map();
  ge::gpu::PipelineSet pipelines;
  app::Trails t;
  app::TrailOptions options;
  options.fine_size = 256;  // 16 m: a texel 6.25 cm
  options.fine_span = 16;
  options.coarse_size = 64;  // 1 m a texel
  options.capacity = 4;
  app::create_trails(*device, pipelines, t, level, options);
  CHECK(pipelines.finish(*device));
  ge::render::FrameRecorder rec;
  rec.init(*device, "trails.test", 2);
  // A along x = 8 from z = 8 to 12, B over its far half and on to 14 and deeper; six more far
  // off, so the first frame carries more than a draw does.
  std::vector<app::TrailStamp> stamps = {{8, 8, 8, 12, 0.1f, 0.04f, 0.01f, 2},
                                         {8, 10, 8, 14, 0.1f, 0.09f, 0.01f, 2}};
  for (int i = 0; i < 6; ++i) {
    const auto x = static_cast<float>(40 + i);
    stamps.push_back({x, 40, x, 41, 0.1f, 0.05f, 0, 2});
  }
  app::add_trail_stamps(t, stamps);
  frame_at(*device, pipelines, t, rec, 8, 10);
  const app::TrailRect r = app::fine_rect(t);
  CHECK(r.x0 == 0 && r.z0 == 2 && near(r.texel, 0.0625, 1e-9));
  auto check_fine = [&](const ge::gpu::Image& img) {
    // A texel centre 3.125 cm off A's axis: u = 0.3125.
    const double x = 8.03125;
    CHECK(std::abs(texel(img, r, x, 9.03125, 0) - encoded(0.04 * (1 - 0.3125 * 0.3125))) <= 2);
    // Where B lies over A: the deeper.
    CHECK(std::abs(texel(img, r, x, 11.03125, 0) - encoded(0.09 * (1 - 0.3125 * 0.3125))) <= 2);
    CHECK(texel(img, r, x, 9.03125, 2) == 255);  // the floor pressed
    // 15.6 cm off the axis (u = 1.56): no groove, the berm.
    CHECK(texel(img, r, 8.15625, 9.03125, 0) == 0);
    CHECK(texel(img, r, 8.15625, 9.03125, 1) > 0);
    CHECK(texel(img, r, 8.15625, 9.03125, 2) == 0);
    // Nothing a metre off.
    CHECK(texel(img, r, 9.03125, 9.03125, 0) == 0);
  };
  const ge::gpu::Image first = ge::gpu::read_now(*device, t.fine);
  check_fine(first);
  // The coarse map: a stamp a fifth of a texel wide lands at a fifth of its depth.
  const ge::gpu::Image coarse = ge::gpu::read_now(*device, t.coarse);
  const int faint = texel(coarse, t.coarse_rect, 8.5, 12.5, 0);
  CHECK(faint > 0 && faint < encoded(0.09));
  CHECK(texel(coarse, t.coarse_rect, 40.5, 40.5, 0) > 0);  // a far stamp, drawn in an overflow
  // Away (the window moves and is redrawn without A and B) and back (redrawn from the history).
  frame_at(*device, pipelines, t, rec, 40, 40);
  CHECK(texel(ge::gpu::read_now(*device, t.fine), app::fine_rect(t), 40.03125, 40.03125, 2) == 255);
  frame_at(*device, pipelines, t, rec, 8, 10);
  const ge::gpu::Image back = ge::gpu::read_now(*device, t.fine);
  check_fine(back);
  CHECK(back.rgba == first.rgba);
  CHECK(device->errors.load() == 0);
}
