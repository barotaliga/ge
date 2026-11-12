// The rocket jump helper's command and its waiting request (rjump_assist.hpp states the rules).
//
// ge:summary The rocket jump helper's rewritten command and the request that waits for the launcher
// ge:keywords rjump_command rjump_take rjump_apply rjump rrjump
// ge:skill shooter
// ge:test tests/shooter_rjump_assist_test.cpp
#include "shooter/rjump_assist.hpp"

namespace ge::shooter {

namespace {

int16_t turned(int16_t move) { return move == INT16_MIN ? INT16_MAX : int16_t(-move); }

}  // namespace

Rjump rjump_of(uint8_t impulse) {
  if (impulse == kRjumpImpulse) return Rjump::down;
  if (impulse == kBackRjumpImpulse) return Rjump::back;
  return Rjump::none;
}

uint8_t rjump_impulse(Rjump kind) {
  return kind == Rjump::down ? kRjumpImpulse : kind == Rjump::back ? kBackRjumpImpulse : 0;
}

const char* rjump_name(Rjump kind) {
  return kind == Rjump::down ? "rjump" : kind == Rjump::back ? "rrjump" : "";
}

MoveCmd rjump_command(const MoveCmd& cmd, Rjump kind) {
  MoveCmd out = cmd;
  out.impulse = 0;
  if (kind == Rjump::none) return out;
  out.buttons = uint8_t(out.buttons | kButtonAttack | kButtonJump);
  if (kind == Rjump::down) {
    out.angles.x = kRjumpPitch;
  } else {
    // Half a turn, kept in -180..360 so a wrapped yaw stays near the player's own; the moves turn
    // with it, so forward is still the way he wished to go.
    out.angles.y = out.angles.y >= 180 ? out.angles.y - 180 : out.angles.y + 180;
    out.forward = turned(out.forward);
    out.side = turned(out.side);
  }
  return out;
}

Rjump rjump_take(RjumpSeat& seat, MoveCmd& cmd) {
  const Rjump kind = rjump_of(cmd.impulse);
  if (kind == Rjump::none) return kind;
  cmd.impulse = 0;
  seat.kind = kind;
  seat.waited_ms = 0;
  return kind;
}

bool rjump_apply(RjumpSeat& seat, MoveCmd& cmd, RjumpReady ready) {
  if (seat.kind == Rjump::none) return false;
  if (ready == RjumpReady::now) {
    cmd = rjump_command(cmd, seat.kind);
    seat = {};
    return true;
  }
  if (ready == RjumpReady::never) {
    seat = {};
    return false;
  }
  cmd.buttons = uint8_t(cmd.buttons & ~kButtonJump);  // the jump waits for the shot
  seat.waited_ms += cmd.msec;
  if (seat.waited_ms > kRjumpWaitMs) seat = {};
  return false;
}

}  // namespace ge::shooter
