// THE AIM HELP'S TURN OF THE VIEW: our client's view moved toward the player the aim help bent a
// shot onto, with inertia, and back to where the player was aiming once he stops firing, so a
// recording of his view (which is what the server and the demo get) looks like a person's aim.
//
// - THE BEND told (`aim_turn_bent`): the server names the player it bent a shot onto (the game's
//   own message; a classic client never gets one).
// - HELD while the attack is held and the target is still near the player's own aim: his own aim
//   is the view less the turn (`base`), and the target is let go when its middle is more than
//   `kAimTurnReach` units off that aim at its distance (he looked away) or it is gone.
// - THE TURN (`aim_turn_step`): the offset from his own aim to the target's middle is followed by
//   a critically damped spring, quick on the way in (`kAimTurnIn`) and gentler back
//   (`kAimTurnOut`); the view is moved by the offset's change each frame, so the mouse keeps
//   turning the view the whole time and the player's own aim is where the view comes back to.
//
// Float math on the frame's own numbers; the client's only (it changes what the player sends,
// never what the server works out).
//
// ge:summary The aim help's view turn: toward the bent-onto player with inertia, then back
// ge:keywords aim-help aim-turn aimturn view inertia spring demo natural client mouse bend
// ge:keywords cl_aimhelp_turn aim_turn_step aim_turn_bent
// ge:skill shooter
// ge:test tests/shooter_aim_turn_test.cpp
#pragma once

#include "shooter/vec.hpp"

namespace ge::shooter {

constexpr float kAimTurnIn = 14;       // rad/s: the spring's frequency toward the target
constexpr float kAimTurnOut = 8;       // rad/s: and back to the player's own aim
constexpr float kAimTurnReach = 64;    // units off his own aim at the target's distance
constexpr float kAimTurnRest = 0.02f;  // degrees: an offset this small with no target is none

struct AimTurn {
  int target = -1;  // the entity the view turns toward (-1: none)
  float pitch = 0;  // degrees: the turn the view holds now
  float yaw = 0;
  float pitch_rate = 0;  // degrees/s
  float yaw_rate = 0;
};

// The server bent a shot of the player's onto entity `target`.
void aim_turn_bent(AimTurn& a, int target);

// One frame of `dt` seconds: `view` the player's angles (pitch down positive, yaw, roll; degrees)
// as his mouse left them, moved by the turn's change; `eye` his eye; `target_at` the target's
// middle now, or null when it is gone (dead, out of sight); `attack` the attack held.
void aim_turn_step(AimTurn& a, Vec3f& view, Vec3f eye, const Vec3f* target_at, bool attack,
                   float dt);

}  // namespace ge::shooter
