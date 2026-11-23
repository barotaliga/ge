// Quake II's extended limits in the server's wire (q2_wire.hpp § THE EXTENDED LIMITS): an
// entity's and the player's delta at each extension level.
//
// ge:summary Quake II's entity and player deltas under the extended limits, both levels
// ge:keywords q2 extended-limits protocol-extensions extensions2 entity-delta player-state
// ge:keywords model16 morefx alpha scale loop-volume loop-attenuation coord23 stat-mask
// ge:keywords damage-blend q2_write_entity_ext q2_write_player_ext q2_entity_flags_ext
// ge:skill shooter net
// ge:test tests/shooters_edge_q2_wire_limits_test.cpp
#include <algorithm>
#include <cstring>

#include "classic/protocol/q2_limits.hpp"
#include "net/wire.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace shooters_edge::server {

namespace {

namespace cp = ge::classic::protocol;

constexpr uint64_t kOriginX = 1u << 0, kOriginY = 1u << 1, kAngleYaw = 1u << 2,
                   kAngleRoll = 1u << 3, kFrameByte = 1u << 4, kEvent = 1u << 5, kRemove = 1u << 6,
                   kMore1 = 1u << 7, kNumberShort = 1u << 8, kOriginZ = 1u << 9,
                   kAnglePitch = 1u << 10, kModel = 1u << 11, kRenderLo = 1u << 12,
                   kEffectsLo = 1u << 14, kMore2 = 1u << 15, kSkinLo = 1u << 16,
                   kFrameShort = 1u << 17, kRenderHi = 1u << 18, kEffectsHi = 1u << 19,
                   kModel2 = 1u << 20, kModel3 = 1u << 21, kModel4 = 1u << 22, kMore3 = 1u << 23,
                   kOldOrigin = 1u << 24, kSkinHi = 1u << 25, kSoundBit = 1u << 26,
                   kSolid = 1u << 27;
constexpr uint64_t kModels = kModel | kModel2 | kModel3 | kModel4;
constexpr uint32_t kBeam = 128;

constexpr uint8_t kPlayerInfo = 17;
constexpr uint16_t kPmType = 1 << 0, kPmOrigin = 1 << 1, kPmVelocity = 1 << 2, kPmTime = 1 << 3,
                   kPmFlags = 1 << 4, kPmGravity = 1 << 5, kPmDelta = 1 << 6, kViewOffset = 1 << 7,
                   kViewAngles = 1 << 8, kKick = 1 << 9, kBlend = 1 << 10, kFov = 1 << 11,
                   kGunIndex = 1 << 12, kGunFrame = 1 << 13, kRdFlags = 1 << 14;

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
  void coord23(int32_t v, int32_t from) {
    uint8_t buf[3];
    ge::net::Writer w = ge::net::make_writer(buf, sizeof buf);
    cp::write_q2_coord23(w, v, from);
    b.insert(b.end(), buf, buf + w.size);
  }
};

// The corrected width rule: a long when any of the top 16 bits is set, a word for bits 8 to 15,
// else a byte; `lo` and `hi` the two flags (both: a long).
uint64_t width(uint32_t v, uint64_t lo, uint64_t hi) {
  if (v & 0xFFFF0000u) return lo | hi;
  if (v & 0x0000FF00u) return hi;
  return lo;
}

void sized(Out& o, uint32_t v, bool lo, bool hi) {
  if (lo && hi)
    o.i32(static_cast<int32_t>(v));
  else if (hi)
    o.i16(static_cast<int32_t>(v));
  else
    o.u8(v);
}

bool same3(const auto& x, const auto& y) { return x[0] == y[0] && x[1] == y[1] && x[2] == y[2]; }

}  // namespace

int32_t q2_ext_solid(int solid) {
  if (solid == 31 || solid == 0) return solid;
  const int x = std::clamp((solid & 31) * 8, 1, 255);
  const int down = std::clamp(((solid >> 5) & 31) * 8, 0, 255);
  const int up = std::clamp(((solid >> 10) & 63) * 8, 0, 255);  // the height above, + 32
  return static_cast<int32_t>(static_cast<uint32_t>(x) | static_cast<uint32_t>(x) << 8 |
                              static_cast<uint32_t>(down) << 16 | static_cast<uint32_t>(up) << 24);
}

