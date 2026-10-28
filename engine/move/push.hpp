// WHAT MOVES A BODY FROM OUTSIDE: a push (knock-back, a blast), a launch (a jump pad, a cannon),
// the arc a jump pad aims along, and a solid that moves into or under it (a platform, a lift, a
// door, a pusher). The step moves a body by its own command; these are the game's, called
// between steps when its world does something to the body.
//
// MOVING SOLIDS are stepped by their owner before the bodies: the solid is moved (and reports its
// `surface_vel`, world.hpp), then `carry` moves every body standing on it or now overlapping it
// by the same delta. A body standing on a moving solid keeps its own `vel` relative to it, so it
// rides without sliding off, and takes the solid's velocity with it when it jumps or walks off.
//
// ge:summary Pushes, launches, a jump pad's arc and moving solids carrying the bodies they bear
// ge:keywords move push knock-back knockback impulse launch jump-pad launcher arc arc_velocity
// ge:keywords carry pusher platform lift elevator door crush rider moving-ground
// ge:skill move
// ge:test tests/move_push_test.cpp
#pragma once

#include <cstdint>

#include "move/step.hpp"

namespace ge::move {

// `dv` (L/s) added to the body's velocity. A push with any upward part lifts a standing body off
// the ground (friction does not eat it, the ground's velocity goes with it) and spends its coyote
// time; a flat push on the ground is left to friction.
void push(MoveState& s, const Vec3& dv);

// The velocity set outright (L/s): the body leaves the ground with exactly `v`, its coyote time
// spent, its air jumps given back.
void launch(const MoveSpec& spec, MoveState& s, const Vec3& v);

// The launch velocity that carries a body from `from` to `to` over an arc whose top is `apex` L
// above the higher of the two, under `gravity` L/s^2 (the row's, while rising and falling alike):
// a jump pad aimed at its landing. Zero when gravity is not positive.
Vec3 arc_velocity(const Vec3& from, const Vec3& to, double apex, double gravity);

// Solid `solid` moved by `delta` this step (already moved in the world): the body goes with it
// when it stands on it or the solid now overlaps it. False, the body left where it was, when it
// does not fit where the solid would put it (crushed against something else: the game decides
// what that does, a door reverses, a crusher kills). True when it was carried or not touched.
bool carry(const MoveSpec& spec, const World& world, MoveState& s, uint32_t solid,
           const Vec3& delta);

}  // namespace ge::move
