// Quake II's multiview recording read (demo_mvd2.hpp): the reader that keeps every player and
// entity, and the stream that gives one player's view as protocol 34 blocks.
//
// ge:summary The .mvd2 reader and the stream giving one player's view as a .dm2
// ge:keywords mvd2 multiview q2 dm2 convert pov follow-player gamestate player-delta
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_mvd2_test.cpp
#include "shooters_edge/client/demo_mvd2.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::client {

namespace {

namespace sv = server;
using sv::Q2EntityState;
using sv::Q2PlayerState;

// Protocol 34's message ids, as a view writes them.
// The game's own message to the demo player: every player's state (demo_mvd2.hpp).
constexpr uint8_t kGeStats = 125;
// The player state's stats the scan reads, and where the models' settings strings start.
constexpr int kStatHealthSq = 1, kStatAmmoSq = 3, kStatArmourSq = 5, kStatFragsSq = 14;
constexpr int kModelStrings = 32;
constexpr uint8_t kSound = 9, kPrint = 10, kStuffText = 11, kServerData = 12, kConfigString = 13,
                  kBaseline = 14, kPacketEntities = 18, kFrame = 20;

constexpr int kSlots = sv::kQ2ConfigStrings;  // the settings strings' end marker
constexpr int kCsName = 0, kCsMaxClients = 30;
constexpr size_t kMostBlock = 1400;  // a stock client's block

// An entity delta's flag bits (protocol 34's).
constexpr uint32_t kOriginX = 1u << 0, kOriginY = 1u << 1, kAngleYaw = 1u << 2,
                   kAngleRoll = 1u << 3, kFrameByte = 1u << 4, kEvent = 1u << 5, kRemove = 1u << 6,
                   kMore1 = 1u << 7, kNumberShort = 1u << 8, kOriginZ = 1u << 9,
                   kAnglePitch = 1u << 10, kModel = 1u << 11, kRenderLo = 1u << 12,
                   kEffectsLo = 1u << 14, kMore2 = 1u << 15, kSkinLo = 1u << 16,
                   kFrameShort = 1u << 17, kRenderHi = 1u << 18, kEffectsHi = 1u << 19,
                   kModel2 = 1u << 20, kModel3 = 1u << 21, kModel4 = 1u << 22, kMore3 = 1u << 23,
                   kOldOrigin = 1u << 24, kSkinHi = 1u << 25, kSoundBit = 1u << 26,
                   kSolid = 1u << 27;
constexpr uint32_t kBeam = 128;  // the render effect whose old origin is the beam's far end

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

void read_player(DemoMsg& m, uint32_t f, Q2PlayerState& s) {
  if (f & sv::kMv2Type) s.type = static_cast<uint8_t>(m.u8());
  if (f & sv::kMv2OriginXY) {
    s.origin[0] = static_cast<int16_t>(m.i16());
    s.origin[1] = static_cast<int16_t>(m.i16());
  }
  if (f & sv::kMv2OriginZ) s.origin[2] = static_cast<int16_t>(m.i16());
  if (f & sv::kMv2ViewOffset)
    for (int8_t& v : s.view_offset) v = static_cast<int8_t>(m.i8());
  if (f & sv::kMv2ViewAngles) {
    s.view_angles[0] = static_cast<int16_t>(m.i16());
    s.view_angles[1] = static_cast<int16_t>(m.i16());
  }
  if (f & sv::kMv2ViewRoll) s.view_angles[2] = static_cast<int16_t>(m.i16());
  if (f & sv::kMv2Kick)
    for (int8_t& v : s.kick) v = static_cast<int8_t>(m.i8());
  if (f & sv::kMv2GunIndex) s.gun_index = static_cast<uint8_t>(m.u8());
  if (f & sv::kMv2GunFrame) s.gun_frame = static_cast<uint8_t>(m.u8());
  if (f & sv::kMv2GunOffset)
    for (int8_t& v : s.gun_offset) v = static_cast<int8_t>(m.i8());
  if (f & sv::kMv2GunAngles)
    for (int8_t& v : s.gun_angles) v = static_cast<int8_t>(m.i8());
  if (f & sv::kMv2Blend)
    for (uint8_t& v : s.blend) v = static_cast<uint8_t>(m.u8());
  if (f & sv::kMv2Fov) s.fov = static_cast<uint8_t>(m.u8());
  if (f & sv::kMv2RdFlags) s.rdflags = static_cast<uint8_t>(m.u8());
  if (f & sv::kMv2Stats) {
    const auto mask = static_cast<uint32_t>(m.i32());
    for (int i = 0; i < 32; i++)
      if (mask & (1u << i)) s.stats[i] = static_cast<int16_t>(m.i16());
  }
}

// --- the entity entry ---

// The entry's header: its flag bits and its number (0 with no bits: the list's end).
uint32_t entity_header(DemoMsg& m, int& number) {
  auto f = static_cast<uint32_t>(m.u8());
  if (f & kMore1) f |= static_cast<uint32_t>(m.u8()) << 8;
  if (f & kMore2) f |= static_cast<uint32_t>(m.u8()) << 16;
  if (f & kMore3) f |= static_cast<uint32_t>(m.u8()) << 24;
  number = (f & kNumberShort) ? m.u16() : m.u8();
  return f;
}

uint32_t sized(DemoMsg& m, uint32_t f, uint32_t lo, uint32_t hi) {
  if ((f & lo) && (f & hi)) return static_cast<uint32_t>(m.i32());
  if (f & hi) return static_cast<uint32_t>(m.u16());
  return static_cast<uint32_t>(m.u8());
}

// The fields after the header onto `s` (the base already in it).
void entity_fields(DemoMsg& m, uint32_t f, Q2EntityState& s) {
  const Q2EntityState base = s;
  s.event = 0;
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
  if (f & kAnglePitch) s.angles[0] = static_cast<uint8_t>(m.u8());
  if (f & kAngleYaw) s.angles[1] = static_cast<uint8_t>(m.u8());
  if (f & kAngleRoll) s.angles[2] = static_cast<uint8_t>(m.u8());
  if (f & kOldOrigin) {
    for (int32_t& v : s.old_origin) v = static_cast<int16_t>(m.i16());
  } else if ((s.renderfx & kBeam) == 0) {
    std::memcpy(s.old_origin, base.origin, sizeof s.old_origin);
  }
  if (f & kSoundBit) s.sound = m.u8();
  if (f & kEvent) s.event = m.u8();
  if (f & kSolid) s.solid = m.u16();
}

// A player's entity placed from his state: the origin his, the pitch a third of his view's
// (a pitch past half a turn wrapped below 0 first), the yaw his view's, no roll.

bool read_body(DemoMsg& m, Mvd2Frame& f, std::string& error) {
  const int portals = m.u8();
  m.skip(static_cast<size_t>(portals));
  for (;;) {
    const int c = m.u8();
    if (m.bad) break;
    if (c == 255) break;
    const auto flags = static_cast<uint32_t>(m.u16());
    if (size_t(c) >= f.players.size()) {
      error = "a player numbered " + std::to_string(c) + " past the game's " +
              std::to_string(f.players.size()) + " at byte " + std::to_string(m.at);
      return false;
    }
    Mvd2Player& p = f.players[size_t(c)];
    if (flags & sv::kMv2Removed) {
      p.in = false;
      continue;
    }
    read_player(m, flags, p.s);
    p.in = true;
  }
  for (;;) {
    int n = 0;
    const uint32_t bits = entity_header(m, n);
    if (m.bad) break;
    if (n == 0 && (bits & ~kNumberShort) == 0) break;
    if (n <= 0 || n >= sv::kQ2Entities) {
      error = "an entity numbered " + std::to_string(n) + " (1 to " +
              std::to_string(sv::kQ2Entities - 1) + ") at byte " + std::to_string(m.at);
      return false;
    }
    Q2EntityState& e = f.entities[size_t(n)];
    entity_fields(m, bits, e);  // read even when removed: the game state keeps old slots so
    e.present = (bits & kRemove) == 0;
    if (!e.present) e = Q2EntityState{};
  }
  for (int n = 1; n < sv::kQ2Entities; n++)
    if (f.entities[size_t(n)].present && sv::mvd2_placed(f.players, n)) {
      const int c = n - 1;
      sv::mvd2_place_from_player(f.entities[size_t(n)], f.players[size_t(c)].s);
    }
  if (m.bad) {
    error = "the frame runs past its block";
    return false;
  }
  return true;
}

// --- the reader ---

// What one block of the source held besides the state it leaves.
struct Mvd2Block {
  bool level = false;  // a game state: a level starts
  bool frame = false;  // a frame was read (a game state holds one)
  Mvd2Frame changes;   // its settings strings, prints, unicasts, multicasts and sounds
};

struct Mvd2Reader {
  bool have_level = false;
  Mvd2Level level;  // the settings strings as they stand now, the game folder, the count
  Mvd2Frame now;    // the players and entities as they stand now
  int max_clients = 0;
  int64_t frames = 0;

