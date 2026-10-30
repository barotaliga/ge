// engine/render/bolts.hpp and the frame's lightning rows (render/view.hpp's `flash_rows`): a
// lamp's place, reach, light and depth out of its cloud reach the frame block, its light as its
// flux over the sun's lux at the frame's pre-exposure, past four lamps none; a flash takes the
// cloud's spread march off for four frames from it; a segment is the shader's 32 bytes.
//
// ge:summary Test: the lightning's lamps in the frame block, the spread held off after a flash
// ge:keywords test lightning flash flash_rows bolts frame-uniforms cloud-spread
// ge:skill render water testing
#include <cmath>

#include "check.hpp"
#include "render/bolts.hpp"
#include "render/frame.hpp"

namespace gr = ge::render;

TEST(flash_rows_carry_each_lamp) {
  gr::Flashes f;
  f.count = 2;
  f.lamp[0] = {{100, 6000, -50}, 2000, 1e7, {1, 1, 1}, 12};
  f.lamp[1] = {{0, 3000, 900}, 4000, 3e7, {0.5, 1, 2}, 3};
  float info[4], rows[2 * gr::kFlashLamps][4];
  gr::flash_rows(f, 2.0, info, rows);
  CHECK(info[0] == 2);
  CHECK(rows[0][0] == 100 && rows[0][1] == 6000 && rows[0][2] == -50 && rows[0][3] == 2000);
  const double flux = 4 * 3.14159265358979 * 1e7 / 128000 * 2.0;
  CHECK(std::fabs(rows[1][0] - flux) < 1e-3 * flux && rows[1][3] == 12);
  CHECK(std::fabs(rows[3][2] - 2 * 3 * flux) < 1e-3 * flux);
  for (int k = 4; k < 2 * gr::kFlashLamps; ++k)
    CHECK(rows[k][0] == 0 && rows[k][1] == 0 && rows[k][3] == 0);
  f.count = 9;
  gr::flash_rows(f, 1.0, info, rows);
  CHECK(info[0] == gr::kFlashLamps);
}

TEST(a_flash_holds_the_cloud_spread_off_four_frames) {
  gr::FrameView v;
  v.clouds.count = 1;
  v.clouds.layers[0].top_m = 1000;
  gr::FrameHistory h;
  gr::FrameUniforms u;
  auto spread_at = [&](uint64_t frame, bool flash) {
    v.frame = frame;
    v.flashes.count = flash ? 1 : 0;
    v.flashes.lamp[0].candela = 1e7;
    gr::fill_uniforms(u, v, 64, 64, &h);
    return u.cloud_info[2] > 0.5f;
  };
  CHECK(!spread_at(0, false));  // no history yet
  CHECK(spread_at(1, false));
  CHECK(!spread_at(2, true));
  CHECK(u.flash_info[0] == 1);
  CHECK(!spread_at(3, false) && !spread_at(4, false) && !spread_at(5, false));
  CHECK(spread_at(6, false));
  CHECK(u.flash_info[0] == 0);
}

TEST(a_segment_is_the_shaders) { CHECK(sizeof(gr::BoltSegment) == 32); }
