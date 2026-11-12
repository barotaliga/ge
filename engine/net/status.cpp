// A node's public status record and its cookie-checked query (net/status.hpp).
//
// ge:summary The status record's bytes, the ask, the cookie and the answer
// ge:keywords net status record write read ask cookie answer truncated
// ge:skill net
// ge:test tests/net_status_test.cpp
#include "net/status.hpp"

#include <cstring>

#include "net/reach_wire.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

constexpr uint8_t kMagic[4] = {'g', 'e', 'S', 'T'};
constexpr uint8_t kAsk = 1, kCookie = 2, kRecord = 3;
constexpr uint8_t kRecordVersion = 1;
constexpr uint8_t kFlagLocked = 1, kFlagTruncated = 2;
constexpr int64_t kBucketUs = 10000000;  // a cookie's bucket: ten seconds
constexpr size_t kAskHead = 4 + 1 + 4 + 8 + 8;
constexpr size_t kAddressSize = 19;

uint64_t mix(uint64_t x) {
  x += 0x9e3779b97f4a7c15ull;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

uint64_t cookie_for(uint64_t secret, const Address& from, int64_t bucket) {
  const uint64_t c = mix(secret ^ mix(address_hash(from) ^ mix(static_cast<uint64_t>(bucket))));
  return c ? c : 1;  // 0 is "no cookie yet"
}

size_t cut(const std::string& s, size_t limit) { return s.size() < limit ? s.size() : limit; }
size_t str_size(const std::string& s, size_t limit) { return 1 + cut(s, limit); }

void write_str(Writer& w, const std::string& s, size_t limit) {
  const size_t n = cut(s, limit);
  write_u8(w, static_cast<uint8_t>(n));
  write_bytes(w, s.data(), n);
}

bool read_str(Reader& r, std::string& out) {
  const uint8_t n = read_u8(r);
  const uint8_t* p = read_view(r, n);
  if (!p) return false;
  out.assign(reinterpret_cast<const char*>(p), n);
  return true;
}

void write_start(Writer& w, uint8_t kind) {
  write_bytes(w, kMagic, 4);
  write_u8(w, kind);
}

bool read_start(Reader& r, uint8_t& kind) {
  const uint8_t* m = read_view(r, 4);
  if (!m || std::memcmp(m, kMagic, 4) != 0) return false;
  kind = read_u8(r);
  return !r.bad;
}

size_t player_size(const StatusPlayer& p) {
  return str_size(p.name, kStatusPlayer) + str_size(p.team, kStatusTeam) + 4 + 2 + 2 + 1 + 4;
}
size_t team_size(const StatusTeam& t) { return str_size(t.name, kStatusTeam) + 4 + 4; }
size_t key_size(const std::pair<std::string, std::string>& k) {
  return str_size(k.first, kStatusKey) + str_size(k.second, kStatusValue);
}
constexpr size_t kStreamSize = kAddressSize + 4 + 2 + 2 + 1;

}  // namespace

const char* node_kind_name(NodeKind k) {
  switch (k) {
    case NodeKind::game:
      return "game";
    case NodeKind::tv:
      return "tv";
    case NodeKind::forward:
      return "forward";
  }
  return "?";
}

bool node_kind_of(std::string_view name, NodeKind& out) {
  for (const NodeKind k : {NodeKind::game, NodeKind::tv, NodeKind::forward}) {
    if (name == node_kind_name(k)) {
      out = k;
      return true;
    }
  }
  return false;
}

const char* status_phase_name(StatusPhase p) {
  switch (p) {
    case StatusPhase::unknown:
      return "unknown";
    case StatusPhase::lobby:
      return "lobby";
    case StatusPhase::countdown:
      return "countdown";
    case StatusPhase::playing:
      return "playing";
    case StatusPhase::intermission:
      return "intermission";
  }
  return "?";
}

