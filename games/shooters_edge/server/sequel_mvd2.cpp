// The sequel's multiview recording written (sequel_mvd2.hpp): the game state, the frames as
// deltas, the messages around them.
//
// ge:summary The .mvd2 writer: the game state, frames as deltas, the messages around them
// ge:keywords mvd2 multiview sequel writer player-delta entity-delta mvd2_write
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_mvd2_test.cpp
#include "shooters_edge/server/sequel_mvd2.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace shooters_edge::server {

namespace {

constexpr int kSlots = kSequelConfigStrings;  // the settings strings' end marker

// Protocol 34's entity delta bits this writer sets or clears.
constexpr uint32_t kOriginX = 1u << 0, kOriginY = 1u << 1, kAngleYaw = 1u << 2,
                   kAngleRoll = 1u << 3, kRemove = 1u << 6, kNumberShort = 1u << 8,
                   kOriginZ = 1u << 9, kAnglePitch = 1u << 10, kOldOrigin = 1u << 24;
constexpr uint32_t kPlaceBits =
    kOriginX | kOriginY | kOriginZ | kAnglePitch | kAngleYaw | kAngleRoll;
constexpr uint32_t kBeam = 128;  // the render effect whose old origin is the beam's far end
constexpr int kPmNormal = 0;     // the movement type of a walking player

struct Out {
  std::vector<uint8_t>& b;
  void u8(uint32_t v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int32_t v) {
    const auto u = static_cast<uint16_t>(v);
    u8(u & 0xFFu);
    u8(u >> 8);
  }
  void i32(int64_t v) {
    const auto u = static_cast<uint32_t>(v);
    for (int k = 0; k < 4; k++) u8((u >> (8 * k)) & 0xFFu);
  }
  void str(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
  void bytes(std::span<const uint8_t> s) { b.insert(b.end(), s.begin(), s.end()); }
};

template <typename A>
bool same3(const A& x, const A& y) {
  return x[0] == y[0] && x[1] == y[1] && x[2] == y[2];
}

// --- the player entry ---

uint32_t player_flags(const SequelPlayerState& a, const SequelPlayerState& b) {
  uint32_t f = 0;
  if (b.type != a.type) f |= kMv2Type;
  if (b.origin[0] != a.origin[0] || b.origin[1] != a.origin[1]) f |= kMv2OriginXY;
  if (b.origin[2] != a.origin[2]) f |= kMv2OriginZ;
  if (!same3(b.view_offset, a.view_offset)) f |= kMv2ViewOffset;
  if (b.view_angles[0] != a.view_angles[0] || b.view_angles[1] != a.view_angles[1])
    f |= kMv2ViewAngles;
  if (b.view_angles[2] != a.view_angles[2]) f |= kMv2ViewRoll;
  if (!same3(b.kick, a.kick)) f |= kMv2Kick;
  if (std::memcmp(b.blend, a.blend, sizeof b.blend) != 0) f |= kMv2Blend;
  if (b.fov != a.fov) f |= kMv2Fov;
  if (b.gun_index != a.gun_index) f |= kMv2GunIndex;
  if (b.gun_frame != a.gun_frame) f |= kMv2GunFrame;
  if (!same3(b.gun_offset, a.gun_offset)) f |= kMv2GunOffset;
  if (!same3(b.gun_angles, a.gun_angles)) f |= kMv2GunAngles;
  if (b.rdflags != a.rdflags) f |= kMv2RdFlags;
  if (std::memcmp(b.stats, a.stats, sizeof b.stats) != 0) f |= kMv2Stats;
  return f;
}

void write_player(Out& o, int client, uint32_t f, const SequelPlayerState& a,
                  const SequelPlayerState& b) {
  o.u8(static_cast<uint32_t>(client));
  o.i16(static_cast<int32_t>(f));
  if (f & kMv2Type) o.u8(b.type);
  if (f & kMv2OriginXY) {
    o.i16(b.origin[0]);
    o.i16(b.origin[1]);
  }
  if (f & kMv2OriginZ) o.i16(b.origin[2]);
  if (f & kMv2ViewOffset)
    for (const int8_t v : b.view_offset) o.u8(static_cast<uint8_t>(v));
  if (f & kMv2ViewAngles) {
    o.i16(b.view_angles[0]);
    o.i16(b.view_angles[1]);
  }
  if (f & kMv2ViewRoll) o.i16(b.view_angles[2]);
  if (f & kMv2Kick)
    for (const int8_t v : b.kick) o.u8(static_cast<uint8_t>(v));
  if (f & kMv2GunIndex) o.u8(b.gun_index);
  if (f & kMv2GunFrame) o.u8(b.gun_frame);
  if (f & kMv2GunOffset)
    for (const int8_t v : b.gun_offset) o.u8(static_cast<uint8_t>(v));
  if (f & kMv2GunAngles)
    for (const int8_t v : b.gun_angles) o.u8(static_cast<uint8_t>(v));
  if (f & kMv2Blend)
    for (const uint8_t v : b.blend) o.u8(v);
  if (f & kMv2Fov) o.u8(b.fov);
  if (f & kMv2RdFlags) o.u8(b.rdflags);
  if (f & kMv2Stats) {
    uint32_t mask = 0;
    for (int i = 0; i < 32; i++)
      if (b.stats[i] != a.stats[i] || (f & kMv2All) == kMv2All) mask |= 1u << i;
    o.i32(static_cast<int32_t>(mask));
    for (int i = 0; i < 32; i++)
      if (mask & (1u << i)) o.i16(b.stats[i]);
  }
}

// --- the frame body ---

void write_body(Out& o, const Mvd2Frame& was, const Mvd2Frame& now, bool whole) {
  o.u8(0);  // no area portals kept
  for (size_t c = 0; c < now.players.size(); c++) {
    const Mvd2Player& p = now.players[c];
    const bool had = c < was.players.size() && was.players[c].in && !whole;
    if (!p.in) {
      if (had) {
        o.u8(static_cast<uint32_t>(c));
        o.i16(static_cast<int32_t>(kMv2Removed));
      }
      continue;
    }
    const SequelPlayerState zero;
    const uint32_t f = had ? player_flags(was.players[c].s, p.s) : kMv2All;
    if (f == 0) continue;
    write_player(o, static_cast<int>(c), f, had ? was.players[c].s : zero, p.s);
  }
  o.u8(255);
  const SequelEntityState zero;
  for (int n = 1; n < kSequelEntities; n++) {
    const SequelEntityState& s = now.entities[size_t(n)];
    const SequelEntityState& b =
        whole || size_t(n) >= was.entities.size() ? zero : was.entities[size_t(n)];
    if (!s.present) {
      if (b.present) {
        std::vector<uint8_t> one;
        sequel_write_entity(one, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
        o.bytes(one);
      }
      continue;
    }
    const bool fresh = !b.present;
    uint32_t f = sequel_entity_flags(fresh ? zero : b, s, n, false);
    if (mvd2_placed(now.players, n)) f &= ~kPlaceBits;
    // A beam's far end goes only when it moved (the reader keeps the one it holds).
    if ((s.renderfx & kBeam) != 0) {
      if (fresh || !same3(s.old_origin, b.old_origin))
        f |= kOldOrigin;
      else
        f &= ~kOldOrigin;
    }
    if ((f & ~kNumberShort) == 0 && !fresh) continue;
    std::vector<uint8_t> one;
    sequel_write_entity(one, f, n, s);
    o.bytes(one);
  }
  o.u8(0);
  o.u8(0);
}

// The frame `body` onto `f` (players and entities as deltas from what it holds).

void put_cast(Out& o, uint8_t id, int target, std::span<const uint8_t> bytes, bool leaf) {
  const size_t n = std::min(bytes.size(), kMvd2MostCast);
  o.u8(id | static_cast<uint32_t>((n >> 8) << 5));
  o.u8(static_cast<uint32_t>(n & 0xFF));
  if (id == kMv2Unicast || id == kMv2UnicastReliable) o.u8(static_cast<uint32_t>(target));
  if (leaf) o.i16(target);
  o.bytes(bytes.subspan(0, n));
}

void put_block(std::vector<uint8_t>& out, const std::vector<uint8_t>& block) {
  out.push_back(static_cast<uint8_t>(block.size() & 0xFF));
  out.push_back(static_cast<uint8_t>(block.size() >> 8));
  out.insert(out.end(), block.begin(), block.end());
}

void write_frame_messages(Out& o, const Mvd2Frame& was, const Mvd2Frame& f) {
  for (const auto& [i, s] : f.config_strings) {
    o.u8(kMv2ConfigString);
    o.i16(i);
    o.str(s);
  }
  for (const auto& [level, text] : f.prints) {
    o.u8(kMv2Print);
    o.u8(static_cast<uint32_t>(level));
    o.str(text);
  }
  for (const auto& [c, bytes] : f.unicasts) put_cast(o, kMv2UnicastReliable, c, bytes, false);
  o.u8(kMv2Frame);
  write_body(o, was, f, false);
  for (const std::vector<uint8_t>& bytes : f.multicasts)
    put_cast(o, kMv2MulticastAllReliable, 0, bytes, false);
  for (const Mvd2Sound& s : f.sounds) {
    uint32_t flags = 0;
    if (s.volume != 255) flags |= 1;
    if (s.attenuation != 64) flags |= 2;
    if (s.offset != 0) flags |= 16;
    o.u8(kMv2Sound);
    o.u8(flags);
    o.u8(static_cast<uint32_t>(s.index));
    if (flags & 1) o.u8(static_cast<uint32_t>(s.volume));
    if (flags & 2) o.u8(static_cast<uint32_t>(s.attenuation));
    if (flags & 16) o.u8(static_cast<uint32_t>(s.offset));
    o.i16(s.entity * 8 + (s.channel & 7));
  }
}

}  // namespace

bool is_mvd2(std::span<const uint8_t> bytes) {
  return bytes.size() >= 4 && std::memcmp(bytes.data(), kMvd2Magic, 4) == 0;
}

void mvd2_place_from_player(SequelEntityState& e, const SequelPlayerState& p) {
  std::memcpy(e.origin, p.origin, sizeof e.origin);
  float pitch = static_cast<float>(static_cast<uint16_t>(p.view_angles[0])) * (360.0f / 65536.0f);
  if (pitch > 180.0f) pitch -= 360.0f;
  pitch /= 3.0f;
  const float yaw =
      static_cast<float>(static_cast<uint16_t>(p.view_angles[1])) * (360.0f / 65536.0f);
  e.angles[0] = static_cast<uint8_t>(static_cast<int32_t>(pitch * 256.0f / 360.0f) & 0xFF);
  e.angles[1] = static_cast<uint8_t>(static_cast<int32_t>(yaw * 256.0f / 360.0f) & 0xFF);
  e.angles[2] = 0;
}

bool mvd2_placed(const std::vector<Mvd2Player>& players, int number) {
  const int c = number - 1;
  return c >= 0 && size_t(c) < players.size() && players[size_t(c)].in &&
         players[size_t(c)].s.type == kPmNormal;
}

void mvd2_write_magic(std::vector<uint8_t>& out) {
  out.insert(out.end(), std::begin(kMvd2Magic), std::end(kMvd2Magic));
}

void mvd2_write_level(std::vector<uint8_t>& out, const Mvd2Level& level) {
  std::vector<uint8_t> block;
  Out o{block};
  o.u8(kMv2ServerData);
  o.i32(kMvd2Major);
  o.i16(kMvd2Minor);
  o.i32(level.server_count);
  o.str(level.game);
  o.i16(level.observer);
  for (int i = 0; i < kSlots && size_t(i) < level.config_strings.size(); i++) {
    const std::string& s = level.config_strings[size_t(i)];
    if (s.empty()) continue;
    o.i16(i);
    o.str(s);
  }
  o.i16(kSlots);
  write_body(o, Mvd2Frame{}, level.base, true);
  put_block(out, block);
}

void mvd2_write_frame(std::vector<uint8_t>& out, const Mvd2Frame& was, const Mvd2Frame& now) {
  std::vector<uint8_t> block;
  Out o{block};
  write_frame_messages(o, was, now);
  put_block(out, block);
}

void mvd2_write_end(std::vector<uint8_t>& out) {
  out.push_back(0);
  out.push_back(0);
}

std::vector<uint8_t> mvd2_write(const Mvd2Level& level, const std::vector<Mvd2Frame>& frames) {
  std::vector<uint8_t> out;
  mvd2_write_magic(out);
  mvd2_write_level(out, level);
  const Mvd2Frame* was = &level.base;
  for (const Mvd2Frame& f : frames) {
    mvd2_write_frame(out, *was, f);
    was = &f;
  }
  mvd2_write_end(out);
  return out;
}

}  // namespace shooters_edge::server
