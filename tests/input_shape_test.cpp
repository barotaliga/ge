// The shapes (engine/input/shape.hpp): each dead zone's numbers, the outer edge, the curve, the
// anti-dead zone, the threshold's hysteresis and the key ramp.
//
// ge:summary Tests: dead zones (axial, radial, scaled radial, sloped), curve, anti, ramp
// ge:keywords test input dead-zone axial radial scaled-radial sloped curve anti ramp threshold
// ge:skill input testing
#include <cmath>

#include "check.hpp"
#include "input/shape.hpp"

namespace in = ge::input;

namespace {
bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }
}  // namespace

TEST(scaled_radial_keeps_the_direction_and_rescales_the_length_without_a_step) {
  in::StickShape s{in::Zone::scaled_radial, 0.2f, 1.0f, {}, 0};
  float x = 0.1f, y = 0.1f;
  in::shape_stick(x, y, s);
  CHECK(x == 0 && y == 0);
  x = 0.6f, y = 0.0f;
  in::shape_stick(x, y, s);
  CHECK(near(x, 0.5) && y == 0);
  x = 0.3f, y = 0.4f;  // length 0.5: (0.5 - 0.2) / 0.8 = 0.375 along (0.6, 0.8)
  in::shape_stick(x, y, s);
  CHECK(near(x, 0.225) && near(y, 0.3));
  // Just past the zone the output is just past zero: no step.
  x = 0.2001f, y = 0;
  in::shape_stick(x, y, s);
  CHECK(x > 0 && x < 0.001f);
}

TEST(radial_steps_at_its_edge_and_axial_snaps_to_the_cardinals) {
  float x = 0.21f, y = 0;
  in::shape_stick(x, y, {in::Zone::radial, 0.2f, 1.0f, {}, 0});
  CHECK(x == 0.21f);
  x = 0.15f, y = 0.9f;
  in::shape_stick(x, y, {in::Zone::axial, 0.2f, 1.0f, {}, 0});
  CHECK(x == 0 && near(y, 0.875));
}

TEST(sloped_snaps_a_push_near_a_cardinal_and_keeps_a_diagonal) {
  const in::StickShape s{in::Zone::sloped, 0.2f, 1.0f, {}, 0};
  float x = 0.1f, y = 0.95f;  // nearly straight up
  in::shape_stick(x, y, s);
  CHECK(x == 0 && y > 0.9f);
  x = 0.6f, y = 0.6f;
  in::shape_stick(x, y, s);
  CHECK(x > 0.4f && near(x, y));
}

TEST(past_the_outer_edge_reads_full_and_never_past_one) {
  float x = 0.98f, y = 0;
  in::shape_stick(x, y, {in::Zone::scaled_radial, 0.1f, 0.9f, {}, 0});
  CHECK(x == 1);
  x = 1, y = 1;  // a square gate's corner
  in::shape_stick(x, y, {in::Zone::none, 0, 1, {}, 0});
  CHECK(near(std::sqrt(x * x + y * y), 1));
  CHECK(in::shape_axis(0.99f, {0.05f, 0.95f, {}, 0}) == 1);
  CHECK(in::shape_axis(-0.99f, {0.05f, 0.95f, {}, 0}) == -1);
}

TEST(the_curve_bends_the_middle_and_the_anti_zone_lifts_the_first_output) {
  const float half = in::shape_axis(0.5f, {0, 1, {2}, 0});
  CHECK(near(half, 0.25));
  CHECK(in::shape_axis(1, {0, 1, {2}, 0}) == 1);
  CHECK(near(in::shape_axis(0.1001f, {0.1f, 1, {}, 0.2f}), 0.2 + 0.8 * (0.0001 / 0.9)));
  CHECK(in::shape_axis(0.05f, {0.1f, 1, {}, 0.2f}) == 0);
}

TEST(a_threshold_holds_between_off_and_on) {
  const in::Threshold t{0.5f, 0.4f};
  CHECK(!in::digital(false, 0.45f, t));
  CHECK(in::digital(false, 0.5f, t));
  CHECK(in::digital(true, 0.45f, t));
  CHECK(!in::digital(true, 0.4f, t));
}

TEST(the_ramp_eases_toward_the_key_and_snaps_back_to_centre) {
  const in::Ramp r{6, 9, 0.02f};
  float v = 0;
  v = in::ramp_toward(v, 1, 1.0 / 120, r);
  CHECK(near(v, 0.05));
  for (int i = 0; i < 120; i++) v = in::ramp_toward(v, 1, 1.0 / 120, r);
  CHECK(v > 0.99f && v <= 1);
  int steps = 0;
  while (v != 0 && steps < 1000) {
    v = in::ramp_toward(v, 0, 1.0 / 120, r);
    steps++;
  }
  CHECK(v == 0 && steps < 60);
  CHECK(in::ramp_toward(0.3f, 1, 0.01, {}) == 1);  // no ramp: at once
}
