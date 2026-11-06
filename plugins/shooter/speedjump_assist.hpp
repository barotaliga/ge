// THE EASY JUMP ON A SERVER THAT DOES NOT RUN IT: the command a client sends so that forward and
// jump held gain speed on a server running the classic move alone (`pm_speedjump` absent). The
// server moves the player by what it is sent, so the client strafes for him: in the air it sends
// the side key in place of the forward key and a yaw that puts the wish at the angle that gains
// `share` of the best strafe's gain, on the side of the way that turns it toward the player's view.
//
// The rules (`assisted_command`), from the player's held command and his predicted state:
//  - It acts only on a living, walking, dry player holding the forward key without a side key and
//    without attack: anything else is sent as held. A fire command always carries his own view.
//  - THE JUMP: with the jump held, it is sent pressed while he stands (he jumps on the command he
//    lands) and let go while he flies, so the server's "let go between jumps" rule never stops the
//    next hop.
//  - THE AIR: with h the plan speed and c the wish's share along the way, a command adds 30 - c
//    along the wish and raises h^2 by 900 - c^2, so c = 30 x sqrt(1 - share) gains the share. The
//    wish lies at acos(c / h) from the way, toward the view (turning the way to it, crossing over
//    when the view is passed). A view more than 45 degrees off the way, or a plan speed under 1,
//    gets no help in the air.
//  - The sent yaw is the wish's heading + 90 (the side key moves right of the view); the pitch is
//    the player's. The player's own screen never turns: only the sent command does.
//
// Disclosed, never hidden: a client that uses it says `cl_speedjump 1` in its user info. On our
// own servers the server runs the easy jump itself (`pm_speedjump` in the server info) and this
// is not used; a server that says `pm_speedjump 0` gets no help.
//
// Pure, allocation-free, transcendentals through core's libm. Held by
// tests/shooter_speedjump_assist_test.cpp and the `se-speedjump` lab.
//
// ge:summary The easy jump on a stock server: the side key and yaw a client sends for forward+jump
// ge:keywords speedjump cl_speedjump easy-jump assist accessibility auto-strafe stock-server
// ge:keywords assisted_command strafe-share jump-release
// ge:skill shooter
// ge:test tests/shooter_speedjump_assist_test.cpp
#pragma once

#include "shooter/pmove.hpp"

namespace ge::shooter {

// The command to send for `held` (the player's own, his view in `angles`) from his predicted
// state `s`, gaining `share` (0..1) of the best strafe's gain.
MoveCmd assisted_command(const MoveCmd& held, const MoveState& s, float share);

}  // namespace ge::shooter
