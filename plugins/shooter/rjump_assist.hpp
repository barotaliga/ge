// THE ROCKET JUMP HELPER: one press does a rocket jump for the player, the rocket fired straight
// down (`down`, the `+rjump` button) or behind him (`back`, `+rrjump`, the reverse rocket jump),
// the jump and the shot in one command. An aid for a player whose hands cannot turn, jump and fire
// at once (as the easy jump is for strafing); the server decides who may use it.
//
// THE REQUEST is an impulse: kRjumpImpulse (164) for down, kBackRjumpImpulse (156) for
// back, sent with the jump held in the same command (so a server that offers the helper under
// these numbers sees what it expects). A server that offers it takes the impulse away from the
// rules and keeps the request in the player's `RjumpSeat`.
//
// THE SHOT is made on the first command the rules say the launcher can fire (`RjumpReady::now`):
// that command is rewritten (`rjump_command`) and runs as a perfect hand-made rocket jump.
// Nothing is added: the rules fire a real rocket (ammo, refire time, self-damage and push), and the
// move jumps only where a jump is allowed (on the ground, the jump let go before).
//  - DOWN: the view's pitch is kRjumpPitch (80 degrees down, the classic lowest look); the
//    yaw is the player's own, so the blast lands a little ahead and pushes a little back.
//  - BACK: the yaw is turned 180 degrees, the pitch the player's own (looking down 30 fires back
//    and down 30); the forward and side moves are turned with it, so the move's wish is unchanged.
//  - Both: attack and jump held for that one command. The next command is the player's own again:
//    his screen never turns (the turned view lives only in that command, on the server).
// WAITING (`RjumpReady::soon`: the launcher reloading, being raised or not yet in hand): the
// request waits up to kRjumpWaitMs of the player's commands, and while it waits his jump is
// held back, so the jump comes with the shot. `RjumpReady::never` (no launcher, no rockets,
// dead) drops it at once; the caller tells the player why.
//
// Pure and allocation-free. Held by tests/shooter_rjump_assist_test.cpp; the game's glue
// (games/shooters_edge/server/rjump_rules.hpp) gates it by the server's settings and grants, and
// the `se-rjump` lab shows it on real maps.
//
// ge:summary The rocket jump helper: one press turns the shot down or back, fires and jumps
// ge:keywords rocket-jump rjump rrjump reverse-rocket-jump rjump_command rjumpseat
// ge:keywords accessibility jump-and-fire impulse-164 impulse-156 rocket-ready pitch-80
// ge:skill shooter
// ge:test tests/shooter_rjump_assist_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pmove.hpp"

namespace ge::shooter {

enum class Rjump : uint8_t { none, down, back };

inline constexpr uint8_t kRjumpImpulse = 164;      // +rjump: the rocket straight down
inline constexpr uint8_t kBackRjumpImpulse = 156;  // +rrjump: the rocket behind
inline constexpr float kRjumpPitch = 80;           // degrees down, the down shot's view
inline constexpr int kRjumpWaitMs = 1000;          // a request waits this long for the launcher

// What the rules say of the launcher for a waiting request.
enum class RjumpReady : uint8_t {
  now,    // it fires on this command
  soon,   // reloading, being raised or chosen: wait
  never,  // no launcher, no rockets, dead: drop the request
};

// A player's request between commands (a server keeps one a seat).
struct RjumpSeat {
  Rjump kind = Rjump::none;
  int waited_ms = 0;  // the player's command time since the request
};

// The helper an impulse asks for (none for any other impulse).
Rjump rjump_of(uint8_t impulse);
// The impulse that asks for `kind` (0 for none).
uint8_t rjump_impulse(Rjump kind);
// The name a player binds and reads: "rjump", "rrjump" ("" for none).
const char* rjump_name(Rjump kind);

// `cmd` as the rocket jump of `kind` sends it: the view turned, attack and jump held, no impulse.
MoveCmd rjump_command(const MoveCmd& cmd, Rjump kind);

// A request in `cmd` (its impulse) taken into `seat`: the impulse cleared, the kind returned (none
// when the command asks for nothing). A new request replaces a waiting one.
Rjump rjump_take(RjumpSeat& seat, MoveCmd& cmd);

// The waiting request applied to `cmd` by what the rules say of the launcher: `now` rewrites it
// and ends the request (true: the shot is in this command); `soon` holds the jump back and counts
// the wait (a request older than kRjumpWaitMs ends); `never` ends it. False: no shot here.
bool rjump_apply(RjumpSeat& seat, MoveCmd& cmd, RjumpReady ready);

}  // namespace ge::shooter
