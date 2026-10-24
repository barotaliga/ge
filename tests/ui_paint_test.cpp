// engine/ui's paints: the stops' lookup, the linear and radial parameters and the CSS angles.
//
// ge:summary Tests the 2D paints: solid, linear and radial gradients, stops, angles
// ge:keywords test ui paint gradient linear radial stop angle hex
// ge:skill ui testing
#include <cmath>

#include "check.hpp"
#include "ui/paint.hpp"

using namespace ge::ui;

namespace {

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

}  // namespace

TEST(hex_reads_the_three_bytes_and_alpha) {
  const Color c = hex(0xff8000, 0.5f);
  CHECK(near(c.r, 1) && near(c.g, 128.0f / 255) && near(c.b, 0) && near(c.a, 0.5f));
}

TEST(linear_holds_its_end_stops_past_its_ends) {
  const Paint p = linear(10, 0, 110, 0, hex(0x000000), hex(0xffffff));
  CHECK(near(p.at(0, 5).r, 0));
  CHECK(near(p.at(60, 5).r, 0.5f));
  CHECK(near(p.at(200, 5).r, 1));
}

TEST(stops_interpolate_between_their_neighbours) {
  const Stop s[3] = {{0, hex(0x000000)}, {0.25f, hex(0xffffff)}, {1, hex(0x000000)}};
  const Paint p = linear(0, 0, 100, 0, s, 3);
  CHECK(near(p.at(25, 0).r, 1));
  CHECK(near(p.at(12.5f, 0).r, 0.5f));
  CHECK(near(p.at(62.5f, 0).r, 0.5f));
}

TEST(angles_run_as_css_has_them) {
  const Stop s[2] = {{0, hex(0x000000)}, {1, hex(0xffffff)}};
  const Paint down = linear_at_angle(200, 100, 180, s, 2);  // top to bottom
  CHECK(down.at(100, 0).r < 0.01f && down.at(100, 100).r > 0.99f);
  const Paint right = linear_at_angle(200, 100, 90, s, 2);  // left to right
  CHECK(right.at(0, 50).r < 0.01f && right.at(200, 50).r > 0.99f);
  const Paint up = linear_at_angle(200, 100, 0, s, 2);
  CHECK(up.at(100, 100).r < 0.01f && up.at(100, 0).r > 0.99f);
  // A diagonal reaches its end stops exactly at the opposite corners.
  const Paint diag = linear_at_angle(200, 100, 135, s, 2);
  CHECK(diag.at(0, 0).r < 0.01f && diag.at(200, 100).r > 0.99f);
}

TEST(radial_runs_out_from_its_centre_and_squashes_by_aspect) {
  const Stop s[2] = {{0, hex(0xffffff)}, {1, hex(0x000000)}};
  const Paint p = radial(50, 50, 40, s, 2, 0.5f);
  CHECK(near(p.at(50, 50).r, 1));
  CHECK(near(p.at(90, 50).r, 0));  // 40 px across: t = 1
  CHECK(near(p.at(50, 70).r, 0));  // 20 px down is t = 1 at aspect 0.5
  CHECK(near(p.at(70, 50).r, 0.5f));
}

TEST(opaque_is_every_stop_opaque) {
  CHECK(solid(hex(0x123456)).opaque());
  CHECK(!linear(0, 0, 1, 0, hex(0, 1), hex(0, 0.5f)).opaque());
}
