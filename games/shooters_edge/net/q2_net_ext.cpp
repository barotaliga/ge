// Quake II's later protocols on the live connection (q2_net_ext.hpp).
//
// ge:summary Quake II's protocols 35 and 36: the connect's tail and the later moves, defined
// ge:keywords protocol-36 batched-moves bit-stream compact-move connect-tail
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_ext_test.cpp
#include "shooters_edge/net/q2_net_ext.hpp"

#include <algorithm>
#include <cstdlib>

#include "shooters_edge/net/qw_wire.hpp"

namespace shooters_edge::net {

namespace {

// A number word as the connect reads it: decimal, anything else 0.
int number(const std::vector<std::string>& w, size_t i, int absent) {
  if (i >= w.size()) return absent;
  return std::atoi(w[i].c_str());
}

// 35's compact command (minor 1904 on).
Q2Cmd read_compact(gn::Reader& r, const Q2Cmd& from) {
  Q2Cmd to = from;
  const int bits = read_byte(r);
  int format = 0;
  if (bits & kSqCmButtons) {
    const int b = read_byte(r);
    to.buttons = static_cast<uint8_t>(b & 0x83);
    format = b;
  }
  const auto small = [&r](int scale) { return read_char(r) * scale; };
  if (bits & kSqCmPitch)
    to.angles[0] = static_cast<int16_t>((format & 0x20) ? small(64) : read_short(r));
  if (bits & kSqCmYaw)
    to.angles[1] = static_cast<int16_t>((format & 0x40) ? small(256) : read_short(r));
  if (bits & kSqCmRoll) to.angles[2] = static_cast<int16_t>(read_short(r));
  if (bits & kSqCmForward)
    to.forward = static_cast<int16_t>((format & 0x04) ? small(5) : read_short(r));
  if (bits & kSqCmSide) to.side = static_cast<int16_t>((format & 0x08) ? small(5) : read_short(r));
  if (bits & kSqCmUp) to.up = static_cast<int16_t>((format & 0x10) ? small(5) : read_short(r));
  if (bits & kSqCmImpulse) to.impulse = static_cast<uint8_t>(read_byte(r));
  to.msec = static_cast<uint8_t>(read_byte(r));
  to.light = static_cast<uint8_t>(read_byte(r));
  return to;
}

void write_compact(gn::Writer& w, const Q2Cmd& from, const Q2Cmd& to) {
  uint8_t bits = 0;
  if (to.angles[0] != from.angles[0]) bits |= kSqCmPitch;
  if (to.angles[1] != from.angles[1]) bits |= kSqCmYaw;
  if (to.angles[2] != from.angles[2]) bits |= kSqCmRoll;
  if (to.forward != from.forward) bits |= kSqCmForward;
  if (to.side != from.side) bits |= kSqCmSide;
  if (to.up != from.up) bits |= kSqCmUp;
  bits |= kSqCmButtons;  // the format bits ride in the buttons byte
  if (to.impulse != from.impulse) bits |= kSqCmImpulse;
  const auto fits = [](int v, int scale) { return v % scale == 0 && v / scale >= -128 && v / scale <= 127; };
  int format = to.buttons & 0x83;
  if (fits(to.angles[0], 64)) format |= 0x20;
  if (fits(to.angles[1], 256)) format |= 0x40;
  if (fits(to.forward, 5)) format |= 0x04;
  if (fits(to.side, 5)) format |= 0x08;
  if (fits(to.up, 5)) format |= 0x10;
  write_byte(w, bits);
  write_byte(w, format);
  const auto put = [&w](int v, bool small, int scale) {
    if (small)
      write_byte(w, (v / scale) & 0xFF);
    else
      write_short(w, v);
  };
  if (bits & kSqCmPitch) put(to.angles[0], format & 0x20, 64);
  if (bits & kSqCmYaw) put(to.angles[1], format & 0x40, 256);
  if (bits & kSqCmRoll) write_short(w, to.angles[2]);
  if (bits & kSqCmForward) put(to.forward, format & 0x04, 5);
  if (bits & kSqCmSide) put(to.side, format & 0x08, 5);
  if (bits & kSqCmUp) put(to.up, format & 0x10, 5);
  if (bits & kSqCmImpulse) write_byte(w, to.impulse);
  write_byte(w, to.msec);
  write_byte(w, to.light);
}

// The batched moves' bits, least significant first.
struct BitsIn {
  gn::Reader& r;
  uint32_t acc = 0;
  int have = 0;
  uint32_t take(int n) {
    while (have < n) {
      acc |= static_cast<uint32_t>(read_byte(r)) << have;
      have += 8;
    }
    const uint32_t v = acc & ((1u << n) - 1);
    acc >>= n;
    have -= n;
    return v;
  }
  int32_t take_signed(int n) {
    const uint32_t v = take(n);
    return (v & (1u << (n - 1))) != 0 ? static_cast<int32_t>(v) - (1 << n) : static_cast<int32_t>(v);
  }
};

struct BitsOut {
  gn::Writer& w;
  uint32_t acc = 0;
  int have = 0;
  void put(uint32_t v, int n) {
    acc |= (v & ((1u << n) - 1)) << have;
    have += n;
    while (have >= 8) {
      write_byte(w, static_cast<int>(acc & 0xFF));
      acc >>= 8;
      have -= 8;
    }
  }
  void flush() {
    if (have > 0) write_byte(w, static_cast<int>(acc & 0xFF));
    acc = 0;
    have = 0;
  }
};

constexpr int kBatchFramesMost = 3;  // frames beyond the newest

}  // namespace

bool q2_connect_ask(const std::vector<std::string>& w, Q2ConnectAsk& out, std::string& refusal) {
  out = Q2ConnectAsk{};
  out.protocol = number(w, 1, 0);
  if (out.protocol < 26 || out.protocol > kQ2Protocol36) {
    refusal = "Unsupported protocol version " + std::to_string(out.protocol) + ".";
    return false;
  }
  if (out.protocol < kQ2NetProtocol) {
    refusal = "You need Quake 2 version 3.19 or higher.";
    return false;
  }
  if (out.protocol == kQ2NetProtocol) return true;
  out.msglen = number(w, 5, kQ2MsgLenDefault);
  if (out.msglen < 0 || out.msglen > kQ2MsgLenMost) {
    refusal = "Invalid maximum message length.";
    return false;
  }
  if (out.msglen == 0) out.msglen = kQ2MsgLenMost;
  if (out.protocol == kQ2Protocol35) {
    out.zlib = true;
    out.minor = std::clamp(number(w, 6, kQ2MinorFirst35), kQ2MinorFirst35, kQ2MinorLast35);
    return true;
  }
  out.chan = number(w, 6, 1);
  if (out.chan != 0 && out.chan != 1) {
    refusal = "Invalid netchan type.";
    return false;
  }
  out.zlib = w.size() <= 7 || number(w, 7, 1) != 0;
  out.minor = std::clamp(number(w, 8, kQ2MinorFirst36), kQ2MinorFirst36, kQ2MinorLast36);
  if (out.minor == kQ2MinorFirst36 + 1) out.minor = kQ2MinorFirst36;
  return true;
}

int q2_msglen(const Q2ConnectAsk& ask, int net_maxmsglen, bool local) {
  int len = kQ2MsgLenDefault;
  if (ask.protocol == kQ2Protocol35 || ask.protocol == kQ2Protocol36) len = ask.msglen;
  if (!local && net_maxmsglen > 0) len = std::min(len, net_maxmsglen);
  return std::max(len, kQ2MsgLenLeast);
}

bool read_q2_move_ext(gn::Reader& r, int protocol, int minor, Q2Move& out) {
  out.frame = read_long(r);
  const bool compact = protocol == kQ2Protocol35 && minor >= 1904;
  const Q2Cmd zero;
  Q2Cmd prev = zero;
  for (Q2Cmd& c : out.cmds) {
    c = compact ? read_compact(r, prev) : read_q2_cmd(r, prev);
    prev = c;
  }
  out.checksum_ok = true;  // these protocols carry none
  return !r.bad;
}

void write_q2_move_ext(gn::Writer& w, int protocol, int minor, int32_t frame, const Q2Cmd cmds[3]) {
  write_byte(w, kClcMove);
  write_long(w, frame);
  const bool compact = protocol == kQ2Protocol35 && minor >= 1904;
  const Q2Cmd zero;
  const Q2Cmd* prev = &zero;
  for (int k = 0; k < 3; k++) {
    if (compact)
      write_compact(w, *prev, cmds[k]);
    else
      write_q2_cmd(w, *prev, cmds[k]);
    prev = &cmds[k];
  }
}

bool read_q2_batch(gn::Reader& r, uint8_t id, Q2Batch& out, std::string& reason) {
  out = Q2Batch{};
  const int extra = id >> 5;
  if (extra > kBatchFramesMost) {
    reason = "too many frames in packet";
    return false;
  }
  out.frame = (id & 0x1F) == kClcMoveBatched ? read_long(r) : -1;
  out.light = static_cast<uint8_t>(read_byte(r));
  BitsIn b{r};
  Q2Cmd prev;
  for (int f = 0; f <= extra; f++) {
    const int count = static_cast<int>(b.take(5));
    std::vector<Q2Cmd>& cmds = out.frames.emplace_back();
    for (int k = 0; k < count; k++) {
      Q2Cmd c = prev;
      if (b.take(1) != 0) {
        const uint32_t bits = b.take(8);
        if (bits & kSqCmPitch)
          c.angles[0] = static_cast<int16_t>(b.take(1) ? c.angles[0] + b.take_signed(8)
                                                       : b.take_signed(16));
        if (bits & kSqCmYaw)
          c.angles[1] = static_cast<int16_t>(b.take(1) ? c.angles[1] + b.take_signed(8)
                                                       : b.take_signed(16));
        if (bits & kSqCmRoll) c.angles[2] = static_cast<int16_t>(b.take_signed(16));
        if (bits & kSqCmForward) c.forward = static_cast<int16_t>(b.take_signed(10));
        if (bits & kSqCmSide) c.side = static_cast<int16_t>(b.take_signed(10));
        if (bits & kSqCmUp) c.up = static_cast<int16_t>(b.take_signed(10));
        if (bits & kSqCmButtons) {
          const uint32_t v = b.take(3);
          c.buttons = static_cast<uint8_t>((v & 3u) | ((v & 4u) != 0 ? 0x80u : 0u));
        }
        if (bits & 0x80u) c.msec = static_cast<uint8_t>(b.take(8));
      }
      c.light = out.light;
      cmds.push_back(c);
      prev = c;
    }
    if (r.bad) break;
  }
  if (r.bad) {
    reason = "read past end of message";
    return false;
  }
  return true;
}

void write_q2_batch(gn::Writer& w, const Q2Batch& batch) {
  const int extra = std::clamp(static_cast<int>(batch.frames.size()) - 1, 0, 7);
  const bool nodelta = batch.frame < 0;
  write_byte(w, (nodelta ? kClcMoveNoDelta : kClcMoveBatched) | (extra << 5));
  if (!nodelta) write_long(w, batch.frame);
  write_byte(w, batch.light);
  BitsOut b{w};
  Q2Cmd prev;
  for (size_t f = 0; f < batch.frames.size() && f <= 7; f++) {
    const std::vector<Q2Cmd>& cmds = batch.frames[f];
    const size_t count = std::min<size_t>(cmds.size(), 31);
    b.put(static_cast<uint32_t>(count), 5);
    for (size_t k = 0; k < count; k++) {
      Q2Cmd c = cmds[k];
      c.light = prev.light;
      c.impulse = prev.impulse;
      if (c == prev) {
        b.put(0, 1);
        continue;
      }
      b.put(1, 1);
      uint32_t bits = 0;
      if (c.angles[0] != prev.angles[0]) bits |= kSqCmPitch;
      if (c.angles[1] != prev.angles[1]) bits |= kSqCmYaw;
      if (c.angles[2] != prev.angles[2]) bits |= kSqCmRoll;
      if (c.forward != prev.forward) bits |= kSqCmForward;
      if (c.side != prev.side) bits |= kSqCmSide;
      if (c.up != prev.up) bits |= kSqCmUp;
      if (c.buttons != prev.buttons) bits |= kSqCmButtons;
      if (c.msec != prev.msec) bits |= 0x80u;
      b.put(bits, 8);
      for (int a = 0; a < 2; a++) {
        if ((bits & (a == 0 ? kSqCmPitch : kSqCmYaw)) == 0) continue;
        const int d = c.angles[a] - prev.angles[a];
        if (d >= -128 && d <= 127) {
          b.put(1, 1);
          b.put(static_cast<uint32_t>(d), 8);
        } else {
          b.put(0, 1);
          b.put(static_cast<uint32_t>(c.angles[a]), 16);
        }
      }
      if (bits & kSqCmRoll) b.put(static_cast<uint32_t>(c.angles[2]), 16);
      if (bits & kSqCmForward) b.put(static_cast<uint32_t>(c.forward), 10);
      if (bits & kSqCmSide) b.put(static_cast<uint32_t>(c.side), 10);
      if (bits & kSqCmUp) b.put(static_cast<uint32_t>(c.up), 10);
      if (bits & kSqCmButtons)
        b.put(static_cast<uint32_t>((c.buttons & 3) | ((c.buttons & 0x80) != 0 ? 4 : 0)), 3);
      if (bits & 0x80u) b.put(c.msec, 8);
      prev = c;
    }
  }
  b.flush();
}

}  // namespace shooters_edge::net