  bool server_data(DemoMsg& m, int extra, Mvd2Block& b, std::string& error) {
    const int32_t major = m.i32();
    const int minor = m.u16();
    if (major != sv::kMvd2Major || minor < 2009 || minor > 2013) {
      error = "the multiview recording's version is " + std::to_string(major) + "." +
              std::to_string(minor) + ", not 37.2009 to 37.2013";
      return false;
    }
    const int flags = minor >= 2012 ? m.u16() : extra;
    if (flags & (4 | 8)) {
      error =
          "the multiview recording was made with the extended limits, which this reader "
          "does not take yet";
      return false;
    }
    level = Mvd2Level{};
    level.server_count = m.i32();
    level.game = m.str();
    level.observer = m.i16();
    level.flags = static_cast<uint8_t>(flags & 0xFF);
    level.config_strings.assign(size_t(kSlots), std::string());
    for (;;) {
      const int i = m.u16();
      if (m.bad || i == kSlots) break;
      if (i > kSlots) {
        error =
            "a settings string numbered " + std::to_string(i) + " past " + std::to_string(kSlots);
        return false;
      }
      level.config_strings[size_t(i)] = m.str();
    }
    max_clients =
        std::clamp(std::atoi(level.config_strings[kCsMaxClients].c_str()), 1, sv::kMvd2MostClients);
    now = Mvd2Frame{};
    now.players.assign(size_t(max_clients), Mvd2Player{});
    now.entities.assign(size_t(sv::kQ2Entities), Q2EntityState{});
    if (!read_body(m, now, error)) return false;
    have_level = true;
    b.level = b.frame = true;
    return true;
  }

