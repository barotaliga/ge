// THE STRAFE JUMPER'S HANDS: the view angle that gains the most speed in a step of air under the
// acceleration rule, and the command that holds it. Speed is gained in the air only by turning:
// the rule adds along the wish direction until the speed ALONG it reaches the cap, so a wish
// direction swung far enough off the velocity takes a full `accel * wish * dt` and still adds to
// the speed across. The best angle is the one where that addition just meets the cap:
//
//     cos(theta) = (cap - accel * wish * dt) / speed
//
// (zero when that exceeds 1: too slow to gain by turning; then along the velocity). A bot that
// strafe jumps reads it, the `move-strafe` lab charts it, and a player's best line is the same.
//
// ge:summary The strafe jumper's hands: the best wish angle for air speed gain, and its command
// ge:keywords move strafe strafe-jump bunny-hop bhop optimal-angle best_wish_angle strafe_cmd
// ge:keywords air-strafe speed-gain bot hands circle turn
// ge:skill move bots
// ge:test tests/move_step_test.cpp
#pragma once

#include <cstdint>

#include "move/step.hpp"

namespace ge::move {

// The angle (rad, 0..pi) between the velocity and the wish that gains the most plan speed in one
// step of the acceleration rule.
double best_wish_angle(double speed, double wish_speed, double cap, double accel, double dt);

// Which keys the strafer holds with the strafe: forward and a strafe (the wish 45 degrees off the
// view), or the strafe alone (90 degrees off; the strafe-only air rules when a row has them).
enum class StrafeKeys : uint8_t { forward_side, side_only };

// The command that turns the view to the best angle for `dir` (+1 turning right, -1 left) in the
// air and presses jump on the ground (released in between, so a row that wants a fresh press gets
// one each landing). On the ground it runs forward along the velocity.
MoveCmd strafe_cmd(const MoveSpec& spec, const MoveState& s, StrafeKeys keys, int dir, double dt);

// The plan heading of a velocity (0 = +z, clockwise from above), 0 when it has none.
double heading_of(const Vec3& v);

}  // namespace ge::move
