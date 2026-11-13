// The sequel's live protocol, both ends' share (sequel_net.hpp).
//
// ge:summary The sequel's moves, their checksum and the out-of-band words
// ge:keywords sequel move usercmd delta checksum crc16 out-of-band words
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_net.hpp"

#include <cstring>

#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

namespace {

// A view angle in degrees as 65536ths of a turn, truncated toward zero, its low 16 bits.
int16_t turn16(float deg) {
  const auto v = static_cast<int32_t>(static_cast<double>(deg) * (65536.0 / 360.0));
  return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint32_t>(v) & 0xFFFFu));
}
float degrees(int16_t a) { return static_cast<float>(a) * (360.0f / 65536.0f); }

}  // namespace

SequelCmd sequel_cmd(const sh::MoveCmd& c) {
  SequelCmd s;
  s.angles[0] = turn16(c.angles.x);
  s.angles[1] = turn16(c.angles.y);
  s.angles[2] = turn16(c.angles.z);
  s.forward = c.forward;
  s.side = c.side;
  s.up = c.up;
  s.buttons = c.buttons;
  s.impulse = c.impulse;
  s.msec = c.msec;
  return s;
}

sh::MoveCmd move_cmd(const SequelCmd& s) {
  sh::MoveCmd c;
  c.angles = {degrees(s.angles[0]), degrees(s.angles[1]), degrees(s.angles[2])};
  c.forward = s.forward;
  c.side = s.side;
  c.up = s.up;
  c.buttons = s.buttons;
  c.impulse = s.impulse;
  c.msec = s.msec;
  return c;
}

void write_sequel_cmd(gn::Writer& w, const SequelCmd& from, const SequelCmd& to) {
  uint8_t bits = 0;
  if (to.angles[0] != from.angles[0]) bits |= kSqCmPitch;
  if (to.angles[1] != from.angles[1]) bits |= kSqCmYaw;
  if (to.angles[2] != from.angles[2]) bits |= kSqCmRoll;
  if (to.forward != from.forward) bits |= kSqCmForward;
  if (to.side != from.side) bits |= kSqCmSide;
  if (to.up != from.up) bits |= kSqCmUp;
  if (to.buttons != from.buttons) bits |= kSqCmButtons;
  if (to.impulse != from.impulse) bits |= kSqCmImpulse;
  write_byte(w, bits);
  if (bits & kSqCmPitch) write_short(w, to.angles[0]);
  if (bits & kSqCmYaw) write_short(w, to.angles[1]);
  if (bits & kSqCmRoll) write_short(w, to.angles[2]);
  if (bits & kSqCmForward) write_short(w, to.forward);
  if (bits & kSqCmSide) write_short(w, to.side);
  if (bits & kSqCmUp) write_short(w, to.up);
  if (bits & kSqCmButtons) write_byte(w, to.buttons);
  if (bits & kSqCmImpulse) write_byte(w, to.impulse);
  write_byte(w, to.msec);
  write_byte(w, to.light);
}

SequelCmd read_sequel_cmd(gn::Reader& r, const SequelCmd& from) {
  SequelCmd to = from;
  const int bits = read_byte(r);
  if (bits & kSqCmPitch) to.angles[0] = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmYaw) to.angles[1] = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmRoll) to.angles[2] = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmForward) to.forward = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmSide) to.side = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmUp) to.up = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmButtons) to.buttons = static_cast<uint8_t>(read_byte(r));
  if (bits & kSqCmImpulse) to.impulse = static_cast<uint8_t>(read_byte(r));
  to.msec = static_cast<uint8_t>(read_byte(r));
  to.light = static_cast<uint8_t>(read_byte(r));
  return to;
}

uint8_t sequel_move_checksum(const uint8_t* body, size_t size, uint32_t sequence) {
  const size_t n = size < static_cast<size_t>(kChecksumCovered) ? size : kChecksumCovered;
  uint8_t buf[kChecksumCovered + 4];
  if (n) std::memcpy(buf, body, n);
  std::memcpy(buf + n, kSequelChecksumTable + (sequence % 1020u), 4);
  uint32_t sum = 0;
  for (size_t i = 0; i < n + 4; i++) sum += buf[i];
  return static_cast<uint8_t>((crc16(buf, n + 4) ^ sum) & 0xFFu);
}

void write_sequel_move(gn::Writer& w, int32_t frame, const SequelCmd cmds[3], uint32_t sequence) {
  write_byte(w, kClcMove);
  const size_t at = w.size;
  write_byte(w, 0);  // the checksum, filled below
  write_long(w, frame);
  const SequelCmd zero;
  write_sequel_cmd(w, zero, cmds[0]);
  write_sequel_cmd(w, cmds[0], cmds[1]);
  write_sequel_cmd(w, cmds[1], cmds[2]);
  if (w.overflow) return;
  w.data[at] = sequel_move_checksum(w.data + at + 1, w.size - at - 1, sequence);
}

bool read_sequel_move(gn::Reader& r, uint32_t sequence, SequelMove& out) {
  const size_t start = r.at;
  const int checksum = read_byte(r);
  out.frame = read_long(r);
  const SequelCmd zero;
  out.cmds[0] = read_sequel_cmd(r, zero);
  out.cmds[1] = read_sequel_cmd(r, out.cmds[0]);
  out.cmds[2] = read_sequel_cmd(r, out.cmds[1]);
  if (r.bad) return false;
  out.checksum_ok =
      sequel_move_checksum(r.data + start + 1, r.at - start - 1, sequence) == checksum;
  return true;
}

std::vector<std::string> sequel_words(const std::string& line) {
  std::vector<std::string> words;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= line.size() || line[i] == '\n') break;
    std::string w;
    if (line[i] == '"') {
      i++;
      while (i < line.size() && line[i] != '"' && line[i] != '\n') w += line[i++];
      if (i < line.size() && line[i] == '"') i++;
    } else {
      while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\n')
        w += line[i++];
    }
    words.push_back(std::move(w));
  }
  return words;
}

std::string sequel_oob_text(const uint8_t* data, size_t size) {
  std::string t;
  for (size_t i = 4; i < size && data[i] != 0; i++) t += static_cast<char>(data[i]);
  return t;
}

std::vector<uint8_t> sequel_oob(const std::string& text) {
  std::vector<uint8_t> b = {0xFF, 0xFF, 0xFF, 0xFF};
  b.insert(b.end(), text.begin(), text.end());
  return b;
}

}  // namespace shooters_edge::net
