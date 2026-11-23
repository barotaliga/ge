// Quake II's later protocols read as protocol 34 (q2_proto_in.hpp).
//
// ge:summary Quake II's protocols 35 and 36 rewritten as protocol 34 for the one reader
// ge:keywords protocol-35 protocol-36 zpacket zdownload gamestate downgrade
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_proto_in_test.cpp
#include "shooters_edge/net/q2_proto_in.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

#include "classic/protocol/q2_limits.hpp"
#include "core/inflate.hpp"

namespace shooters_edge::net {

namespace {

namespace sv = server;
using sv::Q2EntityState;
using sv::Q2PlayerState;

enum : uint8_t {
  kServerData = 12,
  kConfigString = 13,
  kBaseline = 14,
  kDownload = 16,
  kPlayerInfo = 17,
  kPacketEntities = 18,
  kFrame = 20,
  kZPacket = 21,
  kZDownload = 22,
  kGameState = 23,
  kSetting = 24,
  kConfigStream = 25,
  kBaselineStream = 26,
};
constexpr int kGameOwnFirst = 117, kGameOwnLast = 127;  // the game's own messages (demo_q2.cpp)
constexpr size_t kMostBlock = 1400;
constexpr size_t kMostInflated = 32768;
constexpr size_t kMostDownload = size_t(256) << 20;  // a compressed download's largest file

// An entity's header bits (protocol 34's, and 36's bit 13).
constexpr uint32_t kOriginX = 1u << 0, kOriginY = 1u << 1, kAngleYaw = 1u << 2,
                   kAngleRoll = 1u << 3, kFrameByte = 1u << 4, kEvent = 1u << 5, kRemove = 1u << 6,
                   kMore1 = 1u << 7, kNumberShort = 1u << 8, kOriginZ = 1u << 9,
                   kAnglePitch = 1u << 10, kModel = 1u << 11, kRenderLo = 1u << 12,
                   kAngle16 = 1u << 13, kEffectsLo = 1u << 14, kMore2 = 1u << 15,
                   kSkinLo = 1u << 16, kFrameShort = 1u << 17, kRenderHi = 1u << 18,
                   kEffectsHi = 1u << 19, kModel2 = 1u << 20, kModel3 = 1u << 21,
                   kModel4 = 1u << 22, kMore3 = 1u << 23, kOldOrigin = 1u << 24, kSkinHi = 1u << 25,
                   kSoundBit = 1u << 26, kSolid = 1u << 27;

// The player state's bits (34's), and the later protocols' extra bits.
constexpr uint32_t kPmType = 1u << 0, kPmOrigin = 1u << 1, kPmVelocity = 1u << 2, kPmTime = 1u << 3,
                   kPmFlags = 1u << 4, kPmGravity = 1u << 5, kPmDelta = 1u << 6,
                   kViewOffset = 1u << 7, kViewAngles = 1u << 8, kKick = 1u << 9, kBlend = 1u << 10,
                   kFov = 1u << 11, kGunIndex = 1u << 12, kGunFrame = 1u << 13, kRdFlags = 1u << 14;
constexpr uint32_t kMoreBitsPlayer = 1u << 15, kFogPlayer = 1u << 16;  // the fog level's
constexpr uint32_t kXGunOffset = 1u << 0, kXGunAngles = 1u << 1, kXVelocityZ = 1u << 2,
                   kXOriginZ = 1u << 3, kXRoll = 1u << 4, kXStats = 1u << 5, kXClient = 1u << 6;

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
};

// Whether the solid is the long form: 35 from minor 1905, every 36.
bool long_solid(const Q2ProtoIn& p) {
  return p.protocol == 36 || (p.protocol == 35 && p.minor >= 1905);
}

uint32_t entity_header(client::DemoMsg& m, int& number) {
  auto f = static_cast<uint32_t>(m.u8());
  if (f & kMore1) f |= static_cast<uint32_t>(m.u8()) << 8;
  if (f & kMore2) f |= static_cast<uint32_t>(m.u8()) << 16;
  if (f & kMore3) f |= static_cast<uint32_t>(m.u8()) << 24;
  number = (f & kNumberShort) ? m.u16() : m.u8();
  return f;
}

// A sized field read whole: a byte, a word (never negative: a later server sends a long for a
// value with bit 15 only to the older protocols), or a long.
uint32_t sized(client::DemoMsg& m, uint32_t f, uint32_t lo, uint32_t hi) {
  if ((f & lo) && (f & hi)) return static_cast<uint32_t>(m.i32());
  if (f & hi) return static_cast<uint32_t>(m.u16());
  return static_cast<uint32_t>(m.u8());
}

// A 16-bit angle as the byte the older protocol carries (rounded to the nearest).
uint8_t angle_byte(int a16) { return static_cast<uint8_t>(((a16 & 0xFFFF) + 128) >> 8); }

namespace lim = ge::classic::protocol;

// The configstrings' end marker (and count) of the layout the stream uses.
int slots(const Q2ProtoIn& p) { return lim::q2_cs_layout(p.ext > 0).end; }

// A 23-bit coordinate (extensions 2) against `from`: a word whose low bit is 0 holds the change,
// one whose low bit is 1 takes a third byte and holds the value itself.
int32_t coord23(client::DemoMsg& m, int32_t from) {
  auto w = static_cast<uint32_t>(m.u16());
  if ((w & 1u) == 0) return from + (static_cast<int16_t>(static_cast<uint16_t>(w)) >> 1);
  w |= static_cast<uint32_t>(m.u8()) << 16;
  return static_cast<int32_t>(w << 8) >> 9;
}

// A variable-length integer, seven bits a byte, least significant first (64 bits at most).
uint64_t varint(client::DemoMsg& m) {
  uint64_t v = 0;
  for (int shift = 0; shift < 64 && !m.bad; shift += 7) {
    const auto b = static_cast<uint64_t>(m.u8());
    v |= (b & 0x7Fu) << shift;
    if ((b & 0x80u) == 0) break;
  }
  return v;
}

// An entity entry under the extended limits, its header first: the same bytes in protocol 36 and
// in the recording at the same level, so it is copied as it is once its end is found. False at
// the list's end (number 0).
bool copy_entity_ext(const Q2ProtoIn& p, client::DemoMsg& m, std::vector<uint8_t>& out) {
  const size_t from = m.at;
  uint64_t f = static_cast<uint64_t>(m.u8());
  if (f & kMore1) f |= static_cast<uint64_t>(m.u8()) << 8;
  if (f & kMore2) f |= static_cast<uint64_t>(m.u8()) << 16;
  if (f & kMore3) f |= static_cast<uint64_t>(m.u8()) << 24;
  if (f & lim::kQ2UMoreBits4) f |= static_cast<uint64_t>(m.u8()) << 32;
  const int number = (f & kNumberShort) ? m.u16() : m.u8();
  if (number != 0 && (f & kRemove) == 0) {
    const bool v2 = p.ext >= 2;
    const auto sized_skip = [&m, f](uint64_t lo, uint64_t hi) {
      if ((f & lo) && (f & hi))
        m.skip(4);
      else if (f & hi)
        m.skip(2);
      else if (f & lo)
        m.skip(1);
    };
    const size_t model = (f & lim::kQ2UModel16) != 0 ? 2 : 1;
    for (const uint32_t bit : {kModel, kModel2, kModel3, kModel4})
      if (f & bit) m.skip(model);
    if (f & kFrameByte) m.skip(1);
    if (f & kFrameShort) m.skip(2);
    sized_skip(kSkinLo, kSkinHi);
    sized_skip(kEffectsLo, kEffectsHi);
    sized_skip(kRenderLo, kRenderHi);
    const auto point = [&m, v2] { v2 ? static_cast<void>(coord23(m, 0)) : m.skip(2); };
    for (const uint32_t bit : {kOriginX, kOriginY, kOriginZ})
      if (f & bit) point();
    const size_t angle = (f & kAngle16) != 0 ? 2 : 1;
    for (const uint32_t bit : {kAnglePitch, kAngleYaw, kAngleRoll})
      if (f & bit) m.skip(angle);
    if (f & kOldOrigin)
      for (int k = 0; k < 3; k++) point();
    if (f & kSoundBit) {
      const int w = m.u16();
      if (w & lim::kQ2LoopVolume) m.skip(1);
      if (w & lim::kQ2LoopAttenuation) m.skip(1);
    }
    if (f & kEvent) m.skip(1);
    if (f & kSolid) m.skip(4);
    sized_skip(lim::kQ2UMoreFx8, lim::kQ2UMoreFx16);
    if (f & lim::kQ2UAlpha) m.skip(1);
    if (f & lim::kQ2UScale) m.skip(1);
  }
  if (!m.bad && m.at <= m.b.size())
    out.insert(out.end(), m.b.begin() + static_cast<std::ptrdiff_t>(from),
               m.b.begin() + static_cast<std::ptrdiff_t>(m.at));
  return number != 0;
}

// One entity's fields (the later form) after its header into `s`; the header's flags in the
// older form back.
uint32_t read_entity(const Q2ProtoIn& p, client::DemoMsg& m, uint32_t f, Q2EntityState& s) {
  const bool wide_angles = p.protocol == 36 && p.minor >= 1018 && (f & kAngle16) != 0;
  if (f & kModel) s.model[0] = m.u8();
  if (f & kModel2) s.model[1] = m.u8();
  if (f & kModel3) s.model[2] = m.u8();
  if (f & kModel4) s.model[3] = m.u8();
  if (f & kFrameByte) s.frame = m.u8();
  if (f & kFrameShort) s.frame = m.u16();
  if (f & (kSkinLo | kSkinHi)) s.skin = sized(m, f, kSkinLo, kSkinHi);
  if (f & (kEffectsLo | kEffectsHi)) s.effects = sized(m, f, kEffectsLo, kEffectsHi);
  if (f & (kRenderLo | kRenderHi)) s.renderfx = sized(m, f, kRenderLo, kRenderHi);
  if (f & kOriginX) s.origin[0] = static_cast<int16_t>(m.i16());
  if (f & kOriginY) s.origin[1] = static_cast<int16_t>(m.i16());
  if (f & kOriginZ) s.origin[2] = static_cast<int16_t>(m.i16());
  const auto angle = [&] {
    return wide_angles ? angle_byte(m.i16()) : static_cast<uint8_t>(m.u8());
  };
  if (f & kAnglePitch) s.angles[0] = angle();
  if (f & kAngleYaw) s.angles[1] = angle();
  if (f & kAngleRoll) s.angles[2] = angle();
  if (f & kOldOrigin)
    for (int32_t& v : s.old_origin) v = static_cast<int16_t>(m.i16());
  if (f & kSoundBit) s.sound = m.u8();
  if (f & kEvent) s.event = m.u8();
  if (f & kSolid)
    s.solid = long_solid(p) ? q2_short_solid(static_cast<uint32_t>(m.i32())) : m.u16();
  // The older header: sizes chosen again by value, bit 13 and the chain bits cleared.
  uint32_t g = f & ~(kAngle16 | kMore1 | kMore2 | kMore3 | kSkinLo | kSkinHi | kEffectsLo |
                     kEffectsHi | kRenderLo | kRenderHi | kFrameByte | kFrameShort);
  if (f & (kFrameByte | kFrameShort)) g |= s.frame < 256 ? kFrameByte : kFrameShort;
  const auto size_of = [](uint32_t v, uint32_t lo, uint32_t hi) {
    return v < 256 ? lo : v < 32768 ? hi : (lo | hi);
  };
  if (f & (kSkinLo | kSkinHi)) g |= size_of(s.skin, kSkinLo, kSkinHi);
  if (f & (kEffectsLo | kEffectsHi)) g |= size_of(s.effects, kEffectsLo, kEffectsHi);
  if (f & (kRenderLo | kRenderHi)) g |= size_of(s.renderfx, kRenderLo, kRenderHi);
  return g;
}

// One entity's entry in the older form: the header (its chain bits set), the number, the fields.
void write_entity(std::vector<uint8_t>& out, uint32_t f, int number, const Q2EntityState& s) {
  f &= ~kNumberShort;
  if (number >= 256) f |= kNumberShort;
  sv::q2_write_entity(out, f, number, s);
}

// An entity entry read and written again; false at the list's end (number 0, written as two zero
// bytes).
bool copy_entity(const Q2ProtoIn& p, client::DemoMsg& m, std::vector<uint8_t>& out) {
  if (p.ext > 0) return copy_entity_ext(p, m, out);
  int number = 0;
  const uint32_t f = entity_header(m, number);
  if (number == 0) {
    out.push_back(0);
    out.push_back(0);
    return false;
  }
  Q2EntityState s;
  const uint32_t g = (f & kRemove) != 0 ? kRemove : read_entity(p, m, f, s);
  write_entity(out, g, number, s);
  return true;
}

// The player state in the later form over `s` (its base).
void read_player(const Q2ProtoIn& p, client::DemoMsg& m, uint32_t x, Q2PlayerState& s) {
  const bool v2 = p.ext >= 2;
  auto f = static_cast<uint32_t>(m.u16());
  if (p.ext >= 3 && (f & kMoreBitsPlayer) != 0) f |= static_cast<uint32_t>(m.u8()) << 16;
  const auto point = [&m, v2](int32_t& c) {
    c = v2 ? coord23(m, c) : static_cast<int16_t>(m.i16());
  };
  if (f & kPmType) s.type = static_cast<uint8_t>(m.u8());
  if (f & kPmOrigin) {
    point(s.origin[0]);
    point(s.origin[1]);
  }
  if (x & kXOriginZ) point(s.origin[2]);
  if (f & kPmVelocity) {
    point(s.velocity[0]);
    point(s.velocity[1]);
  }
  if (x & kXVelocityZ) point(s.velocity[2]);
  if (f & kPmTime) s.time = static_cast<uint16_t>(v2 ? m.u16() : m.u8());
  if (f & kPmFlags) s.flags = static_cast<uint16_t>(v2 ? m.u16() : m.u8());
  if (f & kPmGravity) s.gravity = static_cast<int16_t>(m.i16());
  if (f & kPmDelta)
    for (int16_t& v : s.delta_angles) v = static_cast<int16_t>(m.i16());
  if (f & kViewOffset)
    for (int8_t& v : s.view_offset) v = static_cast<int8_t>(m.i8());
  if (f & kViewAngles) {
    s.view_angles[0] = static_cast<int16_t>(m.i16());
    s.view_angles[1] = static_cast<int16_t>(m.i16());
  }
  if (x & kXRoll) s.view_angles[2] = static_cast<int16_t>(m.i16());
  if (f & kKick)
    for (int8_t& v : s.kick) v = static_cast<int8_t>(m.i8());
  if (f & kGunIndex) s.gun_index = static_cast<uint16_t>(p.ext >= 1 ? m.u16() : m.u8());
  if (f & kGunFrame) s.gun_frame = static_cast<uint8_t>(m.u8());
  if (x & kXGunOffset)
    for (int8_t& v : s.gun_offset) v = static_cast<int8_t>(m.i8());
  if (x & kXGunAngles)
    for (int8_t& v : s.gun_angles) v = static_cast<int8_t>(m.i8());
  if ((f & kBlend) && v2) {  // a byte of which follow: the blend's parts, the damage blend's
    const int which = m.u8();
    for (int k = 0; k < 4; k++)
      if (which & (1 << k)) s.blend[k] = static_cast<uint8_t>(m.u8());
    for (int k = 0; k < 4; k++)
      if (which & (16 << k)) s.damage_blend[k] = static_cast<uint8_t>(m.u8());
  } else if (f & kBlend) {
    for (uint8_t& v : s.blend) v = static_cast<uint8_t>(m.u8());
  }
  if (f & kFogPlayer) {  // the fog level's fog: read past, not drawn (the look is ours)
    const int bits = m.u8();
    const size_t sizes[6] = {3, 4, 2, 2, 3, 3};
    for (int k = 0; k < 6; k++)
      if (bits & (1 << k)) m.skip(sizes[k]);
    for (int k = 6; k < 8; k++)
      if (bits & (1 << k)) coord23(m, 0);
  }
  if (f & kFov) s.fov = static_cast<uint8_t>(m.u8());
  if (f & kRdFlags) s.rdflags = static_cast<uint8_t>(m.u8());
  if (x & kXStats) {
    const uint64_t mask = v2 ? varint(m) : static_cast<uint32_t>(m.i32());
    for (int i = 0; i < (v2 ? 64 : 32); i++)
      if (mask & (uint64_t{1} << i)) s.stats[i] = static_cast<int16_t>(m.i16());
  }
}

// The player state in the older form from `a` to `b`: the gun's frame carries its offset and
// angles, so any of the three changed sends all three; the stats' mask is always there.
void write_player(std::vector<uint8_t>& out, const Q2PlayerState& a, const Q2PlayerState& b) {
  const auto same3 = [](const auto& x, const auto& y) {
    return x[0] == y[0] && x[1] == y[1] && x[2] == y[2];
  };
  uint32_t f = 0;
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
  if (b.gun_index != a.gun_index) f |= kGunIndex;
  if (b.gun_frame != a.gun_frame || !same3(b.gun_offset, a.gun_offset) ||
      !same3(b.gun_angles, a.gun_angles))
    f |= kGunFrame;
  if (std::memcmp(b.blend, a.blend, sizeof b.blend) != 0) f |= kBlend;
  if (b.fov != a.fov) f |= kFov;
  if (b.rdflags != a.rdflags) f |= kRdFlags;
  Out o{out};
  o.u8(kPlayerInfo);
  o.i16(static_cast<int32_t>(f));
  if (f & kPmType) o.u8(b.type);
  if (f & kPmOrigin)
    for (const int32_t v : b.origin) o.i16(v);
  if (f & kPmVelocity)
    for (const int32_t v : b.velocity) o.i16(v);
  if (f & kPmTime) o.u8(b.time);
  if (f & kPmFlags) o.u8(b.flags);
  if (f & kPmGravity) o.i16(b.gravity);
  if (f & kPmDelta)
    for (const int16_t v : b.delta_angles) o.i16(v);
  if (f & kViewOffset)
    for (const int8_t v : b.view_offset) o.u8(static_cast<uint8_t>(v));
  if (f & kViewAngles)
    for (const int16_t v : b.view_angles) o.i16(v);
  if (f & kKick)
    for (const int8_t v : b.kick) o.u8(static_cast<uint8_t>(v));
  if (f & kGunIndex) o.u8(b.gun_index);
  if (f & kGunFrame) {
    o.u8(b.gun_frame);
    for (const int8_t v : b.gun_offset) o.u8(static_cast<uint8_t>(v));
    for (const int8_t v : b.gun_angles) o.u8(static_cast<uint8_t>(v));
  }
  if (f & kBlend)
    for (const uint8_t v : b.blend) o.u8(v);
  if (f & kFov) o.u8(b.fov);
  if (f & kRdFlags) o.u8(b.rdflags);
  uint32_t mask = 0;
  for (int i = 0; i < 32; i++)
    if (b.stats[i] != a.stats[i]) mask |= 1u << i;
  o.i32(static_cast<int64_t>(mask));
  for (int i = 0; i < 32; i++)
    if (mask & (1u << i)) o.i16(b.stats[i]);
}

struct Run {
  Q2ProtoIn& p;
  std::vector<std::vector<uint8_t>>& msgs;  // the older form's messages, one each
  std::string& error;

