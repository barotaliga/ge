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
//    aim line, and a far-off aim is turned to at the strafe's full rate while it gains. In a slow
//    air (a command's step A short of the air's cap: the sequel's default) a flight turns less
//    than the swing, so a takeoff near the aim (within
//    half the last flight's turn) turns to the side the last takeoff did, so the hops go out to
//    both sides in turn rather than back and forth on one. Such a move cannot turn its hops to a
//    far-off aim at speed (the turn a command is the gain's step over the speed), so it steers on
//    foot: landing more than `kSpeedJumpSteer` off the aim, the jump is held back until the walk
//    has turned the way back within the swing, as a player lands and turns.
//  - THE GAIN, by the move's air law (`SpeedJumpRules`): a command adds along the wish at most
//    A = accelerate x wish speed x seconds, and no more than W - c, where c is the velocity's share
//    along the wish and W the wish speed held to the air's cap. Its gain in h^2 (h the plan speed)
//    is 2Ac + A^2 while c <= W - A and W^2 - c^2 above: the best is at c* = max(0, W - A), and
//    c = sqrt(W^2 - share x (W^2 - c*^2)) gains `share` of it (the classic move, A past 30:
//    c = 30 x sqrt(1 - share)). The wish lies at acos(c / h) from the way on the turning side.
//  - THE MOVES: the classic move (`kClassicJumpRules`: the 30 cap, accelerate 10, wish 320), and
//    the sequel's (`sequel_jump_rules`, sequel_move.hpp): with the server's air acceleration 0 (its
//    default) no cap and accelerate 1 on a wish of up to 300, which turns slowly and gains much,
//    else the classic cap with accelerate 10. Its jump is the up move and needs a let-go as the
//    classic one does; the window's jump key reaches it as the up move (the sequel's runner) and
//    the aid's let-go in the air clears both, so either move hops on every landing it allows
//    (the sequel holds a hop back for its landing pause after a long fall).
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
// ge:keywords speedjump-rules sequel-speedjump air-law
// ge:skill shooter
// ge:test tests/shooter_speedjump_assist_test.cpp
#pragma once

#include "shooter/pmove.hpp"

namespace ge::shooter {

constexpr float kSpeedJumpSwing = 45;  // degrees the way swings past the aim before turning back
constexpr float kSpeedJumpSteer = 90;  // degrees off the aim a slow air lands and turns on foot

// The air law a move gains by (above, THE GAIN): a command adds at most accelerate x wish speed x
// seconds along the wish, up to the wish speed held to `cap` (0: no cap) less the velocity's share.
struct SpeedJumpRules {
  float accelerate = 10;  // the air's acceleration
  float cap = 30;         // the wish speed the air's gain is held to; 0 the wish speed itself
  float max_wish = 320;   // the wish speed's own cap (the move's maximum speed)
};

inline constexpr SpeedJumpRules kClassicJumpRules{};

// The sequel's air (sequel_move.hpp): `air_accelerate` the server's air acceleration in force (0 in
// a single-player game and by default).
constexpr SpeedJumpRules sequel_jump_rules(float air_accelerate) {
  return air_accelerate != 0 ? SpeedJumpRules{10, 30, 300} : SpeedJumpRules{1, 0, 300};
}

// The aid's memory between commands (a client keeps one for its player).
struct SpeedJumpAid {
  int turn = 0;           // +1 the way turning left (counter-clockwise), -1 right, 0 not yet
  bool flying = false;    // the last command the aid read was in the air
  float swing = 0;        // degrees the last sent yaw was off the player's own (kept on a landing)
  float shown = 0;        // degrees the screen shows off the player's own yaw (eased)
  int side = 0;           // the turn the last takeoff chose
  float turned = 0;       // degrees the way has turned in this flight (each command's, unsigned)
  float sweep = 0;        // degrees it turned in the last whole flight; 0 none yet
  float last_way = 0;     // the way's heading at the last command, degrees
  bool steering = false;  // on the ground with the jump held back, turning to a far-off aim
};

// The command to send for `held` (the player's own, his aim in `angles`) from his predicted state
// `s`, gaining `share` (0..1) of the best strafe's gain the move's air (`rules`) allows; `view`
// (0..1) of the swing is shown. The jump is the jump button or an up move of 10 or more (the
// sequel's), let go together in the air.
MoveCmd assisted_command(const MoveCmd& held, const MoveState& s, float share, float view,
                         SpeedJumpAid& aid, const SpeedJumpRules& rules = kClassicJumpRules);

}  // namespace ge::shooter
