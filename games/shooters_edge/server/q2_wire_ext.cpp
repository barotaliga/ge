// Quake II's later protocols (35 and 36) in the server's wire (q2_wire.hpp § QUAKE II'S LATER
// PROTOCOLS).
//
// ge:summary Quake II's wire for protocols 35 and 36: the frame top, the server data, streams
// ge:keywords protocol-35 protocol-36 frame-top extra-flags long-solid gamestate configstring-stream
// ge:keywords baseline-stream client-settings nogun noblend nogibs nofootsteps noprediction
// ge:skill shooter net
// ge:test tests/shooters_edge_q2_wire_ext_test.cpp
#include <cstring>

#include "shooters_edge/server/q2_wire.hpp"

namespace shooters_edge::server {

namespace {

constexpr uint8_t kFrame = 20, kGameState = 23, kConfigStream = 25, kBaselineStream = 26;

constexpr uint16_t kPmType = 1 << 0, kPmOrigin = 1 << 1, kPmVelocity = 1 << 2, kPmTime = 1 << 3,
                   kPmFlags = 1 << 4, kPmGravity = 1 << 5, kPmDelta = 1 << 6, kViewOffset = 1 << 7,
                   kViewAngles = 1 << 8, kKick = 1 << 9, kBlend = 1 << 10, kFov = 1 << 11,
                   kGunIndex = 1 << 12, kGunFrame = 1 << 13, kRdFlags = 1 << 14;
constexpr uint8_t kXGunOffset = 1 << 0, kXGunAngles = 1 << 1, kXVelocityZ = 1 << 2,
                  kXOriginZ = 1 << 3, kXRoll = 1 << 4, kXStats = 1 << 5, kXClient = 1 << 6;

constexpr uint8_t kNoPrediction = 64;  // the move's flag: the client does not predict
constexpr uint32_t kEffectGib = 0x2, kEffectGreenGib = 0x00200000;
constexpr int kEventFootstep = 2;

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
};

bool alive(const Q2PlayerState& p) { return p.type < 2; }  // normal or a spectator

template <typename T>
void keep3(T (&to)[3], const T (&from)[3]) {
  for (int k = 0; k < 3; k++) to[k] = from[k];
}

}  // namespace

int32_t q2_long_solid(int solid) {
  if (solid == 31 || solid == 0) return solid;
  const int x = (solid & 31) * 8;
  const int down = ((solid >> 5) & 31) * 8;
  const int up = ((solid >> 10) & 63) * 8 - 32;
  const int32_t packed = x | (down << 8) | ((up + 32768) << 16);
  return packed == 31 ? 0 : packed;
}

void q2_server_data_tail(std::vector<uint8_t>& out, const Q2WireOptions& opt) {
  Out o{out};
  if (opt.protocol == 35) {
    o.u8(0);  // not the "enhanced" form
    o.i16(opt.minor);
    o.u8(0);
    o.u8(0);  // no strafe-jump hack: the move runs as protocol 34's
  } else if (opt.protocol == 36) {
    o.i16(opt.minor);
    o.u8(2);  // the server's state: a game
    if (opt.minor >= 1024) {
      o.i16(0);  // no movement hacks, no extended limits
    } else {
      o.u8(0);
      o.u8(0);
      o.u8(0);
    }
  }
}

void q2_entity_for_ext(const Q2Wire& w, int number, const Q2EntityState& base, Q2EntityState& s) {
  if (!s.present) return;
  if (w.asked.no_gibs && (s.effects & (kEffectGib | kEffectGreenGib)) != 0) {
    s.present = false;
    return;
  }
  if (w.asked.no_footsteps && s.event == kEventFootstep) s.event = 0;
  if (w.opt.protocol == 36 && !w.asked.recording && number == w.slot + 1 &&
      alive(w.last_player)) {  // his client places itself: what it holds is kept
    keep3(s.origin, base.origin);
    keep3(s.angles, base.angles);
    keep3(s.old_origin, base.old_origin);
  }
}

