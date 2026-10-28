// Pushes, launches, the jump pad's arc and the carry.
//
// ge:summary push, launch, arc_velocity and carry: what moves a body from outside the step
// ge:keywords move push launch arc_velocity carry
// ge:skill move
// ge:test tests/move_push_test.cpp
#include "move/push.hpp"

#include <cmath>

namespace ge::move {

namespace {

void leave_ground(MoveState& s) {
  if (s.grounded) s.vel = add(s.vel, s.ground_vel);
  s.grounded = false;
  s.sliding = false;
  s.since_ground = kSinceNever;
  s.ground_solid = 0;
  s.ground_vel = {0, 0, 0};
}

}  // namespace

void push(MoveState& s, const Vec3& dv) {
  if (dv.y > 0) leave_ground(s);
  s.vel = add(s.vel, dv);
}

void launch(const MoveSpec& spec, MoveState& s, const Vec3& v) {
  leave_ground(s);
  s.vel = v;
  s.jump_rising = false;
  s.air_jumps_left = spec.air_jumps;
}

Vec3 arc_velocity(const Vec3& from, const Vec3& to, double apex, double gravity) {
  if (gravity <= 0) return {0, 0, 0};
  const double top = (from.y > to.y ? from.y : to.y) + (apex > 0 ? apex : 0);
  const double up = top - from.y, down = top - to.y;
  const double vy = std::sqrt(2 * gravity * up);
  const double t = vy / gravity + std::sqrt(2 * down / gravity);
  if (t <= 0) return {0, vy, 0};
  return {(to.x - from.x) / t, vy, (to.z - from.z) / t};
}

bool carry(const MoveSpec& spec, const World& world, MoveState& s, uint32_t solid,
           const Vec3& delta) {
  const Box box = hull(spec, s.ducked);
  const bool riding = s.grounded && solid != 0 && s.ground_solid == solid;
  const bool overlapped = world.sweep(box, s.pos, s.pos).start_solid;
  if (!riding && !overlapped) return true;
  const Vec3 to = add(s.pos, delta);
  if (world.sweep(box, to, to).start_solid) return false;
  s.pos = to;
  return true;
}

}  // namespace ge::move