uint64_t q2_entity_flags_ext(const Q2EntityState& from, const Q2EntityState& to, int number,
                             bool old, int level) {
  if (level <= 0) return q2_entity_flags(from, to, number, old);
  uint64_t f = 0;
  if (to.origin[0] != from.origin[0]) f |= kOriginX;
  if (to.origin[1] != from.origin[1]) f |= kOriginY;
  if (to.origin[2] != from.origin[2]) f |= kOriginZ;
  if (to.angles[0] != from.angles[0]) f |= kAnglePitch;
  if (to.angles[1] != from.angles[1]) f |= kAngleYaw;
  if (to.angles[2] != from.angles[2]) f |= kAngleRoll;
  if (to.model[0] != from.model[0]) f |= kModel;
  if (to.model[1] != from.model[1]) f |= kModel2;
  if (to.model[2] != from.model[2]) f |= kModel3;
  if (to.model[3] != from.model[3]) f |= kModel4;
  if ((f & kModels) && std::any_of(to.model, to.model + 4, [](int m) { return m > 255; }))
    f |= cp::kQ2UModel16;
  if (to.frame != from.frame) f |= to.frame < 256 ? kFrameByte : kFrameShort;
  if (to.skin != from.skin) f |= width(to.skin, kSkinLo, kSkinHi);
  if (to.effects != from.effects) f |= width(to.effects, kEffectsLo, kEffectsHi);
  if (to.renderfx != from.renderfx) f |= width(to.renderfx, kRenderLo, kRenderHi);
  if (to.sound != from.sound || to.loop_volume != from.loop_volume ||
      to.loop_attenuation != from.loop_attenuation)
    f |= kSoundBit;
  if (to.event) f |= kEvent;
  if (to.solid != from.solid) f |= kSolid;
  if (old || (to.renderfx & kBeam)) f |= kOldOrigin;
  if (number & 0xff00) f |= kNumberShort;
  if (to.more_effects != from.more_effects)
    f |= width(to.more_effects, cp::kQ2UMoreFx8, cp::kQ2UMoreFx16);
  if (to.alpha != from.alpha) f |= cp::kQ2UAlpha;
  if (to.scale != from.scale) f |= cp::kQ2UScale;
  return f;
}

void q2_write_entity_ext(std::vector<uint8_t>& out, uint64_t f, int number,
                         const Q2EntityState& from, const Q2EntityState& to, int level) {
  if (level <= 0) {
    q2_write_entity(out, static_cast<uint32_t>(f), number, to);
    return;
  }
  Out o{out};
  if (f & 0xFF00000000ull) f |= cp::kQ2UMoreBits4;
  if (f & 0xFF000000u) f |= kMore3;
  if (f & 0x00FF0000u) f |= kMore2;
  if (f & 0x0000FF00u) f |= kMore1;
  o.u8(f & 0xFFu);
  if (f & kMore1) o.u8((f >> 8) & 0xFFu);
  if (f & kMore2) o.u8((f >> 16) & 0xFFu);
  if (f & kMore3) o.u8((f >> 24) & 0xFFu);
  if (f & cp::kQ2UMoreBits4) o.u8((f >> 32) & 0xFFu);
  if (f & kNumberShort)
    o.i16(number);
  else
    o.u8(static_cast<uint32_t>(number));
  if (f & kRemove) return;
  const uint64_t model_bits[4] = {kModel, kModel2, kModel3, kModel4};
  for (int k = 0; k < 4; k++) {
    if (!(f & model_bits[k])) continue;
    if (f & cp::kQ2UModel16)
      o.i16(to.model[k]);
    else
      o.u8(static_cast<uint32_t>(to.model[k]));
  }
  if (f & kFrameByte) o.u8(static_cast<uint32_t>(to.frame));
  if (f & kFrameShort) o.i16(to.frame);
  if (f & (kSkinLo | kSkinHi)) sized(o, to.skin, (f & kSkinLo) != 0, (f & kSkinHi) != 0);
  if (f & (kEffectsLo | kEffectsHi))
    sized(o, to.effects, (f & kEffectsLo) != 0, (f & kEffectsHi) != 0);
  if (f & (kRenderLo | kRenderHi))
    sized(o, to.renderfx, (f & kRenderLo) != 0, (f & kRenderHi) != 0);
  const uint64_t origin_bits[3] = {kOriginX, kOriginY, kOriginZ};
  for (int k = 0; k < 3; k++) {
    if (!(f & origin_bits[k])) continue;
    if (level >= 2)
      o.coord23(to.origin[k], from.origin[k]);
    else
      o.i16(to.origin[k]);
  }
  if (f & kAnglePitch) o.u8(to.angles[0]);
  if (f & kAngleYaw) o.u8(to.angles[1]);
  if (f & kAngleRoll) o.u8(to.angles[2]);
  if (f & kOldOrigin)
    for (const int32_t v : to.old_origin) {
      if (level >= 2)
        o.coord23(v, 0);
      else
        o.i16(v);
    }
  if (f & kSoundBit) {
    int word = to.sound & cp::kQ2LoopIndexMask;
    if (to.loop_volume != from.loop_volume) word |= cp::kQ2LoopVolume;
    if (to.loop_attenuation != from.loop_attenuation) word |= cp::kQ2LoopAttenuation;
    o.i16(word);
    if (word & cp::kQ2LoopVolume) o.u8(to.loop_volume);
    if (word & cp::kQ2LoopAttenuation) o.u8(to.loop_attenuation);
  }
  if (f & kEvent) o.u8(static_cast<uint32_t>(to.event));
  if (f & kSolid) o.i32(q2_ext_solid(to.solid));
  if (f & (cp::kQ2UMoreFx8 | cp::kQ2UMoreFx16))
    sized(o, to.more_effects, (f & cp::kQ2UMoreFx8) != 0, (f & cp::kQ2UMoreFx16) != 0);
  if (f & cp::kQ2UAlpha) o.u8(to.alpha);
  if (f & cp::kQ2UScale) o.u8(to.scale);
}

