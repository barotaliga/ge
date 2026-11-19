// The fly between the map and the eye (engine/overview/fly.hpp): the top pose fits the level,
// the dive starts at the top and lands in the eye, the demo is held while it dives and the
// catch-up gives back exactly what was held, a way turned back mid-dive climbs from where it is.
//
// ge:summary Tests: the fly's top pose, dive, tilt, map fade and the demo's catch-up
// ge:keywords overview fly-down test catch-up demo-rate tilt-up
// ge:skill map-design testing
#include <cmath>

#include "check.hpp"
#include "core/math.hpp"
#include "overview/fly.hpp"

namespace ov = ge::overview;

namespace {

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

ov::FlyPose an_eye() {
  ov::FlyPose e;
  e.eye[0] = 30;
  e.eye[1] = 1.5;
  e.eye[2] = -12;
  e.heading = 2.0;
  e.pitch = 0.1;
  return e;
}

}  // namespace

TEST(overview_fly_top_pose_sees_the_whole_level) {
  const double lo[3] = {-50, -5, -20}, hi[3] = {50, 10, 20};
  const ov::FlyPose t = ov::top_pose(lo, hi, 1.0, 16.0 / 9.0);
  CHECK(t.eye[0] == 0 && t.eye[2] == 0);
  CHECK(near(t.pitch, -ge::core::kPi / 2, 1e-12));
  // The width (100 m) fits the horizontal half-angle from that height.
  const double half_w = (t.eye[1] - hi[1]) * std::tan(0.5) * 16.0 / 9.0;
  CHECK(half_w >= 50);
}

TEST(overview_fly_dives_from_the_top_into_the_eye) {
  const double lo[3] = {-50, -5, -20}, hi[3] = {50, 10, 20};
  const ov::FlyPose top = ov::top_pose(lo, hi, 1.0, 1.6);
  const ov::FlyPose eye = an_eye();
  const ov::FlyTimes times;
  ov::Fly f;
  CHECK(!ov::fly_shows_world(f));
  CHECK(ov::fly_map_share(f, times) == 1);
  const ov::FlyPose at0 = ov::fly_pose(f, times, top, eye);
  CHECK(at0.eye[1] == top.eye[1] && at0.pitch == top.pitch);
  ov::fly_start(f, ov::FlyWay::down);
  double last_y = top.eye[1];
  const double dt = 1.0 / 120;
  double held = 0;
  while (f.way == ov::FlyWay::down) {
    CHECK(ov::fly_demo_rate(f, times) == 0);  // held while it dives
    ov::fly_advance(f, dt, times);
    held += dt;
    const ov::FlyPose now = ov::fly_pose(f, times, top, eye);
    CHECK(now.eye[1] <= last_y + 1e-9);  // only ever down
    last_y = now.eye[1];
  }
  CHECK(near(held, times.dive_s, 2 * dt));
  CHECK(ov::fly_shows_world(f));
  CHECK(ov::fly_map_share(f, times) == 0);
  const ov::FlyPose landed = ov::fly_pose(f, times, top, eye);
  CHECK(landed.eye[0] == eye.eye[0] && landed.pitch == eye.pitch && landed.heading == eye.heading);
  // The catch-up gives back what the dive held, starting fast and easing to 1x.
  CHECK(ov::fly_running(f));
  CHECK(ov::fly_demo_rate(f, times) > 3);
  double given = 0;
  while (ov::fly_running(f)) {
    given += (ov::fly_demo_rate(f, times) - 1) * dt;
    ov::fly_advance(f, dt, times);
  }
  CHECK(near(given, held, 0.05));
  CHECK(ov::fly_demo_rate(f, times) == 1);
}

TEST(overview_fly_turns_back_from_where_it_is) {
  const ov::FlyTimes times;
  ov::Fly f;
  ov::fly_start(f, ov::FlyWay::down);
  for (int i = 0; i < 60; ++i) ov::fly_advance(f, 1.0 / 120, times);
  const double mid = f.p;
  CHECK(mid > 0.1 && mid < 0.9);
  ov::fly_start(f, ov::FlyWay::up);
  ov::fly_advance(f, 1.0 / 120, times);
  CHECK(f.p < mid);
  while (f.way == ov::FlyWay::up) ov::fly_advance(f, 1.0 / 120, times);
  CHECK(f.p == 0);
  CHECK(ov::fly_map_share(f, times) == 1);
  CHECK(!ov::fly_shows_world(f));
}