  std::vector<uint8_t>& next() { return msgs.emplace_back(); }
  bool fail(const std::string& what, const client::DemoMsg& m) {
    error = what + " at byte " + std::to_string(m.at);
    return false;
  }

  bool server_data(client::DemoMsg& m) {
    Out o{next()};
    p.protocol = m.i32();
    p.has_server_data = true;
    const int32_t count = m.i32();
    const int attract = m.u8();
    const std::string game = m.str();
    const int player = m.i16();
    const std::string level = m.str();
    p.strafe_hack = p.qw_move = p.waterjump_hack = false;
    p.server_state = 2;
    p.ext = 0;
    if (p.protocol == 35) {
      const int enhanced = m.u8();
      p.minor = m.i16();
      m.u8();
      p.strafe_hack = m.u8() != 0;
      if (enhanced != 0)
        return fail("This client does not support the enhanced form of protocol 35", m);
      if (p.minor < 1903 || p.minor > 1905) {  // taken as the newest, with a warning
        p.warnings.push_back("The server reports protocol 35 version " + std::to_string(p.minor) +
                             ". Assuming it really uses version 1905; things will break if it "
                             "does not!");
        p.minor = 1905;
      }
    } else if (p.protocol == 36) {
      p.minor = m.i16();
      if (p.minor < 1015 || p.minor > 1026)
        return fail("The server reports protocol 36 version " + std::to_string(p.minor) +
                        "; this client's is 1026",
                    m);
      p.server_state = m.u8();
      if (p.minor >= 1024) {
        const int flags = m.i16();
        p.strafe_hack = (flags & lim::kQ2ServerFlagStrafeJump) != 0;
        p.qw_move = (flags & lim::kQ2ServerFlagQwMode) != 0;
        p.waterjump_hack = (flags & lim::kQ2ServerFlagWaterJump) != 0;
        const bool one = (flags & lim::kQ2ServerFlagExtensions) != 0;
        const bool two =
            p.minor >= lim::kQ2MinorExtensions2 && (flags & lim::kQ2ServerFlagExtensions2) != 0;
        if (two && !one)
          return fail("The server turned on the second protocol extensions without the first", m);
        p.ext = !one ? 0 : !two ? 1 : p.minor >= lim::kQ2MinorMoreBits ? 3 : 2;
      } else {
        p.strafe_hack = m.u8() != 0;
        p.qw_move = m.u8() != 0;
        p.waterjump_hack = m.u8() != 0;
      }
    }
    // The recording's protocol: 34, or under the extended limits the recording of their level
    // (3434, or 3435 for the second; the fog level's fog is not carried, so it is 3435 too).
    const int written = p.ext > 0 ? 3433 + std::min(p.ext, 2) : 34;
    p.scratch.state.protocol = written;  // the copied messages' ends found at that level
    o.u8(kServerData);
    o.i32(written);
    o.i32(count);
    o.u8(static_cast<uint32_t>(attract));
    o.str(game);
    o.i16(player);
    o.str(level);
    for (int32_t& f : p.view_frame) f = -1;
    p.client_number = -1;
    return true;
  }

