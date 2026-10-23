// THE SESSION'S MESSAGES on its reliable channel, stated once for the host and the client (and
// not installed: a game speaks to a session through session.hpp). Every message is a kind byte
// and its fields, little endian; a seat or a team is a byte, 0xff for none; text is a varint
// length and its bytes.
//
//   client → host                         host → client
//   hello    token u64, name              welcome  seat, token u64, max players, teams, phase
//   ready    0|1                          player   seat, cause, flags, team, name
//   team     team                         gone     seat, why
//   chat     scope, to, text              pings    count, (seat, ms u16) each
//   kick     seat                         chat     from, scope, to, text
//   start                                 settings bytes
//   back                                  phase    phase
//   settings bytes                        kicked   why
//                                         refused  ask, why
//
// ge:summary The session's messages: kinds, fields and their encoding, host and client sides
// ge:keywords net session lobby message wire encode decode hello welcome roster chat
// ge:skill net
// ge:test tests/net_session_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "net/session.hpp"
#include "net/wire.hpp"

namespace ge::net::session_wire {

enum Kind : uint8_t {
  kHello = 1,
  kReady = 2,
  kTeam = 3,
  kChat = 4,
  kKick = 5,
  kStart = 6,
  kBack = 7,
  kSettings = 8,
  kWelcome = 20,
  kPlayer = 21,
  kGone = 22,
  kPings = 23,
  kChatOut = 24,
  kSettingsOut = 25,
  kPhase = 26,
  kKicked = 27,
  kRefused = 28,
};

// Why a player message is sent: the client turns it into the matching event (a snapshot, sent
// to a client just seated, into none).
enum Cause : uint8_t { kSnapshot = 0, kJoined = 1, kRejoined = 2, kAway = 3, kChanged = 4 };

constexpr uint8_t kFlagPresent = 1, kFlagReady = 2, kFlagLeader = 4;
constexpr uint8_t kNone = 0xff;

inline uint8_t seat_byte(int seat) { return seat < 0 ? kNone : static_cast<uint8_t>(seat); }
inline int seat_of(uint8_t b) { return b == kNone ? -1 : b; }

inline void write_text(Writer& w, const void* text, size_t size) {
  write_varint(w, size);
  write_bytes(w, text, size);
}
// A text field: its bytes in the reader's buffer, or nullptr (bad) when longer than `most`.
inline const uint8_t* read_text(Reader& r, size_t most, size_t& size) {
  const uint64_t n = read_varint(r);
  if (r.bad || n > most) {
    r.bad = true;
    size = 0;
    return nullptr;
  }
  size = static_cast<size_t>(n);
  return read_view(r, size);
}

inline void write_player(Writer& w, int seat, uint8_t cause, const Player& p) {
  write_u8(w, kPlayer);
  write_u8(w, seat_byte(seat));
  write_u8(w, cause);
  write_u8(w, static_cast<uint8_t>((p.present ? kFlagPresent : 0) | (p.ready ? kFlagReady : 0) |
                                   (p.leader ? kFlagLeader : 0)));
  write_u8(w, seat_byte(p.team));
  const size_t n = std::strlen(p.name);
  write_text(w, p.name, n);
}

// Text as a session keeps it (a name, a chat line): control bytes become spaces, cut at `most`
// bytes without splitting a UTF-8 sequence. Returns its length.
inline size_t clean_text(const uint8_t* in, size_t size, size_t most, char* out) {
  size_t n = size < most ? size : most;
  if (n < size) {
    // Never end inside a sequence: back off to the start of the cut one.
    while (n > 0 && (in[n] & 0xc0) == 0x80) n--;
  }
  for (size_t i = 0; i < n; i++) {
    const uint8_t c = in[i];
    out[i] = static_cast<char>(c < 0x20 || c == 0x7f ? ' ' : c);
  }
  return n;
}

}  // namespace ge::net::session_wire
