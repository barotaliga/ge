// The multiview recording's records (demo.hpp): the format told by its first record, then each
// record walked and its messages read for the player the view rides. The messages themselves are
// the multiplayer protocol's (demo_client.cpp).
//
// A RECORD is a milliseconds byte (the time since the record before it: 0 under 2 ms, 255 over
// 255, so the clock is their sum and not exact), a type byte (the kind in the low 3 bits, a
// player in the high 5) and a body:
//
// | Kind | Body | Read |
// | --- | --- | --- |
// | 0 command | a client's command: the server does not write them | refused |
// | 1 read | i32 length, messages | always |
// | 2 set | two i32 | skipped |
// | 3 multiple | u32 player mask, i32 length, messages | when the mask has the tracked player; mask
// 0 is the recorder's own data, which no client sees, skipped | | 4 single | i32 length, messages
// for that player | for the tracked player | | 5 stats | i32 length, that player's stats | always,
// into that player's stats | | 6 all | i32 length, messages for everyone | always |
//
// A FRAME is every record from one with milliseconds that are not 0 to the next such record: the
// stats that changed, the players and the entity list, then what was sent to everyone. The
// recording ends with a disconnect.
//
// ge:summary The multiview recording's records walked into frames for the tracked player
// ge:keywords multiview mvd record milliseconds multiple single stats all hidden track
// ge:keywords is_multiview next_multiview player-mask
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include <algorithm>
#include <array>
#include <string>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kCommand = 0, kRead = 1, kSet = 2, kMultiple = 3, kSingle = 4, kStats = 5, kAll = 6;

constexpr uint32_t kMostMessage = 1u << 20;  // a body larger than this is damage, bytes
constexpr int kServerData = 11;

uint32_t u32_at(std::span<const uint8_t> b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}

// The record at `at`: its kind, its player mask (multiple) and its messages' span. False when
// the file ends inside it or a length is not one.
struct Record {
  int ms = 0, kind = 0, target = 0;
  uint32_t mask = 0;
  size_t body = 0, size = 0, next = 0;
};

bool record_at(std::span<const uint8_t> b, size_t at, Record& r) {
  if (at + 2 > b.size()) return false;
  r.ms = b[at];
  r.kind = b[at + 1] & 7;
  r.target = b[at + 1] >> 3;
  at += 2;
  if (r.kind == kSet) {
    if (at + 8 > b.size()) return false;
    r.body = at;
    r.size = 8;
    r.next = at + 8;
    return true;
  }
  if (r.kind == kCommand || r.kind > kAll) return false;
  if (r.kind == kMultiple) {
    if (at + 4 > b.size()) return false;
    r.mask = u32_at(b, at);
    at += 4;
  }
  if (at + 4 > b.size()) return false;
  const uint32_t len = u32_at(b, at);
  if (len > kMostMessage || at + 4 + len > b.size()) return false;
  r.body = at + 4;
  r.size = len;
  r.next = r.body + len;
  return true;
}

std::string at_offset(size_t pos) { return " at byte " + std::to_string(pos); }

}  // namespace

bool is_multiview(std::span<const uint8_t> bytes) {
  Record r;
  if (!record_at(bytes, 0, r) || (r.kind != kAll && r.kind != kRead) || r.size == 0 ||
      bytes[r.body] != kServerData)
    return false;
  // The next record reads too (or the file ends on its edge); damage further on is the reader's
  // to report with its place, not a reason to read the file as another format.
  if (r.next < bytes.size() && !record_at(bytes, r.next, r)) return false;
  return true;
}

DemoRead next_multiview(Demo& d, std::string& error) {
  const std::span<const uint8_t> b(d.bytes);
  DemoState& s = d.state;
  for (;;) {
    if (d.pos >= b.size()) {
      if (d.pending == 0) return DemoRead::end;
      d.pending = 0;
      return DemoRead::frame;
    }
    Record r;
    const size_t rec = d.pos;
    if (!record_at(b, rec, r)) {
      const int kind = rec + 1 < b.size() ? b[rec + 1] & 7 : kAll;
      if (kind == kCommand || kind > kAll) {
        error =
            "the record kind " + std::to_string(kind) + at_offset(rec) +
            (kind == kCommand ? " is a client's command, which a multiview recording has none of"
                              : " is not one of the format's 1..6") +
            "; next: the file is damaged";
        return DemoRead::error;
      }
      if (d.pending == 0) return DemoRead::end;  // cut short inside a record
      d.pending = 0;
      d.pos = b.size();
      return DemoRead::frame;
    }
    if (r.ms != 0 && d.pending > 0) {  // the next frame starts here
      d.pending = 0;
      return DemoRead::frame;
    }
    d.pos = r.next;
    d.pending++;
    d.records++;
    s.time += r.ms / 1000.0;
    bool read = false;
    int stats_of = -1;
    switch (r.kind) {
      case kRead:
      case kAll:
        read = true;
        break;
      case kMultiple:
        if (r.mask == 0) d.hidden++;
        read = s.player >= 0 && s.player < 32 && (r.mask >> s.player & 1u) != 0;
        break;
      case kSingle:
        read = r.target == s.player;
        stats_of = r.target;
        break;
      case kStats:
        read = true;
        stats_of = r.target;
        break;
      default:
        break;  // set: nothing the view needs
    }
    if (!read) continue;
    // A player's own records write his stats, not the view's.
    std::array<int32_t, kStatCount> view_stats{};
    std::copy(s.stats, s.stats + kStatCount, view_stats.begin());
    if (stats_of >= 0) {
      const auto& st = d.player_stats[static_cast<size_t>(stats_of)];
      std::copy(st.begin(), st.end(), s.stats);
    }
    DemoMsg m{b.subspan(r.body, r.size)};
    const bool ok = read_client_messages(d, m, error);
    if (stats_of >= 0) {
      std::copy(s.stats, s.stats + kStatCount,
                d.player_stats[static_cast<size_t>(stats_of)].begin());
      std::copy(view_stats.begin(), view_stats.end(), s.stats);
    }
    if (!ok) {
      error += " (in the record" + at_offset(rec) + ")";
      return DemoRead::error;
    }
    if (d.ended) return DemoRead::end;
  }
}

}  // namespace shooters_edge::client