  // Configstrings (a word index and a string) to the end marker.
  bool config_strings(client::DemoMsg& m) {
    for (;;) {
      const int index = m.u16();
      if (m.bad) return fail("the configstrings run past their message's end", m);
      if (index == slots(p)) return true;
      if (index > slots(p)) return fail("a configstring numbered " + std::to_string(index), m);
      Out o{next()};
      o.u8(kConfigString);
      o.i16(index);
      o.str(m.str());
    }
  }
  // Baselines to the end (an entity number of 0).
  bool baselines(client::DemoMsg& m) {
    for (;;) {
      if (p.ext > 0) {
        std::vector<uint8_t> one{kBaseline};
        if (!copy_entity_ext(p, m, one)) return !m.bad || fail("the baselines run past", m);
        if (m.bad) return fail("the baselines run past their message's end", m);
        next() = std::move(one);
        continue;
      }
      int number = 0;
      const uint32_t f = entity_header(m, number);
      if (m.bad) return fail("the baselines run past their message's end", m);
      if (number == 0) return true;
      Q2EntityState s;
      const uint32_t g = read_entity(p, m, f, s);
      std::vector<uint8_t>& out = next();
      out.push_back(kBaseline);
      write_entity(out, g, number, s);
    }
  }

  bool frame(client::DemoMsg& m, uint32_t top) {
    const auto word = static_cast<uint32_t>(m.i32());
    const auto number = static_cast<int32_t>(word & 0x07FFFFFFu);
    const uint32_t back = word >> 27;
    const int32_t delta = back == 31 ? -1 : number - static_cast<int32_t>(back);
    const int low = m.u8();
    const uint32_t x = static_cast<uint32_t>(low >> 4) | top << 4;
    const int held = p.protocol == 35 ? (low & 15) : (low & 1);
    const size_t areas = static_cast<size_t>(m.u8());
    if (areas > 32) return fail("a frame with " + std::to_string(areas) + " bytes of areas", m);
    std::vector<uint8_t>& out = next();
    Out o{out};
    o.u8(kFrame);
    o.i32(number);
    o.i32(delta);
    o.u8(static_cast<uint32_t>(held));
    o.u8(static_cast<uint32_t>(areas));
    for (size_t k = 0; k < areas; k++) o.u8(static_cast<uint32_t>(m.u8()));
    const size_t base_slot = static_cast<size_t>(delta) & 31;
    const bool have_base = delta >= 0 && p.view_frame[base_slot] == delta;
    const Q2PlayerState zero;
    const Q2PlayerState& a = have_base ? p.views[base_slot] : zero;
    Q2PlayerState b = a;
    read_player(p, m, x, b);
    if (x & kXClient) p.client_number = p.minor >= 1022 ? m.i16() : m.u8();
    if (p.ext > 0)
      sv::q2_write_player_ext(out, a, b, std::min(p.ext, 2));
    else
      write_player(out, a, b);
    const size_t slot = static_cast<size_t>(number) & 31;
    p.views[slot] = b;
    p.view_frame[slot] = number;
    o.u8(kPacketEntities);
    while (!m.bad && copy_entity(p, m, out)) {
    }
    return !m.bad || fail("the frame runs past the packet's end", m);
  }

