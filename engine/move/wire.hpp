// A MOVING BODY ON THE WIRE: the body's state as a row of numbers a network can quantize and send,
// the command as a few bytes, and the one step a server and its predicting clients both take from
// them, so a client that replays its unacknowledged commands over the server's last word reaches
// the server's own body (docs/movement.md § Prediction, M28d).
//
// THE BODY is `kWireFields` numbers (`wire_fields()` says each one's range and the bits it needs):
// the feet, the velocity, a word of flags (mode, ground, crouch, slide, the jump's button and rise,
// the ladder, the air jumps left, the ground's solid), the crouch's share, the three timers
// (coyote, jump buffer, slide), the ground's normal on the plan (its height rebuilt) and the
// ground's velocity. Everything a step reads is in it, so a body rebuilt from its row steps exactly
// as the one it was packed from would have, once both stand on the row's grid. `t` stays behind.
// A coyote clock past its field's top is "never" (`kSinceNever`); timers below zero are kept as
// they are (the step reads only their sign there).
//
// THE COMMAND is `kWireCmdBytes` bytes: the stick (a signed byte a direction), the view (yaw by
// 1/65536 of a turn, pitch by 1/32767 of a quarter turn), the buttons and the command's length in
// milliseconds. `wire_cmd` is what the receiver reads: the sender predicts with the unpacked
// command too, never its own doubles.
//
// A LONG COMMAND SPLIT UNDER A CAP: a command's milliseconds (a frame that hitched, a slow client)
// are stepped in equal pieces no longer than `max_dt`, the count settled by the length alone, so
// a server and a client split the same command alike and a long frame never takes one long step
// (a jump's arc, a step up and the strafe gain all hold to the piece's length, not the frame's).
//
// ge:summary A moving body on the wire: state as quantizable fields, the command as bytes, a split
// ge:keywords move movement wire network prediction predict reconcile quantize pack unpack
// ge:keywords usercmd msec command-split max-step long-command step_move_split step_wire
// ge:keywords wire_fields pack_body unpack_body pack_cmd unpack_cmd split_count
// ge:skill move net
// ge:test tests/move_wire_test.cpp
#pragma once

#include <cstdint>

#include "move/step.hpp"

namespace ge::move {

constexpr int kWireFields = 16;
constexpr int kWireCmdBytes = 9;

// One field's range on the wire and the bits it needs there (a network schema copies these).
struct WireField {
  const char* name;
  double min, max;
  uint8_t bits;
};

// The body's fields, in order: x y z, vx vy vz, flags, crouch, since_ground, buffer_left,
// slide_ready, normal_x, normal_z, ground_vx, ground_vy, ground_vz.
const WireField* wire_fields();

// The body as its row, every field clamped to its range; and the row as a body.
void pack_body(const MoveState& s, double* fields);
MoveState unpack_body(const double* fields);

// The command and its length (ms, 1..255) as bytes; and the bytes as the command the receiver
// steps, with its length in seconds.
void pack_cmd(const MoveCmd& cmd, int msec, uint8_t* bytes);
MoveCmd unpack_cmd(const uint8_t* bytes, double* dt);

// The pieces a command of `dt` seconds is stepped in under `max_dt` (1 when it fits; 0: no cap).
int split_count(double dt, double max_dt);
// One command stepped in `split_count` equal pieces; the events of every piece together (the
// hardest landing, the most a wall took, the height climbed summed).
MoveEvents step_move_split(const MoveSpec& spec, const World& world, MoveState& s,
                           const MoveCmd& cmd, double dt, double max_dt);
// The step a server and its predicting clients share: the row unpacked, the command's bytes
// stepped (split under `max_dt`), the row packed again.
MoveEvents step_wire(const MoveSpec& spec, const World& world, double max_dt, double* fields,
                     const uint8_t* cmd);

}  // namespace ge::move