size_t status_write(const Status& s, uint8_t* out, size_t capacity) {
  Writer w = make_writer(out, capacity);
  write_u8(w, kRecordVersion);
  write_u8(w, static_cast<uint8_t>(s.kind));
  write_u32(w, s.protocol);
  write_u32(w, s.version);
  const size_t flags_at = w.size;
  write_u8(w, s.locked ? kFlagLocked : 0);
  write_u8(w, static_cast<uint8_t>(s.phase));
  write_u32(w, static_cast<uint32_t>(s.time_left_s));
  write_u8(w, static_cast<uint8_t>(s.max_players < 0     ? 0
                                   : s.max_players > 255 ? 255
                                                         : s.max_players));
  write_u8(w, static_cast<uint8_t>(s.max_spectators < 0     ? 0
                                   : s.max_spectators > 255 ? 255
                                                            : s.max_spectators));
  write_bytes(w, s.country, 2);
  write_str(w, s.name, kStatusName);
  write_str(w, s.map, kStatusName);
  write_str(w, s.mode, kStatusName);
  if (w.overflow) return 0;  // not even the header fits

  bool truncated = false;
  // Each section: its count, then as many rows as fit with room left for the later counts.
  auto section = [&](size_t rows, size_t tail, auto row_size, auto write_row) {
    const size_t count_at = w.size;
    write_u8(w, 0);
    size_t n = 0;
    const size_t most = rows < kStatusMaxRows ? rows : kStatusMaxRows;
    const bool after_a_cut = truncated;  // an earlier section was cut: this one is left out
    for (size_t i = 0; i < most && !after_a_cut; i++) {
      if (w.size + row_size(i) + tail > capacity) {
        truncated = true;
        break;
      }
      write_row(i);
      n++;
    }
    if (rows > n) truncated = true;
    out[count_at] = static_cast<uint8_t>(n);
  };
  section(
      s.players.size(), 3, [&](size_t i) { return player_size(s.players[i]); },
      [&](size_t i) {
        const StatusPlayer& p = s.players[i];
        write_str(w, p.name, kStatusPlayer);
        write_str(w, p.team, kStatusTeam);
        write_u32(w, static_cast<uint32_t>(p.score));
        write_u16(w, p.ping_ms);
        write_u16(w, p.minutes);
        write_u8(w, p.flags);
        write_u32(w, p.colour);
      });
  section(
      s.teams.size(), 2, [&](size_t i) { return team_size(s.teams[i]); },
      [&](size_t i) {
        const StatusTeam& t = s.teams[i];
        write_str(w, t.name, kStatusTeam);
        write_u32(w, static_cast<uint32_t>(t.score));
        write_u32(w, t.colour);
      });
  section(
      s.streams.size(), 1, [&](size_t) { return kStreamSize; },
      [&](size_t i) {
        const StatusStream& st = s.streams[i];
        reach_wire::write_address(w, st.node);
        write_u32(w, st.stream);
        write_u16(w, st.delay_s);
        write_u16(w, st.viewers);
        write_u8(w, st.flags);
      });
  section(
      s.keys.size(), 0, [&](size_t i) { return key_size(s.keys[i]); },
      [&](size_t i) {
        write_str(w, s.keys[i].first, kStatusKey);
        write_str(w, s.keys[i].second, kStatusValue);
      });
  if (w.overflow) return 0;
  if (truncated) out[flags_at] |= kFlagTruncated;
  return w.size;
}

