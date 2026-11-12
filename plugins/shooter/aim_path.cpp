// A player's motion read off his recent path (aim_path.hpp).
//
// ge:summary The path rules' bodies: the last step, the mean over a window, the floor under him
// ge:keywords aim-path read_path path_at window mean floor airborne
// ge:skill shooter
// ge:test tests/shooter_aim_path_test.cpp
#include "shooter/aim_path.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::shooter {

Vec3f path_at(std::span<const PathPoint> past, float t) {
  if (past.empty()) return {};
  if (t <= past.front().t) return past.front().origin;
  if (t >= past.back().t) return past.back().origin;
  const auto it = std::upper_bound(past.begin(), past.end(), t,
                                   [](float v, const PathPoint& p) { return v < p.t; });
  const PathPoint& b = *it;
  const PathPoint& a = *(it - 1);
  const float span = b.t - a.t;
  const float f = span > 0 ? (t - a.t) / span : 1;
  return a.origin + (b.origin - a.origin) * f;
}

namespace {

// The rise or fall of the step ending at `past[i]`, units/s.
float step_z(std::span<const PathPoint> past, size_t i) {
  const float dt = past[i].t - past[i - 1].t;
  return dt > 0 ? (past[i].origin.z - past[i - 1].origin.z) / dt : 0;
}

// Whether the steps ending at `past[i - 1]` and `past[i]` fall by gravity.
bool on_arc(std::span<const PathPoint> past, size_t i) {
  const float dt = 0.5f * (past[i].t - past[i - 2].t);
  if (dt <= 0) return false;
  const float a = (step_z(past, i) - step_z(past, i - 1)) / dt;
  return a > -kPathGravity - kPathArcSlack && a < -kPathGravity + kPathArcSlack;
}

// `arc`: whether he is in the air by his path, and the height he took off from.
void read_arc(std::span<const PathPoint> past, PathMotion& m) {
  m.airborne = false;
  m.floor_z = -1e30f;
  const size_t n = past.size();
  if (n < 3) return;
  const float vz = step_z(past, n - 1), vz_before = step_z(past, n - 2);
  const bool jump = vz > kPathJumpZ && vz_before < kPathStillZ && vz_before > -kPathStillZ;
  if (!jump && !on_arc(past, n - 1)) return;
  m.airborne = true;
  size_t start = jump ? n - 2 : n - 1;
  while (start >= 2 && on_arc(past, start)) start--;
  // `start` is the arc's first point, or the jump's (the step before it rose fast from level).
  if (start >= 1 && step_z(past, start) > kPathJumpZ) start--;
  m.floor_z = past[start].origin.z;
  if (past[n - 1].origin.z < m.floor_z) m.floor_z = -1e30f;  // off a ledge: no floor known
}

PathMotion read_rule(std::span<const PathPoint> past, PathRule rule) {
  PathMotion m;
  if (past.empty()) return m;
  const PathPoint& now = past.back();
  Vec3f last;
  if (past.size() >= 2) {
    const PathPoint& before = past[past.size() - 2];
    const float dt = now.t - before.t;
    if (dt > 0) last = (now.origin - before.origin) * (1 / dt);
  }
  float low = now.origin.z;
  for (auto it = past.rbegin(); it != past.rend() && now.t - it->t <= kPathFloorS; ++it)
    low = std::min(low, it->origin.z);
  m.floor_z = low;
  m.airborne = (last.z > kPathStillZ || last.z < -kPathStillZ) || now.origin.z > low + 1;
  switch (rule.guess) {
    case PathGuess::still:
      m.airborne = false;
      return m;
    case PathGuess::last:
      m.velocity = last;
      return m;
    case PathGuess::mean:
    case PathGuess::mean_flat: {
      const float from = std::max(past.front().t, now.t - rule.window);
      const float dt = now.t - from;
      Vec3f mean = last;
      if (dt > 0) mean = (now.origin - path_at(past, from)) * (1 / dt);
      if (rule.guess == PathGuess::mean_flat) mean.z = last.z;
      m.velocity = mean;
      return m;
    }
    case PathGuess::fading:
      m.velocity = last;
      m.fade = rule.window;
      return m;
    case PathGuess::turning: {
      m.velocity = last;
      // The heading of the step `window` ago against the last step's.
      const float then_t = now.t - rule.window;
      if (then_t <= past.front().t || past.size() < 3) return m;
      const float h = std::min(0.02f, rule.window * 0.5f);
      const Vec3f before = (path_at(past, then_t + h) - path_at(past, then_t - h)) * (0.5f / h);
      const double sb = double(before.x) * before.x + double(before.y) * before.y;
      const double sl = double(last.x) * last.x + double(last.y) * last.y;
      if (sb < 100 * 100 || sl < 100 * 100) return m;  // too slow to read a heading
      const double a0 = core::libm::atan2(double(before.y), double(before.x));
      const double a1 = core::libm::atan2(double(last.y), double(last.x));
      double da = a1 - a0;
      const double pi = 3.141592653589793;
      while (da > pi) da -= 2 * pi;
      while (da < -pi) da += 2 * pi;
      m.turn = float(da / double(rule.window));
      return m;
    }
    case PathGuess::smoothed: {
      // From the oldest step within 4 time constants to the last.
      size_t i = past.size() - 1;
      while (i > 0 && now.t - past[i - 1].t <= 4 * rule.window) i--;
      Vec3f v;
      bool first = true;
      for (size_t k = i + 1; k < past.size(); k++) {
        const float dt = past[k].t - past[k - 1].t;
        if (dt <= 0) continue;
        const Vec3f step = (past[k].origin - past[k - 1].origin) * (1 / dt);
        const float pull =
            first ? 1 : float(1 - core::libm::exp(-double(dt) / double(rule.window)));
        v = v + (step - v) * pull;
        first = false;
      }
      m.velocity = first ? last : v;
      m.velocity.z = last.z;
      return m;
    }
    case PathGuess::speeding: {
      m.velocity = last;
      const float then_t = now.t - rule.window;
      if (then_t <= past.front().t || past.size() < 3) return m;
      const float h = std::min(0.02f, rule.window * 0.5f);
      const Vec3f before = (path_at(past, then_t + h) - path_at(past, then_t - h)) * (0.5f / h);
      m.accel = (last - before) * (1 / rule.window);
      m.accel.z = 0;
      return m;
    }
    case PathGuess::sway_centre: {
      const float from = std::max(past.front().t, now.t - rule.window);
      const float dt = now.t - from;
      if (dt <= 0) {
        m.velocity = last;
        return m;
      }
      Vec3f course = (now.origin - path_at(past, from)) * (1 / dt);
      course.z = last.z;
      // The middle of the window's places, carried on by the course to now.
      Vec3f sum;
      int n = 0;
      for (const PathPoint& p : past)
        if (p.t >= from) {
          sum = sum + p.origin;
          n++;
        }
      const Vec3f middle = sum * (1 / float(n)) + course * ((now.t - from) * 0.5f);
      m.velocity = course;
      m.shift = middle - now.origin;
      m.shift.z = 0;
      return m;
    }
  }
  return m;
}

}  // namespace

