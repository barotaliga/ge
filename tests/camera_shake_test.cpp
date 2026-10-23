// The trauma shake (engine/camera/shake.hpp): nothing without trauma, trauma capped and decaying
// linearly, a bigger trauma shakes more than in proportion, comfort 0 removes it, the waves stay
// in -1..1 and repeat from their seed.
//
// ge:summary Tests: the trauma shake's cap, decay, exponent, comfort and waves
// ge:keywords test camera shake trauma decay comfort wave
// ge:skill camera testing
// ge:test tests/camera_shake_test.cpp
#include <cmath>

#include "camera/shake.hpp"
#include "check.hpp"

namespace cam = ge::camera;

namespace {
double peak_yaw(double trauma, double scale) {
  cam::ShakeRow row;
  row.decay_per_s = 0;
  cam::ShakeState s = cam::make_shake(7);
  cam::add_trauma(s, trauma);
  double most = 0;
  for (int i = 0; i < 600; ++i) {
    const double y = std::fabs(cam::shake_at(row, s, 1.0 / 120, scale).yaw);
    if (y > most) most = y;
  }
  return most;
}
}  // namespace

TEST(no_trauma_no_shake) {
  cam::ShakeState s = cam::make_shake(1);
  const cam::ShakeOut o = cam::shake_at(cam::ShakeRow{}, s, 1.0 / 60, 1);
  CHECK(o.yaw == 0 && o.pitch == 0 && o.roll == 0 && o.up == 0);
}

TEST(trauma_is_capped_and_decays_linearly) {
  cam::ShakeState s = cam::make_shake(1);
  cam::add_trauma(s, 0.7);
  cam::add_trauma(s, 0.7);
  CHECK(s.trauma == 1);
  cam::ShakeRow row;
  row.decay_per_s = 0.5;
  for (int i = 0; i < 60; ++i) cam::shake_at(row, s, 1.0 / 60, 1);
  CHECK(std::fabs(s.trauma - 0.5) < 1e-9);
}

TEST(twice_the_trauma_shakes_about_four_times_as_much) {
  const double a = peak_yaw(0.4, 1), b = peak_yaw(0.8, 1);
  CHECK(b > 3.5 * a && b < 4.5 * a);
}

TEST(comfort_zero_removes_the_shake) { CHECK(peak_yaw(1, 0) == 0); }

TEST(the_waves_stay_in_range_and_repeat_from_their_seed) {
  const cam::Waves a = cam::make_waves(3), b = cam::make_waves(3);
  for (int i = 0; i < 1000; ++i) {
    const double t = i * 0.013;
    const double v = cam::wave(a.axis[0], t, 7);
    CHECK(v >= -1 && v <= 1);
    CHECK(v == cam::wave(b.axis[0], t, 7));
  }
}
