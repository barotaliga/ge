// The camera's followers (engine/camera/follow.hpp): the second-order follower settles on a step
// without overshoot when critically damped and with it under, `r` 2 tracks a ramp with no lag,
// a long frame never explodes, the paths at 30 and 240 Hz agree within 4% of a step, angles go the
// short way, and the half-life springs halve on time at any rate.
//
// ge:summary Tests: second-order followers, half-life springs, dead band, rate independence
// ge:keywords test camera follower spring overshoot ramp stable halflife dead-band
// ge:skill camera testing
// ge:test tests/camera_follow_test.cpp
#include <cmath>
#include <string>

#include "camera/follow.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

namespace {
// The follower's position after `seconds` of a unit step, at `hz`; and its highest value.
double step_at(const cam::Poles& p, double hz, double seconds, double* peak = nullptr) {
  cam::Follower f;
  cam::settle(f, 0);
  const int n = static_cast<int>(std::lround(seconds * hz));
  double most = 0;
  for (int i = 0; i < n; ++i) {
    cam::follow(f, p, 1.0, 1.0 / hz);
    if (f.y > most) most = f.y;
  }
  if (peak) *peak = most;
  return f.y;
}
}  // namespace

TEST(a_critically_damped_follower_settles_without_overshoot) {
  double peak = 0;
  const double y = step_at({2, 1, 0}, 120, 3, &peak);
  CHECK(std::fabs(y - 1) < 1e-3);
  CHECK_MSG(peak <= 1 + 1e-9, "critical damping overshot to " + std::to_string(peak));
}

TEST(an_under_damped_follower_overshoots_and_settles) {
  double peak = 0;
  const double y = step_at({2, 0.4, 0}, 120, 6, &peak);
  CHECK(peak > 1.1);
  CHECK(std::fabs(y - 1) < 1e-2);
}

TEST(response_two_tracks_a_ramp_with_no_lag) {
  cam::Follower f;
  cam::settle(f, 0);
  const cam::Poles p{1.5, 0.9, 2};
  double x = 0;
  for (int i = 0; i < 600; ++i) {
    x += 3.0 / 120;
    cam::follow(f, p, x, 1.0 / 120);
  }
  CHECK_MSG(std::fabs(f.y - x) < 1e-3, "r = 2 lags a ramp by " + std::to_string(x - f.y));
}

TEST(a_long_frame_is_a_slower_lens_never_an_explosion) {
  cam::Follower f;
  cam::settle(f, 0);
  const cam::Poles stiff{12, 0.3, 0};
  for (int i = 0; i < 50; ++i) cam::follow(f, stiff, 1.0, 0.5);
  CHECK(std::isfinite(f.y));
  CHECK(std::fabs(f.y - 1) < 0.5);
}

TEST(the_paths_at_thirty_and_two_hundred_forty_hertz_agree) {
  const cam::Poles p{1.2, 0.85, 0};
  for (double s : {0.25, 0.5, 1.0, 2.0}) {
    const double a = step_at(p, 30, s), b = step_at(p, 240, s);
    CHECK_MSG(std::fabs(a - b) < 0.04, "30 Hz and 240 Hz differ by " + std::to_string(a - b) +
                                           " at " + std::to_string(s) + " s");
  }
}

TEST(an_angle_is_followed_the_short_way_round) {
  cam::Follower f;
  cam::settle(f, 3.0);
  for (int i = 0; i < 240; ++i) cam::follow_angle(f, {2, 1, 0}, -3.0, 1.0 / 120);
  CHECK_MSG(std::fabs(ge::core::angle_diff(f.y, -3.0)) < 1e-3 && f.y > 3.0,
            "an angle from 3 to -3 rad was followed the long way: " + std::to_string(f.y));
}

TEST(a_half_life_halves_on_time_at_any_rate) {
  for (double hz : {30.0, 150.0}) {
    cam::Spring s{1, 0};
    double x = 1;
    const int n = static_cast<int>(std::lround(0.2 * hz));
    for (int i = 0; i < n; ++i) x = cam::damp(x, 0, 0.2, 1.0 / hz);
    CHECK(std::fabs(x - 0.5) < 1e-9);
    for (int i = 0; i < static_cast<int>(3 * hz); ++i) cam::spring_to(s, 0, 0.2, 1.0 / hz);
    CHECK(std::fabs(s.x) < 1e-3);
  }
}

TEST(the_dead_band_chases_nothing_inside_and_everything_past_its_soft_edge) {
  CHECK(cam::dead_band(0.05, 0.1, 0.1) == 0);
  CHECK(cam::dead_band(-0.3, 0.1, 0.1) == 1);
  const double mid = cam::dead_band(0.15, 0.1, 0.1);
  CHECK(mid > 0 && mid < 1);
}
