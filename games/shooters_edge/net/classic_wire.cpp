// The classic protocol's own wire (classic_wire.hpp): the move checksum's seam and the delta
// usercmd, over the protocol family's fields (plugins/shooter/protocol/wire.hpp).
//
// ge:summary The classic wire's definitions: the move checksum and usercmd deltas
// ge:keywords move_checksum write_delta_usercmd read_delta_usercmd
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#include "shooters_edge/net/classic_wire.hpp"

#include <cstring>

namespace shooters_edge::net {

uint8_t move_checksum(const uint8_t* body, size_t size, uint32_t sequence) {
  const size_t n = size < static_cast<size_t>(kChecksumCovered) ? size : kChecksumCovered;
  uint8_t buf[kChecksumCovered + 4];
  if (n) std::memcpy(buf, body, n);
  const uint8_t* k = kChecksumTable + (sequence % 1020u);
  buf[n] = static_cast<uint8_t>(k[0] ^ (sequence & 0xff));
  buf[n + 1] = k[1];
  buf[n + 2] = static_cast<uint8_t>(k[2] ^ ((sequence >> 8) & 0xff));
  buf[n + 3] = k[3];
  return static_cast<uint8_t>(crc16(buf, n + 4) & 0xff);
}

void write_delta_usercmd(gn::Writer& w, const sh::MoveCmd& from, const sh::MoveCmd& to) {
  uint8_t bits = 0;
  if (to.angles.x != from.angles.x) bits |= kCmPitch;
  if (to.angles.y != from.angles.y) bits |= kCmYaw;
  if (to.angles.z != from.angles.z) bits |= kCmRoll;
  if (to.forward != from.forward) bits |= kCmForward;
  if (to.side != from.side) bits |= kCmSide;
  if (to.up != from.up) bits |= kCmUp;
  if (to.buttons != from.buttons) bits |= kCmButtons;
  if (to.impulse != from.impulse) bits |= kCmImpulse;
  write_byte(w, bits);
  if (bits & kCmPitch) write_angle16(w, to.angles.x);
  if (bits & kCmYaw) write_angle16(w, to.angles.y);
  if (bits & kCmRoll) write_angle16(w, to.angles.z);
  if (bits & kCmForward) write_short(w, to.forward);
  if (bits & kCmSide) write_short(w, to.side);
  if (bits & kCmUp) write_short(w, to.up);
  if (bits & kCmButtons) write_byte(w, to.buttons);
  if (bits & kCmImpulse) write_byte(w, to.impulse);
  write_byte(w, to.msec);
}

sh::MoveCmd read_delta_usercmd(gn::Reader& r, const sh::MoveCmd& from, uint8_t* bits_out) {
  sh::MoveCmd c = from;
  const auto bits = static_cast<uint8_t>(read_byte(r));
  if (bits_out) *bits_out = bits;
  if (bits & kCmPitch) c.angles.x = read_angle16(r);
  if (bits & kCmYaw) c.angles.y = read_angle16(r);
  if (bits & kCmRoll) c.angles.z = read_angle16(r);
  if (bits & kCmForward) c.forward = static_cast<int16_t>(read_short(r));
  if (bits & kCmSide) c.side = static_cast<int16_t>(read_short(r));
  if (bits & kCmUp) c.up = static_cast<int16_t>(read_short(r));
  if (bits & kCmButtons) c.buttons = static_cast<uint8_t>(read_byte(r));
  if (bits & kCmImpulse) c.impulse = static_cast<uint8_t>(read_byte(r));
  c.msec = static_cast<uint8_t>(read_byte(r));
  return c;
}

sh::MoveCmd usercmd_grid(const sh::MoveCmd& c) {
  sh::MoveCmd g = c;
  g.angles.x = angle16_from_wire(angle16_to_wire(c.angles.x));
  g.angles.y = angle16_from_wire(angle16_to_wire(c.angles.y));
  g.angles.z = angle16_from_wire(angle16_to_wire(c.angles.z));
  return g;
}

}  // namespace shooters_edge::net
