// The best angle, and the strafer's command built off the same air rules the step applies.
//
// ge:summary The strafer's angle and command, read off the air rules the step applies
// ge:keywords move strafe best-angle strafe_cmd heading_of
// ge:skill move bots
// ge:test tests/move_step_test.cpp
#include "move/strafe.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::move {

double best_wish_angle(double speed, double wish_speed, double cap, double accel, double dt) {
  if (speed <= 0) return 0;
  const double c = (cap - accel * wish_speed * dt) / speed;
  if (c >= 1) return 0;
  return core::libm::acos(c < -1 ? -1 : c);
}

double heading_of(const Vec3& v) {
  if (v.x == 0 && v.z == 0) return 0;
  return core::libm::atan2(v.x, v.z);
}

MoveCmd strafe_cmd(const MoveSpec& spec, const MoveState& s, StrafeKeys keys, int dir, double dt) {
  MoveCmd cmd;
  const double h = heading_of(s.vel);
  if (s.grounded) {
    cmd.forward = 1;
    cmd.yaw = h;
    cmd.jump = !s.jump_held;
    return cmd;
  }
  const double side = dir < 0 ? -1.0 : 1.0;
  cmd.side = side;
  cmd.forward = keys == StrafeKeys::forward_side ? 1 : 0;
  // The wish speed and the rule's numbers for these keys, as the step will read them.
  const Wish w = wish_of(spec, s, cmd);  // at yaw 0: its heading is the keys' angle off the view
  double accel = spec.air_accelerate, wish = w.speed;
  double cap = spec.air_speed_cap > 0 && spec.air_speed_cap < wish ? spec.air_speed_cap : wish;
  if (keys == StrafeKeys::side_only && spec.air_strafe_accelerate > 0) {
    accel = spec.air_strafe_accelerate;
    if (spec.air_strafe_cap > 0 && spec.air_strafe_cap < wish) {
      wish = spec.air_strafe_cap;
      cap = wish;
    }
  }
  const double theta = best_wish_angle(flat_length(s.vel), wish, cap, accel, dt);
  // The wish lies off the view by the keys' angle (45 degrees for forward and a strafe when the
  // row's shares are equal; the shares move it), read off the wish itself.
  cmd.yaw = h + side * theta - heading_of(w.dir);
  return cmd;
}

}  // namespace ge::move