  // One block of messages (without its length).
  bool block(std::span<const uint8_t> bytes, Mvd2Block& b, std::string& error) {
    b = Mvd2Block{};
    DemoMsg m{bytes};
    while (!m.done() && !m.bad) {
      const size_t at = m.at;
      const int cmd = m.u8();
      const int id = cmd & 0x1F, extra = cmd >> 5;
      if (id != sv::kMv2ServerData && !have_level) {
        error = "a message (" + std::to_string(id) + ") at byte " + std::to_string(at) +
                " before the game state";
        return false;
      }
      switch (id) {
        case sv::kMv2Nop:
          break;
        case sv::kMv2ServerData:
          if (!server_data(m, extra, b, error)) {
            error += " (at byte " + std::to_string(at) + ")";
            return false;
          }
          break;
        case sv::kMv2ConfigString: {
          const int i = m.u16();
          std::string s = m.str();
          if (i >= kSlots) {
            error = "a settings string numbered " + std::to_string(i) + " at byte " +
                    std::to_string(at);
            return false;
          }
          level.config_strings[size_t(i)] = s;
          b.changes.config_strings.emplace_back(i, std::move(s));
          break;
        }
        case sv::kMv2Frame:
          if (!read_body(m, now, error)) {
            error += " (in the frame at byte " + std::to_string(at) + ")";
            return false;
          }
          b.frame = true;
          frames++;
          break;
        case sv::kMv2Unicast:
        case sv::kMv2UnicastReliable: {
          const size_t n = static_cast<size_t>(m.u8()) | (static_cast<size_t>(extra) << 8);
          const int c = m.u8();
          if (m.at + n > bytes.size()) break;
          b.changes.unicasts.emplace_back(
              c, std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(m.at),
                                      bytes.begin() + static_cast<std::ptrdiff_t>(m.at + n)));
          m.skip(n);
          break;
        }
        case sv::kMv2MulticastAll:
        case sv::kMv2MulticastHear:
        case sv::kMv2MulticastSee:
        case sv::kMv2MulticastAllReliable:
        case sv::kMv2MulticastHearReliable:
        case sv::kMv2MulticastSeeReliable: {
          const size_t n = static_cast<size_t>(m.u8()) | (static_cast<size_t>(extra) << 8);
          if (id != sv::kMv2MulticastAll && id != sv::kMv2MulticastAllReliable)
            m.skip(2);  // the leaf
          if (m.at + n > bytes.size()) break;
          b.changes.multicasts.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(m.at),
                                            bytes.begin() + static_cast<std::ptrdiff_t>(m.at + n));
          m.skip(n);
          break;
        }
        case sv::kMv2Sound: {
          Mvd2Sound s;
          const int flags = m.u8();
          s.index = m.u8();
          if (flags & 1) s.volume = m.u8();
          if (flags & 2) s.attenuation = m.u8();
          if (flags & 16) s.offset = m.u8();
          const int ec = m.u16();
          s.entity = ec >> 3;
          s.channel = ec & 7;
          b.changes.sounds.push_back(s);
          break;
        }
        case sv::kMv2Print: {
          const int level_ = m.u8();
          b.changes.prints.emplace_back(level_, m.str());
          break;
        }
        default:
          error = "an unknown message (" + std::to_string(id) + ") at byte " + std::to_string(at);
          return false;
      }
    }
    if (m.bad) {
      error = "a message runs past the end of its block";
      return false;
    }
    return true;
  }
};

