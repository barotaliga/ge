// THE EASY JUMP: forward and jump held, the client strafes for the player. Every hop it sends the
// commands a bunny hopper's hands would (the jump let go in the air and pressed on landing, the
// side key in place of the forward key, the yaw swung so the wish sits at the angle that gains),
// and any server runs them with its classic move: ours, the window's own and a stock one alike.
//
// The rules (`assisted_command`), from the player's held command and his predicted state:
//  - It acts only on a living, walking, dry player holding the forward key without a side key and
//    without attack: anything else is sent as held. A fire command carries the view the player's
//    screen shows (his own yaw plus what it shows of the swing), so a shot goes where he looks.
//  - THE JUMP: with the jump held, it is sent pressed while he stands (he jumps on the command he
//    lands) and let go while he flies, so the "let go between jumps" rule never stops the next hop.
//  - THE AIM is the player's own yaw: his mouse left and right (and the turn keys) steer the hops;
//    his pitch plays no part (the walk's wish is flat).
//  - THE SWING: the way turns left and right of the aim like a strafer's. At each takeoff it turns
//    back across the aim (toward it from the side the way is on), and in the air it turns the
//    other way once it is `kSpeedJumpSwing` degrees past the aim; so the path weaves across the
//    aim line, and a far-off aim is turned to at the strafe's full rate while it gains.
//  - THE GAIN: with h the plan speed and c the wish's share along the way, a command adds 30 - c
//    along the wish and raises h^2 by 900 - c^2, so c = 30 x sqrt(1 - share) gains `share` of the
//    best strafe's gain. The wish lies at acos(c / h) from the way on the turning side.
//  - THE SENT YAW: the side key held toward the turn (left for a left turn) with the yaw 90
//    degrees off the wish, which puts it near the way, as a strafer looks.
//
// What the player sees (`SpeedJumpAid::shown`): his own yaw plus `view` (0..1, `cl_speedjump_view`)
// of the sent yaw's swing off it, eased over a few commands; 0 a still screen, 1 a strafer's.
//
// Disclosed, never hidden: a client that uses it says `cl_speedjump 1` in its user info. The share
// is the server's `pm_speedjump` where it states one (ours always do; 0 turns the aid off) and the
// client's own on a server that says nothing.
//
// Pure, allocation-free, transcendentals through core's libm. Held by
// tests/shooter_speedjump_assist_test.cpp and the `se-speedjump` lab.
//
// ge:summary The easy jump: the client strafes for forward+jump, weaving the hops; the view's share
// ge:keywords speedjump cl_speedjump cl_speedjump_view pm_speedjump easy-jump easy-bunnyhop assist
// ge:keywords accessibility auto-strafe assisted_command strafe-share jump-release swing weave
// ge:skill shooter
// ge:test tests/shooter_speedjump_assist_test.cpp
#pragma once

#include "shooter/pmove.hpp"

namespace ge::shooter {

constexpr float kSpeedJumpSwing = 45;  // degrees the way swings past the aim before turning back

// The aid's memory between commands (a client keeps one for its player).
struct SpeedJumpAid {
  int turn = 0;         // +1 the way turning left (counter-clockwise), -1 right, 0 not yet
  bool flying = false;  // the last command the aid read was in the air
  float swing = 0;      // degrees the last sent yaw was off the player's own (kept on a landing)
  float shown = 0;      // degrees the screen shows off the player's own yaw (eased)
};

// The command to send for `held` (the player's own, his aim in `angles`) from his predicted state
// `s`, gaining `share` (0..1) of the best strafe's gain; `view` (0..1) of the swing is shown.
MoveCmd assisted_command(const MoveCmd& held, const MoveState& s, float share, float view,
                         SpeedJumpAid& aid);

}  // namespace ge::shooter