  bool zpacket(client::DemoMsg& m, bool inside) {
    const size_t packed = static_cast<size_t>(m.u16());
    const size_t size = static_cast<size_t>(m.u16());
    if (inside) return fail("a compressed packet inside a compressed packet", m);
    if (size > kMostInflated || m.at + packed > m.b.size())
      return fail("a compressed packet of " + std::to_string(size) + " bytes", m);
    const auto inflated = ge::core::inflate_raw(m.b.subspan(m.at, packed), size);
    m.skip(packed);
    if (!inflated) return fail("a compressed packet that does not inflate", m);
    return messages(*inflated, true);
  }

  bool zdownload(client::DemoMsg& m) {
    const int size = m.i16();
    const int percent = m.u8();
    const int whole = p.protocol == 35 ? m.i16() : 0;
    if (size < 0 || m.at + static_cast<size_t>(size) > m.b.size())
      return fail("a compressed download piece of " + std::to_string(size) + " bytes", m);
    const auto piece = m.b.subspan(m.at, static_cast<size_t>(size));
    m.skip(static_cast<size_t>(size));
    std::vector<uint8_t> plain;
    if (p.protocol == 35) {  // each piece its own stream
      const auto inflated = ge::core::inflate_raw(piece, static_cast<size_t>(std::max(whole, 0)));
      if (!inflated) return fail("a compressed download piece that does not inflate", m);
      plain = *inflated;
    } else {  // one stream, inflated once whole
      p.zdl.insert(p.zdl.end(), piece.begin(), piece.end());
      if (percent == 100) {
        const auto inflated = ge::core::inflate_raw_any(p.zdl, kMostDownload);
        p.zdl.clear();
        if (!inflated) return fail("a compressed download that does not inflate", m);
        p.download = *inflated;
        p.download_ready = true;
      }
    }
    // Handed on as plain pieces of at most 1024 bytes (the last carrying the percent).
    size_t at = 0;
    do {
      const size_t n = std::min<size_t>(1024, plain.size() - at);
      const bool last = at + n >= plain.size();
      Out o{next()};
      o.u8(kDownload);
      o.i16(static_cast<int32_t>(n));
      o.u8(static_cast<uint32_t>(last ? percent : std::min(percent, 99)));
      msgs.back().insert(msgs.back().end(), plain.begin() + static_cast<std::ptrdiff_t>(at),
                         plain.begin() + static_cast<std::ptrdiff_t>(at + n));
      at += n;
    } while (at < plain.size());
    return true;
  }