// --- one player's view as protocol 34 ---

struct Mvd2ToQ2 final : ConvertStream {
  Mvd2Reader r;
  bool magic = false, stopped = false, end_written = false;
  int pov = -2;        // the view's player (-2: none yet)
  bool signon = true;  // the next frame starts the view again
  int32_t number = 0;  // the view's last frame number
  int32_t last_frame = -1;
  std::vector<Q2EntityState> held, baselines;
  Q2PlayerState held_player;
  std::vector<std::vector<uint8_t>> blocks;

  std::unique_ptr<ConvertStream> clone() const override {
    auto copy = std::make_unique<Mvd2ToQ2>(*this);
    copy->blocks.clear();
    return copy;
  }

  void message(const std::vector<uint8_t>& msg) {
    if (blocks.empty() || blocks.back().size() + msg.size() > kMostBlock) blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), msg.begin(), msg.end());
  }

  void write_signon(DemoConvert& c) {
    const Q2EntityState zero;
    std::vector<uint8_t> msg;
    Out o{msg};
    o.u8(kServerData);
    o.i32(sv::kQ2Protocol);
    o.i32(r.level.server_count);
    o.u8(1);  // a recording
    o.str(r.level.game);
    o.i16(pov);
    o.str(r.level.config_strings[kCsName]);
    blocks.emplace_back(msg);
    for (int i = 0; i < kSlots; i++) {
      const std::string& s = r.level.config_strings[size_t(i)];
      if (s.empty()) continue;
      msg.clear();
      o.u8(kConfigString);
      o.i16(i);
      o.str(s);
      if (blocks.back().size() + msg.size() + 32 > kMostBlock) blocks.emplace_back();
      blocks.back().insert(blocks.back().end(), msg.begin(), msg.end());
    }
    baselines.assign(size_t(sv::kQ2Entities), Q2EntityState{});
    for (int n = 1; n < sv::kQ2Entities; n++) {
      Q2EntityState s = r.now.entities[size_t(n)];
      if (!s.present) continue;
      s.event = 0;
      baselines[size_t(n)] = s;
      msg.clear();
      o.u8(kBaseline);
      sv::q2_write_entity(msg, sv::q2_entity_flags(zero, s, n, true), n, s);
      if (blocks.back().size() + msg.size() + 64 > kMostBlock) blocks.emplace_back();
      blocks.back().insert(blocks.back().end(), msg.begin(), msg.end());
    }
    msg.clear();
    o.u8(kStuffText);
    o.str("precache\n");
    if (blocks.back().size() + msg.size() + 16 > kMostBlock) blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), msg.begin(), msg.end());
    held.assign(size_t(sv::kQ2Entities), Q2EntityState{});
    held_player = Q2PlayerState{};
    last_frame = -1;
    signon = false;
    c.notes.levels++;
  }

  void write_frame(DemoConvert& c, const Mvd2Block& b, bool fresh_signon) {
    std::vector<uint8_t> msg;
    Out o{msg};
    // What is sent reliably: the settings strings that changed (a fresh signon holds them),
    // the prints, the messages to this player.
    if (!fresh_signon)
      for (const auto& [i, s] : b.changes.config_strings) {
        msg.clear();
        o.u8(kConfigString);
        o.i16(i);
        o.str(s);
        message(msg);
      }
    for (const auto& [level, said] : b.changes.prints) {
      std::string text = said;
      if (c.options.touch_print) c.options.touch_print(level, text);
      msg.clear();
      o.u8(kPrint);
      o.u8(static_cast<uint32_t>(level));
      o.str(text);
      message(msg);
    }
    for (const auto& [to, bytes] : b.changes.unicasts) {
      if (to != pov) continue;
      if (bytes.size() > kMostBlock) {
        c.notes.dropped["a message to the player too long for a block"]++;
        continue;
      }
      message(bytes);
    }
    // The frame starts a block of its own unless most of one is free.
    if (blocks.empty() || blocks.back().size() > 400) blocks.emplace_back();
    std::vector<uint8_t>& block = blocks.back();
    Out f{block};
    number++;
    f.u8(kFrame);
    f.i32(number);
    f.i32(last_frame);
    f.u8(0);
    f.u8(32);
    for (int k = 0; k < 32; k++) f.u8(0xFF);
    Q2PlayerState p = held_player;
    if (size_t(pov) < r.now.players.size() && r.now.players[size_t(pov)].in)
      p = r.now.players[size_t(pov)].s;
    if (const int obs = r.level.observer; c.options.touch_observer && obs >= 0 &&
                                          size_t(obs) < r.now.players.size() &&
                                          r.now.players[size_t(obs)].in)
      c.options.touch_observer(pov, obs, r.now.players[size_t(obs)].s, p);
    if (c.options.touch_player) c.options.touch_player(pov, p.type, p.flags, p.stats);
    std::vector<uint8_t> one;
    sv::q2_write_player(one, last_frame < 0 ? Q2PlayerState{} : held_player, p);
    f.bytes(one);
    held_player = p;
    f.u8(kPacketEntities);
    const Q2EntityState zero;
    const size_t room = kMostBlock - 2 - 200;
    bool waited = false;
    for (int n = 1; n < sv::kQ2Entities; n++) {
      Q2EntityState s = r.now.entities[size_t(n)];
      const Q2EntityState& was = held[size_t(n)];
      if (block.size() + 44 > room) {
        waited = true;
        break;
      }
      if (!s.present) {
        if (was.present) {
          one.clear();
          sv::q2_write_entity(one, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
          f.bytes(one);
        }
        held[size_t(n)] = Q2EntityState{};
        continue;
      }
      const bool fresh = !was.present || last_frame < 0;
      const Q2EntityState& base =
          fresh ? (baselines[size_t(n)].present ? baselines[size_t(n)] : zero) : was;
      if ((s.renderfx & kBeam) == 0) std::memcpy(s.old_origin, base.origin, sizeof s.old_origin);
      const bool player = n <= r.max_clients;
      const uint32_t fl = sv::q2_entity_flags(base, s, n, fresh || player);
      if ((fl & ~kNumberShort) != 0) {
        one.clear();
        sv::q2_write_entity(one, fl, n, s);
        f.bytes(one);
      }
      s.event = 0;
      held[size_t(n)] = s;
    }
    if (waited) c.notes.rewritten["an entity held to a later frame (the block was full)"]++;
    f.u8(0);
    f.u8(0);
    last_frame = number;
    // What is not: the effects to all and the sounds, while there is room.
    for (const std::vector<uint8_t>& bytes : b.changes.multicasts) {
      if (block.size() + bytes.size() > kMostBlock) {
        c.notes.dropped["an effect past a full block"]++;
        continue;
      }
      f.bytes(bytes);
    }
    for (const Mvd2Sound& s : b.changes.sounds) {
      if (block.size() + 9 > kMostBlock) {
        c.notes.dropped["a sound past a full block"]++;
        continue;
      }
      uint32_t flags = 8;  // the entity and channel; the point is the entity's
      if (s.volume != 255) flags |= 1;
      if (s.attenuation != 64) flags |= 2;
      if (s.offset != 0) flags |= 16;
      f.u8(kSound);
      f.u8(flags);
      f.u8(static_cast<uint32_t>(s.index));
      if (flags & 1) f.u8(static_cast<uint32_t>(s.volume));
      if (flags & 2) f.u8(static_cast<uint32_t>(s.attenuation));
      if (flags & 16) f.u8(static_cast<uint32_t>(s.offset));
      f.i16(s.entity * 8 + s.channel);
    }
    if (c.options.player_stats) write_stats(f);
    c.notes.frames++;
  }

  // The active-weapon bit (1 shotgun .. 64 beam gun, 0 none) of the weapon whose view model is
  // the player's gun.
  int weapon_bit(const Q2PlayerState& s) const {
    const size_t cs = size_t(kModelStrings) + s.gun_index;
    if (s.gun_index == 0 || cs >= r.level.config_strings.size()) return 0;
    const std::string& model = r.level.config_strings[cs];
    for (int k = 0; k < server::kSqWeaponCount; k++) {
      if (model != server::kSqWeapons[k].view_model) continue;
      switch (server::kSqWeapons[k].shot) {
        case server::SqShot::pellets:
          return std::string(server::kSqWeapons[k].name).find("Super") == 0 ? 2 : 1;
        case server::SqShot::bullet:
          return std::string(server::kSqWeapons[k].name) == "Chaingun" ? 8 : 4;
        case server::SqShot::grenade:
        case server::SqShot::hand_grenade:
          return 16;
        case server::SqShot::rocket:
          return 32;
        case server::SqShot::rail:
        case server::SqShot::bfg:
          return 64;
        case server::SqShot::bolt:
        case server::SqShot::ripper:
        case server::SqShot::blue_bolt:
        case server::SqShot::spit:
          return k == 0 ? 0 : 64;  // the blaster none; the hyperblaster a beam gun
        case server::SqShot::plasma:
        case server::SqShot::heat_rocket:
          return 32;
        case server::SqShot::trap:
          return 16;
      }
    }
    return 0;
  }

  // Every player's state the demo player's scan reads (demo_mvd2.hpp), the game's message 125.
  void write_stats(Out& f) const {
    int n = 0;
    for (const Mvd2Player& p : r.now.players) n += p.in ? 1 : 0;
    f.u8(kGeStats);
    f.u8(static_cast<uint32_t>(n));
    for (size_t i = 0; i < r.now.players.size(); i++) {
      const Mvd2Player& p = r.now.players[i];
      if (!p.in) continue;
      const int bit = weapon_bit(p.s);
      f.u8(static_cast<uint32_t>(i));
      f.u8(p.s.type);
      f.i16(p.s.stats[kStatHealthSq]);
      f.i16(p.s.stats[kStatArmourSq]);
      f.i16(p.s.stats[kStatFragsSq]);
      f.i16(bit);
      f.i16(bit == 32 ? p.s.stats[kStatAmmoSq] : -1);
    }
  }

  // The player a view of `-1` follows: the lowest one in the game but the recording's own
  // observer (the recorder's spectator), else that observer, else -2.
  int lowest_playing() const {
    const int observer = r.level.observer;
    for (size_t i = 0; i < r.now.players.size(); i++)
      if (r.now.players[i].in && static_cast<int>(i) != observer) return static_cast<int>(i);
    if (observer >= 0 && size_t(observer) < r.now.players.size() &&
        r.now.players[size_t(observer)].in)
      return observer;
    return -2;
  }

  bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) override {
    const std::span<const uint8_t> in(c.in);
    size_t pos = 0;
    if (!magic) {
      if (in.size() < 4) return true;
      if (!is_mvd2(in)) {
        error = "it does not open with the multiview recording's magic (MVD2)";
        return false;
      }
      magic = true;
      pos = 4;
    }
    while (!stopped && pos + 2 <= in.size()) {
      const size_t len = in[pos] | (size_t(in[pos + 1]) << 8);
      if (len == 0) {
        stopped = true;
        pos += 2;
        break;
      }
      if (pos + 2 + len > in.size()) break;
      Mvd2Block b;
      if (!r.block(in.subspan(pos + 2, len), b, error)) {
        std::string why = "the multiview recording does not read: ";
        why += error;
        why += " (in the block at byte ";
        why += std::to_string(pos);
        why += "). Try downloading it again.";
        error = std::move(why);
        return false;
      }
      pos += 2 + len;
      if (b.level) signon = true;
      if (!b.frame) continue;
      // The view's player: the one asked for, or the lowest playing once anyone plays.
      int want = c.options.pov;
      if (want == -1) want = pov >= 0 && !b.level ? pov : lowest_playing();
      if (want >= r.max_clients) want = -2;
      if (want != pov) {
        pov = want;
        signon = true;
      }
      if (pov < 0) continue;
      // A view starts once its player is in the game; after that he may leave and come back.
      if (signon && !r.now.players[size_t(pov)].in) continue;
      blocks.clear();
      const bool fresh = signon;
      if (signon) write_signon(c);
      write_frame(c, b, fresh);
      for (const std::vector<uint8_t>& block : blocks) {
        Out o{out};
        o.i32(static_cast<int64_t>(block.size()));
        o.bytes(block);
      }
    }
    c.in.erase(c.in.begin(), c.in.begin() + static_cast<std::ptrdiff_t>(pos));
    if ((c.ended || stopped) && !end_written) {
      if (!stopped && !c.in.empty()) c.notes.dropped["a block cut short at the end"]++;
      if (!c.options.no_end) {
        Out o{out};
        o.i32(-1);
      }
      end_written = true;
      c.in.clear();
    }
    return true;
  }
};

}  // namespace

