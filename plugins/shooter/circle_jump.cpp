// The circle jump (circle_jump.hpp).
//
// ge:summary The circle jump: the ground curve's keys and view, and when to jump
// ge:keywords circle-jump circle_step circle_angle ground-strafe
// ge:skill shooter bots
// ge:test tests/shooter_circle_jump_test.cpp
#include "shooter/circle_jump.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::shooter {

namespace {

constexpr float kKeysOffView = 45;  // forward and a side key: the wish this far off the view

}  // namespace

float circle_angle(float speed, float dt, const MoveSettings& move) {
  const float control = speed < move.stopspeed ? move.stopspeed : speed;
  float after = speed - control * move.friction * dt;
  if (after < 1) return 0;
  const float add = move.accelerate * move.maxspeed * dt;
  const double c = static_cast<double>(move.maxspeed - add) / static_cast<double>(after);
  if (c >= 1) return 0;
  return static_cast<float>(core::libm::acos(c < 0 ? 0 : c) * (180 / core::kPi));
}

CircleStep circle_step(float heading, float speed, float err, float elapsed_s, float dt,
                       int8_t side, const MoveSettings& move, const CircleSettings& set) {
  CircleStep out;
  // The side that turns the velocity toward the aim (the left key turns it left): chosen once.
  out.side = side != 0 ? side : err > 0 ? int8_t{-1} : int8_t{1};
  const float toward = -static_cast<float>(out.side) * err;  // + while the aim is still ahead
  if ((side == 0 && toward < set.corner) || speed >= set.target || elapsed_s >= set.most_s ||
      toward < -set.past) {
    out.jump = true;
    return out;
  }
  // The wish `angle` off the velocity on the turn's side; forward and the side key put it 45
  // degrees off the view toward the key.
  const float angle = circle_angle(speed, dt, move);
  out.yaw = heading - static_cast<float>(out.side) * (angle - kKeysOffView);
  return out;
}

}  // namespace ge::shooter