PathMotion read_path(std::span<const PathPoint> past, PathRule rule) {
  PathMotion m = read_rule(past, rule);
  if (rule.arc && !rule.level) {
    read_arc(past, m);
    if (!m.airborne) {
      m.velocity.z = 0;
      m.accel.z = 0;
      m.shift.z = 0;
    } else if (past.size() >= 2) {
      const PathPoint& now = past.back();
      const PathPoint& before = past[past.size() - 2];
      const float dt = now.t - before.t;  // the step's mean rise, carried on to now
      if (dt > 0) m.velocity.z = (now.origin.z - before.origin.z) / dt - 0.5f * kPathGravity * dt;
    }
  }
  if (rule.level) {
    m.velocity.z = 0;
    m.accel.z = 0;
    m.shift.z = 0;
    m.airborne = false;
  }
  if (rule.run > 0 && past.size() >= 2) {
    const PathPoint& now = past.back();
    const PathPoint& before = past[past.size() - 2];
    const float dt = now.t - before.t;
    const float dx = now.origin.x - before.origin.x, dy = now.origin.y - before.origin.y;
    if (dt > 0 && dx * dx + dy * dy < rule.run * rule.run * dt * dt) {
      m.velocity.x = 0;
      m.velocity.y = 0;
      m.fade = 0;
      m.turn = 0;
      m.accel = {};
      m.shift = {};
    }
  }
  return m;
}

const char* path_guess_name(PathGuess g) {
  switch (g) {
    case PathGuess::still:
      return "still";
    case PathGuess::last:
      return "last step";
    case PathGuess::mean:
      return "mean";
    case PathGuess::mean_flat:
      return "mean across";
    case PathGuess::fading:
      return "fading";
    case PathGuess::turning:
      return "turning";
    case PathGuess::smoothed:
      return "smoothed";
    case PathGuess::speeding:
      return "speeding";
    case PathGuess::sway_centre:
      return "sway centre";
  }
  return "";
}

}  // namespace ge::shooter