void q2_write_player_ext(std::vector<uint8_t>& out, const Q2PlayerState& a, const Q2PlayerState& b,
                         int level) {
  if (level <= 0) {
    q2_write_player(out, a, b);
    return;
  }
  Out o{out};
  const bool v2 = level >= 2;
  uint16_t f = kGunIndex;  // always, as the protocol 34 form sends it
  if (b.type != a.type) f |= kPmType;
  if (!same3(b.origin, a.origin)) f |= kPmOrigin;
  if (!same3(b.velocity, a.velocity)) f |= kPmVelocity;
  if (b.time != a.time) f |= kPmTime;
  if (b.flags != a.flags) f |= kPmFlags;
  if (b.gravity != a.gravity) f |= kPmGravity;
  if (!same3(b.delta_angles, a.delta_angles)) f |= kPmDelta;
  if (!same3(b.view_offset, a.view_offset)) f |= kViewOffset;
  if (!same3(b.view_angles, a.view_angles)) f |= kViewAngles;
  if (!same3(b.kick, a.kick)) f |= kKick;
  if (b.gun_frame != a.gun_frame || !same3(b.gun_offset, a.gun_offset) ||
      !same3(b.gun_angles, a.gun_angles))
    f |= kGunFrame;  // the frame, offset and angles travel together
  const bool blend = std::memcmp(b.blend, a.blend, sizeof b.blend) != 0;
  const bool damage = v2 && std::memcmp(b.damage_blend, a.damage_blend, sizeof b.blend) != 0;
  if (blend || damage) f |= kBlend;
  if (b.fov != a.fov) f |= kFov;
  if (b.rdflags != a.rdflags) f |= kRdFlags;
  o.u8(kPlayerInfo);
  o.i16(f);
  if (f & kPmType) o.u8(b.type);
  if (f & kPmOrigin)
    for (int k = 0; k < 3; k++) v2 ? o.coord23(b.origin[k], a.origin[k]) : o.i16(b.origin[k]);
  if (f & kPmVelocity)
    for (int k = 0; k < 3; k++) v2 ? o.coord23(b.velocity[k], a.velocity[k]) : o.i16(b.velocity[k]);
  if (f & kPmTime) v2 ? o.i16(b.time) : o.u8(b.time);
  if (f & kPmFlags) v2 ? o.i16(b.flags) : o.u8(b.flags);
  if (f & kPmGravity) o.i16(b.gravity);
  if (f & kPmDelta)
    for (const int16_t v : b.delta_angles) o.i16(v);
  if (f & kViewOffset)
    for (const int8_t v : b.view_offset) o.u8(static_cast<uint8_t>(v));
  if (f & kViewAngles)
    for (const int16_t v : b.view_angles) o.i16(v);
  if (f & kKick)
    for (const int8_t v : b.kick) o.u8(static_cast<uint8_t>(v));
  o.i16(b.gun_index);  // a word under the extended limits
  if (f & kGunFrame) {
    o.u8(b.gun_frame);
    for (const int8_t v : b.gun_offset) o.u8(static_cast<uint8_t>(v));
    for (const int8_t v : b.gun_angles) o.u8(static_cast<uint8_t>(v));
  }
  if (f & kBlend) {
    if (v2) {  // a byte of which parts follow: the blend's, then the damage blend's
      uint32_t which = 0;
      for (int k = 0; k < 4; k++) {
        if (b.blend[k] != a.blend[k]) which |= 1u << k;
        if (b.damage_blend[k] != a.damage_blend[k]) which |= 16u << k;
      }
      o.u8(which);
      for (int k = 0; k < 4; k++)
        if (which & (1u << k)) o.u8(b.blend[k]);
      for (int k = 0; k < 4; k++)
        if (which & (16u << k)) o.u8(b.damage_blend[k]);
    } else {
      for (const uint8_t v : b.blend) o.u8(v);
    }
  }
  if (f & kFov) o.u8(b.fov);
  if (f & kRdFlags) o.u8(b.rdflags);
  const int stats = v2 ? cp::kQ2StatsExtensions2 : 32;
  uint64_t mask = 0;
  for (int i = 0; i < stats; i++)
    if (b.stats[i] != a.stats[i]) mask |= 1ull << i;
  if (v2) {
    uint8_t buf[10];
    ge::net::Writer w = ge::net::make_writer(buf, sizeof buf);
    ge::net::write_varint(w, mask);
    out.insert(out.end(), buf, buf + w.size);
  } else {
    o.i32(static_cast<int32_t>(mask));
  }
  for (int i = 0; i < stats; i++)
    if (mask & (1ull << i)) o.i16(b.stats[i]);
}

}  // namespace shooters_edge::server
