// The aim help's turn of the view, defined: the target held or let go, the spring, the view moved.
//
// ge:summary The aim help's view turn defined
// ge:keywords aim-turn spring inertia view
// ge:skill shooter
// ge:test tests/shooter_aim_turn_test.cpp
#include "shooter/aim_turn.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/aim.hpp"

namespace ge::shooter {

namespace {

// `d` wrapped to -180..180 degrees.
float wrap(float d) {
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

// The critically damped spring of frequency `w` carrying `x` (rate `v`) toward `goal` for `dt`,
// in steps short enough to stay stable at any frame rate.
void spring(float& x, float& v, float goal, float w, float dt) {
  constexpr float kStep = 1.0f / 240;
  while (dt > 0) {
    const float h = dt < kStep ? dt : kStep;
    v += (w * w * (goal - x) - 2 * w * v) * h;
    x += v * h;
    dt -= h;
  }
}

}  // namespace

void aim_turn_bent(AimTurn& a, int target) { a.target = target; }

void aim_turn_step(AimTurn& a, Vec3f& view, Vec3f eye, const Vec3f* target_at, bool attack,
                   float dt) {
  float goal_pitch = 0, goal_yaw = 0;
  if (a.target >= 0 && attack && target_at != nullptr) {
    float yaw = 0, pitch = 0;
    aim_angles(eye, *target_at, yaw, pitch);
    goal_pitch = pitch - (view.x - a.pitch);
    goal_yaw = wrap(yaw - (view.y - a.yaw));
    // Let go when the target is too far off his own aim at its distance.
    const Vec3f d = *target_at - eye;
    const float dist = length(d);
    const double off = std::sqrt(double(goal_pitch) * goal_pitch + double(goal_yaw) * goal_yaw);
    const double reach =
        core::libm::atan2(kAimTurnReach, dist > 1 ? dist : 1) * (180.0 / core::kPi);
    if (off > reach) {
      a.target = -1;
      goal_pitch = goal_yaw = 0;
    }
  } else {
    a.target = -1;
  }
  const float w = a.target >= 0 ? kAimTurnIn : kAimTurnOut;
  const float old_pitch = a.pitch, old_yaw = a.yaw;
  spring(a.pitch, a.pitch_rate, goal_pitch, w, dt);
  spring(a.yaw, a.yaw_rate, goal_yaw, w, dt);
  if (a.target < 0 && a.pitch < kAimTurnRest && a.pitch > -kAimTurnRest && a.yaw < kAimTurnRest &&
      a.yaw > -kAimTurnRest) {
    a.pitch = a.yaw = a.pitch_rate = a.yaw_rate = 0;
  }
  view.x += a.pitch - old_pitch;
  view.y += a.yaw - old_yaw;
}

}  // namespace ge::shooter