std::unique_ptr<ConvertStream> mvd2_to_q2_stream() { return std::make_unique<Mvd2ToQ2>(); }

bool mvd2_reader_level(const DemoConvert& c, Mvd2Level& out) {
  const auto* view = dynamic_cast<const Mvd2ToQ2*>(c.stream.get());
  if (view == nullptr || !view->r.have_level) return false;
  out = view->r.level;
  out.base = view->r.now;
  out.base.config_strings.clear();
  out.base.prints.clear();
  out.base.unicasts.clear();
  out.base.multicasts.clear();
  out.base.sounds.clear();
  return true;
}

bool mvd2_view(std::span<const uint8_t> bytes, int slot, std::vector<uint8_t>& out,
               std::string& error, bool player_stats) {
  DemoConvert c;
  ConvertOptions o;
  o.pov = slot;
  o.player_stats = player_stats;
  if (!convert_begin(c, DemoFormat::q2_multiview, DemoFormat::q2, o, error)) return false;
  convert_feed(c, bytes);
  convert_end(c);
  out.clear();
  if (!convert_take(c, out, error)) return false;
  if (c.notes.frames == 0) {
    error = slot >= 0 ? "player " + std::to_string(slot) + " is not in the recording's game"
                      : "no one plays in the recording";
    return false;
  }
  return true;
}

bool mvd2_follow(Demo& d, int slot, std::string& error) {
  if (d.multiview_source.empty()) {
    error = "the demo is not a multiview recording of Quake II";
    return false;
  }
  std::vector<uint8_t> view;
  if (!mvd2_view(d.multiview_source, slot, view, error, true)) return false;
  Demo next;
  if (!open_demo(view, next, error)) return false;
  const int64_t frames = d.state.frames;
  while (next.state.frames < frames) {
    const DemoRead r = next_frame(next, error);
    if (r == DemoRead::error) return false;
    if (r == DemoRead::end) break;
  }
  next.multiview_source = std::move(d.multiview_source);
  next.layers = std::move(d.layers);
  next.inner_name = std::move(d.inner_name);
  next.track = slot;
  d = std::move(next);
  return true;
}

}  // namespace shooters_edge::client