  // A message of protocol 34's own form: copied whole, its end found by the recording's reader.
  bool copy(std::span<const uint8_t> b, client::DemoMsg& m, size_t at) {
    size_t end = 0;
    std::string why;
    if (!client::q2_message_end(p.scratch, b, at, end, why)) {
      error = why;
      return false;
    }
    next().assign(b.begin() + static_cast<std::ptrdiff_t>(at),
                  b.begin() + static_cast<std::ptrdiff_t>(end));
    m.at = end;
    return true;
  }

  bool messages(std::span<const uint8_t> b, bool inside) {
    client::DemoMsg m{b};
    while (!m.done()) {
      const size_t at = m.at;
      const int byte = m.u8();
      if (byte >= kGameOwnFirst && byte <= kGameOwnLast) {
        if (!copy(b, m, at)) return false;
        continue;
      }
      const int id = byte & 31;
      const uint32_t top = static_cast<uint32_t>(byte) >> 5;
      if (top != 0 && id != kFrame) return fail("a message " + std::to_string(byte), m);
      bool ok = true;
      switch (id) {
        case kServerData:
          ok = server_data(m);
          break;
        case kBaseline: {
          if (p.ext > 0) {
            std::vector<uint8_t>& out = next();
            out.push_back(kBaseline);
            copy_entity_ext(p, m, out);
            break;
          }
          int number = 0;
          const uint32_t f = entity_header(m, number);
          Q2EntityState s;
          const uint32_t g = read_entity(p, m, f, s);
          std::vector<uint8_t>& out = next();
          out.push_back(kBaseline);
          write_entity(out, g, number, s);
          break;
        }
        case kFrame:
          ok = frame(m, top);
          break;
        case kZPacket:
          ok = zpacket(m, inside);
          break;
        case kZDownload:
          ok = zdownload(m);
          break;
        case kGameState:
          if (p.protocol != 36) return fail("a player update, never asked for", m);
          ok = config_strings(m) && baselines(m);
          break;
        case kSetting: {
          const int32_t index = m.i32(), value = m.i32();
          if (index == 1 && value > 0) p.server_fps = value;
          break;
        }
        case kConfigStream:
          ok = config_strings(m);
          break;
        case kBaselineStream:
          ok = baselines(m);
          break;
        default:
          if (id > kBaselineStream) return fail("a message " + std::to_string(byte), m);
          ok = copy(b, m, at);
          break;
      }
      if (!ok) return false;
      if (m.bad) return fail("the message " + std::to_string(id) + " runs past its end", m);
    }
    return true;
  }
};

}  // namespace