bool status_read(const uint8_t* data, size_t size, Status& s) {
  Reader r = make_reader(data, size);
  if (read_u8(r) != kRecordVersion || r.bad) return false;
  const uint8_t kind = read_u8(r);
  if (kind > static_cast<uint8_t>(NodeKind::forward)) return false;
  s.kind = static_cast<NodeKind>(kind);
  s.protocol = read_u32(r);
  s.version = read_u32(r);
  const uint8_t flags = read_u8(r);
  s.locked = flags & kFlagLocked;
  s.truncated = flags & kFlagTruncated;
  const uint8_t phase = read_u8(r);
  s.phase = phase <= static_cast<uint8_t>(StatusPhase::intermission)
                ? static_cast<StatusPhase>(phase)
                : StatusPhase::unknown;
  s.time_left_s = static_cast<int32_t>(read_u32(r));
  s.max_players = read_u8(r);
  s.max_spectators = read_u8(r);
  read_bytes(r, s.country, 2);
  s.country[2] = 0;
  if (!read_str(r, s.name) || !read_str(r, s.map) || !read_str(r, s.mode)) return false;
  s.players.assign(read_u8(r), {});
  for (StatusPlayer& p : s.players) {
    if (!read_str(r, p.name) || !read_str(r, p.team)) return false;
    p.score = static_cast<int32_t>(read_u32(r));
    p.ping_ms = read_u16(r);
    p.minutes = read_u16(r);
    p.flags = read_u8(r);
    p.colour = read_u32(r);
  }
  s.teams.assign(read_u8(r), {});
  for (StatusTeam& t : s.teams) {
    if (!read_str(r, t.name)) return false;
    t.score = static_cast<int32_t>(read_u32(r));
    t.colour = read_u32(r);
  }
  s.streams.assign(read_u8(r), {});
  for (StatusStream& st : s.streams) {
    st.node = reach_wire::read_address(r);
    st.stream = read_u32(r);
    st.delay_s = read_u16(r);
    st.viewers = read_u16(r);
    st.flags = read_u8(r);
  }
  s.keys.assign(read_u8(r), {});
  for (auto& k : s.keys) {
    if (!read_str(r, k.first) || !read_str(r, k.second)) return false;
  }
  return !r.bad && remaining(r) == 0;
}

size_t status_ask(uint32_t protocol, uint64_t nonce, uint64_t cookie, uint8_t* out) {
  Writer w = make_writer(out, kStatusAskSize);
  write_start(w, kAsk);
  write_u32(w, protocol);
  write_u64(w, nonce);
  write_u64(w, cookie);
  write_pad_to(w, kStatusAskSize);
  return w.overflow ? 0 : w.size;
}

StatusReply status_read_reply(const uint8_t* data, size_t size, uint64_t nonce, uint64_t& cookie,
                              Status& s) {
  Reader r = make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind) || read_u64(r) != nonce || r.bad) return StatusReply::none;
  if (kind == kCookie) {
    cookie = read_u64(r);
    return r.bad ? StatusReply::none : StatusReply::cookie;
  }
  if (kind == kRecord && status_read(data + r.at, size - r.at, s)) return StatusReply::status;
  return StatusReply::none;
}

bool is_status_ask(const uint8_t* data, size_t size) {
  return size >= kStatusAskSize && std::memcmp(data, kMagic, 4) == 0 && data[4] == kAsk;
}

size_t status_answer(const uint8_t* data, size_t size, const Address& from, uint32_t protocol,
                     uint64_t secret, int64_t now_us, const Status& s, uint8_t* out,
                     size_t capacity) {
  if (!is_status_ask(data, size)) return 0;
  Reader r = make_reader(data, kAskHead);
  uint8_t kind = 0;
  read_start(r, kind);
  if (read_u32(r) != protocol) return 0;
  const uint64_t nonce = read_u64(r), cookie = read_u64(r);
  if (r.bad) return 0;
  const int64_t bucket = now_us / kBucketUs;
  const bool good = cookie != 0 && (cookie == cookie_for(secret, from, bucket) ||
                                    cookie == cookie_for(secret, from, bucket - 1));
  Writer w = make_writer(out, capacity);
  if (!good) {
    write_start(w, kCookie);
    write_u64(w, nonce);
    write_u64(w, cookie_for(secret, from, bucket));
    return w.overflow ? 0 : w.size;
  }
  write_start(w, kRecord);
  write_u64(w, nonce);
  if (w.overflow) return 0;
  const size_t n = status_write(s, out + w.size, capacity - w.size);
  return n ? w.size + n : 0;
}

}  // namespace ge::net