void q2_write_frame_top_ext(std::vector<uint8_t>& out, Q2Wire& w, int32_t number,
                            const Q2PlayerState& given) {
  const bool delta = w.last_frame >= 0 && number - w.last_frame > 0 && number - w.last_frame <= 30;
  const Q2PlayerState zero;
  const Q2PlayerState& a = delta ? w.last_player : zero;
  Q2PlayerState b = given;
  if (!w.asked.recording) {  // what the client has kept where it is left out
    if (alive(b) && (b.flags & kNoPrediction) == 0) keep3(b.view_angles, a.view_angles);
    if (!alive(b)) keep3(b.delta_angles, a.delta_angles);
    if (w.asked.no_gun >= 1) {
      b.gun_frame = a.gun_frame;
      keep3(b.gun_offset, a.gun_offset);
      keep3(b.gun_angles, a.gun_angles);
      if (w.asked.no_gun == 1) b.gun_index = a.gun_index;
    }
    if (w.asked.no_blend) std::memcpy(b.blend, a.blend, sizeof b.blend);
    if (w.asked.no_predict && w.opt.protocol == 36) {
      keep3(b.velocity, a.velocity);
      b.time = a.time;
      b.flags = a.flags;
      b.gravity = a.gravity;
    }
  }
  uint16_t f = 0;
  uint8_t x = 0;
  if (b.type != a.type) f |= kPmType;
  if (b.origin[0] != a.origin[0] || b.origin[1] != a.origin[1]) f |= kPmOrigin;
  if (b.origin[2] != a.origin[2]) x |= kXOriginZ;
  if (b.velocity[0] != a.velocity[0] || b.velocity[1] != a.velocity[1]) f |= kPmVelocity;
  if (b.velocity[2] != a.velocity[2]) x |= kXVelocityZ;
  if (b.time != a.time) f |= kPmTime;
  if (b.flags != a.flags) f |= kPmFlags;
  if (b.gravity != a.gravity) f |= kPmGravity;
  if (std::memcmp(b.delta_angles, a.delta_angles, sizeof b.delta_angles) != 0) f |= kPmDelta;
  if (std::memcmp(b.view_offset, a.view_offset, sizeof b.view_offset) != 0) f |= kViewOffset;
  if (b.view_angles[0] != a.view_angles[0] || b.view_angles[1] != a.view_angles[1])
    f |= kViewAngles;
  if (b.view_angles[2] != a.view_angles[2]) x |= kXRoll;
  if (std::memcmp(b.kick, a.kick, sizeof b.kick) != 0) f |= kKick;
  if (b.gun_index != a.gun_index) f |= kGunIndex;
  if (b.gun_frame != a.gun_frame) f |= kGunFrame;
  if (std::memcmp(b.gun_offset, a.gun_offset, sizeof b.gun_offset) != 0) x |= kXGunOffset;
  if (std::memcmp(b.gun_angles, a.gun_angles, sizeof b.gun_angles) != 0) x |= kXGunAngles;
  if (std::memcmp(b.blend, a.blend, sizeof b.blend) != 0) f |= kBlend;
  if (b.fov != a.fov) f |= kFov;
  if (b.rdflags != a.rdflags) f |= kRdFlags;
  uint32_t mask = 0;
  for (int i = 0; i < 32; i++)
    if (b.stats[i] != a.stats[i]) mask |= 1u << i;
  if (mask != 0) x |= kXStats;
  // The viewer's own slot as the client number: sent when no frame it holds says it already.
  const bool client = w.opt.protocol == 36 && !delta && w.slot != 0;
  if (client) x |= kXClient;

  Out o{out};
  o.u8(kFrame | ((x >> 4) & 7u) << 5);
  const uint32_t back = delta ? static_cast<uint32_t>(number - w.last_frame) : 31u;
  o.i32(static_cast<int64_t>((static_cast<uint32_t>(number) & 0x07FFFFFFu) | back << 27));
  const uint32_t low = w.opt.protocol == 35 ? (w.suppress > 15 ? 15u : w.suppress)
                                            : (w.frame_flags | (w.suppress > 0 ? 1u : 0u)) & 15u;
  o.u8(low | (x & 15u) << 4);
  w.suppress = 0;
  w.frame_flags = 0;
  o.u8(32);
  for (int k = 0; k < 32; k++) o.u8(0xFF);
  o.i16(f);
  if (f & kPmType) o.u8(b.type);
  if (f & kPmOrigin) {
    o.i16(b.origin[0]);
    o.i16(b.origin[1]);
  }
  if (x & kXOriginZ) o.i16(b.origin[2]);
  if (f & kPmVelocity) {
    o.i16(b.velocity[0]);
    o.i16(b.velocity[1]);
  }
  if (x & kXVelocityZ) o.i16(b.velocity[2]);
  if (f & kPmTime) o.u8(b.time);
  if (f & kPmFlags) o.u8(b.flags);
  if (f & kPmGravity) o.i16(b.gravity);
  if (f & kPmDelta)
    for (const int16_t v : b.delta_angles) o.i16(v);
  if (f & kViewOffset)
    for (const int8_t v : b.view_offset) o.u8(static_cast<uint8_t>(v));
  if (f & kViewAngles) {
    o.i16(b.view_angles[0]);
    o.i16(b.view_angles[1]);
  }
  if (x & kXRoll) o.i16(b.view_angles[2]);
  if (f & kKick)
    for (const int8_t v : b.kick) o.u8(static_cast<uint8_t>(v));
  if (f & kGunIndex) o.u8(b.gun_index);
  if (f & kGunFrame) o.u8(b.gun_frame);
  if (x & kXGunOffset)
    for (const int8_t v : b.gun_offset) o.u8(static_cast<uint8_t>(v));
  if (x & kXGunAngles)
    for (const int8_t v : b.gun_angles) o.u8(static_cast<uint8_t>(v));
  if (f & kBlend)
    for (const uint8_t v : b.blend) o.u8(v);
  if (f & kFov) o.u8(b.fov);
  if (f & kRdFlags) o.u8(b.rdflags);
  if (x & kXStats) {
    o.i32(static_cast<int64_t>(mask));
    for (int i = 0; i < 32; i++)
      if (mask & (1u << i)) o.i16(b.stats[i]);
  }
  if (client) {
    if (w.opt.minor >= 1022)
      o.i16(w.slot);
    else
      o.u8(static_cast<uint32_t>(w.slot));
  }
  w.last_player = b;
}