int q2_short_solid(uint32_t s) {
  if (s == 31 || s == 0) return static_cast<int>(s);
  const int x = static_cast<int>(s & 0xFFu), down = static_cast<int>((s >> 8) & 0xFFu);
  const int up = static_cast<int>(s >> 16) - 32768;
  const int a = std::clamp(x / 8, 1, 31), b = std::clamp(down / 8, 0, 31);
  const int c = std::clamp((up + 32) / 8, 0, 63);
  const int packed = a | b << 5 | c << 10;
  return packed == 31 ? 0 : packed;
}

bool q2_proto_in(Q2ProtoIn& p, std::span<const uint8_t> in,
                 std::vector<std::vector<uint8_t>>& blocks, std::string& error) {
  if (!p.started) {
    client::q2_stream_begin(p.scratch);
    for (int32_t& f : p.view_frame) f = -1;
    p.started = true;
  }
  std::vector<std::vector<uint8_t>> msgs;
  Run r{p, msgs, error};
  const bool ok = r.messages(in, false);
  client::q2_scratch_clear(p.scratch);
  if (!ok) return false;
  // Packed into blocks of at most 1400 bytes; a longer message alone.
  const size_t first = blocks.size();  // a payload's own blocks: one frame a block, as it came
  for (const std::vector<uint8_t>& one : msgs) {
    if (blocks.size() == first || blocks.back().size() + one.size() > kMostBlock)
      blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), one.begin(), one.end());
  }
  return true;
}

}  // namespace shooters_edge::net
