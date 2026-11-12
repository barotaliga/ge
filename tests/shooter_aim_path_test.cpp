// plugins/shooter/aim_path: a player's motion read off his recent path. A steady runner reads his
// speed by every rule but `still`; a strafer's last step swings with each sway while the mean over
// a sway's length reads his course; a jumper reads in the air with the floor he left; and a place
// along the path is interpolated between its points; the arc tells a jump from a climb.
//
// ge:summary Test: the path rules: last step, the mean over a window and across, the air, the floor
// ge:keywords test aim-path read_path path_at window mean airborne floor
// ge:skill shooter testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "shooter/aim_path.hpp"

using namespace ge::shooter;

namespace {

constexpr float kDt = 1.0f / 80;

// `n` points at `kDt` apart, `f(t)` the place.
template <class F>
std::vector<PathPoint> path(int n, F f) {
  std::vector<PathPoint> out;
  for (int i = 0; i < n; i++) {
    const float t = float(i) * kDt;
    out.push_back({t, f(t)});
  }
  return out;
}

bool near(float a, float b, float tol) { return std::abs(a - b) <= tol; }

}  // namespace

TEST(a_steady_runner_reads_his_speed_by_every_rule_but_still) {
  const auto p = path(80, [](float t) { return Vec3f{320 * t, 0, 24}; });
  for (PathGuess g : {PathGuess::last, PathGuess::mean, PathGuess::mean_flat}) {
    const PathMotion m = read_path(p, {g, 0.3f});
    CHECK_MSG(near(m.velocity.x, 320, 1) && near(m.velocity.y, 0, 0.01f) && !m.airborne,
              path_guess_name(g));
  }
  const PathMotion s = read_path(p, {PathGuess::still, 0});
  CHECK(s.velocity.x == 0 && !s.airborne);
}

TEST(a_strafers_mean_over_a_sway_reads_his_course_and_his_last_step_the_sway) {
  // 320 forward along x, swaying 300 units/s across every 0.25 s.
  const auto p = path(160, [](float t) {
    const float leg = t / 0.25f;
    const auto n = static_cast<int>(leg);
    const float f = leg - float(n);
    const float side = (n % 2 == 0 ? f : 1 - f) * 75;
    return Vec3f{320 * t, side, 24};
  });
  const PathMotion last = read_path(p, {PathGuess::last, 0});
  const PathMotion mean = read_path(p, {PathGuess::mean, 0.5f});
  CHECK(near(std::abs(last.velocity.y), 300, 2));
  CHECK(near(mean.velocity.y, 0, 10) && near(mean.velocity.x, 320, 2));
}

TEST(a_jumper_reads_in_the_air_with_the_floor_he_left) {
  // On the floor at 24 for 0.5 s, then a jump of 270 up under 800.
  const auto p = path(60, [](float t) {
    if (t < 0.5f) return Vec3f{0, 0, 24};
    const float u = t - 0.5f;
    return Vec3f{0, 0, 24 + 270 * u - 400 * u * u};
  });
  const PathMotion m = read_path(p, {PathGuess::mean_flat, 0.3f});
  CHECK(m.airborne);
  CHECK(near(m.floor_z, 24, 0.01f));
  CHECK(m.velocity.z > 0 && m.velocity.z < 270);
}

TEST(a_place_along_the_path_is_interpolated_and_held_at_its_ends) {
  const std::vector<PathPoint> p = {{0, {0, 0, 0}}, {1, {10, 0, 0}}, {2, {10, 20, 0}}};
  CHECK(near(path_at(p, 0.5f).x, 5, 1e-4f));
  CHECK(near(path_at(p, 1.5f).y, 10, 1e-4f));
  CHECK(path_at(p, -1).x == 0 && path_at(p, 3).y == 20);
}

TEST(the_arc_reads_a_jump_by_its_fall_and_never_a_climb) {
  // On the ground for 0.5 s at height 24, then a jump at 270 units/s for 0.2 s.
  const auto jump = path(56, [](float t) {
    const float up = t > 0.5f ? t - 0.5f : 0;
    return Vec3f{320 * t, 0, 24 + 270 * up - 0.5f * kPathGravity * up * up};
  });
  PathRule rule{PathGuess::last, 0.1f};
  rule.arc = true;
  const PathMotion m = read_path(jump, rule);
  CHECK(m.airborne);
  CHECK(near(m.floor_z, 24, 2));
  CHECK(near(m.velocity.z, 270 - kPathGravity * 0.1875f, 12));
  // Up a ramp at 100 units/s: no fall by gravity, so on the ground, and not rising.
  const auto climb = path(56, [](float t) { return Vec3f{320 * t, 0, 24 + 100 * t}; });
  const PathMotion c = read_path(climb, rule);
  CHECK(!c.airborne && c.velocity.z == 0);
}