void q2_signon_streams(const Q2Signon& parts, int minor,
                       std::vector<std::vector<uint8_t>>& out) {
  out.clear();
  const bool streams = minor >= 1024;
  const auto start = [&out](uint8_t id) {
    out.emplace_back();
    out.back().push_back(id);
  };
  const auto word = [&out](int v) {
    out.back().push_back(static_cast<uint8_t>(v & 0xFF));
    out.back().push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  };
  // The configstrings: each its index and text (the message's id byte, index and text dropped
  // and taken again), the end a word of the slot count.
  start(streams ? kConfigStream : kGameState);
  for (const auto& [index, one] : parts.config_strings) {
    if (streams && out.back().size() + one.size() + 2 > kQ2BigMessage) {
      word(kQ2ConfigStrings);
      start(kConfigStream);
    }
    out.back().insert(out.back().end(), one.begin() + 1, one.end());
  }
  word(kQ2ConfigStrings);
  if (streams) start(kBaselineStream);
  for (const auto& [n, one] : parts.baselines) {
    if (out.back().size() + one.size() + 2 > kQ2BigMessage) {
      out.back().push_back(0);
      out.back().push_back(0);
      start(streams ? kBaselineStream : kGameState);
    }
    out.back().insert(out.back().end(), one.begin() + 1, one.end());
  }
  out.back().push_back(0);
  out.back().push_back(0);
}

}  // namespace shooters_edge::server
