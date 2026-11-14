// The sequel's client recording read frame by frame (demo.hpp § SEQUEL): its blocks walked, its
// messages read into the same `DemoState` the other formats fill, so the playback, the seek, the
// highlights, the lerp and the video export play it unchanged.
//
// What is kept: the precache lists and the map off the settings strings, the players' names off
// their skins' strings, every entity's origin, angles, model, frame, skin and effects (players by
// slot, as the other multiplayer formats keep them), the recording player's eye, view angles,
// view model and the stats the status bar shows (health, ammunition, armour into the shared
// table), the sounds started, the prints and centre messages, and the temporary entities the
// window draws (hits, blood, explosions, trails). The rest of each message is read past.
//
// Protocols 26 to 34, and the extended recordings 3434 to 3436 a server with the extended limits
// writes: longer settings lists, entity numbers to 8191, models and sounds by a word, alpha and
// scale, int23 points and 64 stats (3435 on), fog (3436), blocks to 32 KB.
//
// ge:summary The sequel's .dm2 client recording read frame by frame into the demo state
// ge:keywords sequel dm2 demo playback frame playerinfo packetentities configstring baseline
// ge:keywords temp_entity sound print centerprint next_sequel is_sequel_demo delta ring
// ge:keywords extended-limits protocol-3436 int23 varint fog alpha scale
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_sequel_test.cpp
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace {

// The message ids.
enum : uint8_t {
  kBad = 0,
  kMuzzleFlash = 1,
  kMuzzleFlash2 = 2,
  kTempEntity = 3,
  kLayout = 4,
  kInventory = 5,
  kNop = 6,
  kDisconnect = 7,
  kReconnect = 8,
  kSound = 9,
  kPrint = 10,
  kStuffText = 11,
  kServerData = 12,
  kConfigString = 13,
  kSpawnBaseline = 14,
  kCenterPrint = 15,
  kDownload = 16,
  kPlayerInfo = 17,
  kPacketEntities = 18,
  kDeltaPacketEntities = 19,
  kFrame = 20,
};

// The settings strings' ranges: protocol 34's, and the extended limits' (3434 to 3436), whose
// lists are longer and start further on.
struct CsLayout {
  int max_clients, models, sounds, images, skins, general, max;
};
constexpr CsLayout kClassicCs{30, 32, 288, 544, 1312, 1568, 2080};
constexpr CsLayout kExtendedCs{60, 62, 8254, 10302, 12862, 13118, 13630};
constexpr int kMostEntities = 1024, kMostEntitiesExt = 8192;
constexpr int kMostBlock = 1400;      // the protocol's largest message
constexpr int kMostBlockExt = 32768;  // an extended recording's

// The extended recording's level: 0 protocol 34 and older, 1 3434 (wider fields), 2 3435 (int23
// points, 64 stats), 3 3436 (fog).
int ext_of(int protocol) { return protocol >= 3434 && protocol <= 3436 ? protocol - 3433 : 0; }
const CsLayout& layout_of(int protocol) { return ext_of(protocol) > 0 ? kExtendedCs : kClassicCs; }
int most_entities(int protocol) { return ext_of(protocol) > 0 ? kMostEntitiesExt : kMostEntities; }
constexpr int kDirections = 162;  // the direction table's entries

// The entity header's flag bits.
enum : uint32_t {
  kOriginX = 1u << 0,
  kOriginY = 1u << 1,
  kAngle2 = 1u << 2,
  kAngle3 = 1u << 3,
  kFrame8 = 1u << 4,
  kEvent = 1u << 5,
  kRemove = 1u << 6,
  kMore1 = 1u << 7,
  kNumber16 = 1u << 8,
  kOriginZ = 1u << 9,
  kAngle1 = 1u << 10,
  kModel = 1u << 11,
  kRenderFxLow = 1u << 12,
  kEffectsLow = 1u << 14,
  kMore2 = 1u << 15,
  kSkinLow = 1u << 16,
  kFrame16 = 1u << 17,
  kRenderFxHigh = 1u << 18,
  kEffectsHigh = 1u << 19,
  kModel2 = 1u << 20,
  kModel3 = 1u << 21,
  kModel4 = 1u << 22,
  kMore3 = 1u << 23,
  kOldOrigin = 1u << 24,
  kSkinHigh = 1u << 25,
  kSoundBit = 1u << 26,
  kSolid = 1u << 27,
};

// The player state's flag bits.
enum : uint16_t {
  kPmType = 1 << 0,
  kPmOrigin = 1 << 1,
  kPmVelocity = 1 << 2,
  kPmTime = 1 << 3,
  kPmFlags = 1 << 4,
  kPmGravity = 1 << 5,
  kPmDelta = 1 << 6,
  kViewOffset = 1 << 7,
  kViewAngles = 1 << 8,
  kKick = 1 << 9,
  kBlend = 1 << 10,
  kFov = 1 << 11,
  kGunModel = 1 << 12,
  kGunFrame = 1 << 13,
  kRdFlags = 1 << 14,
};

// The stats the status bar's program draws as its big numbers.
constexpr int kSeqHealth = 1, kSeqAmmo = 3, kSeqArmour = 5;

using Frame = std::vector<std::pair<int, DemoEntity>>;

// An int23 coordinate in 1/8 unit: a word whose bit 0 clear makes bits 1 to 15 a delta from
// `base`; set, a byte more and bits 1 to 23 are the value itself.
int32_t int23(DemoMsg& m, int32_t base) {
  const uint32_t w = static_cast<uint32_t>(m.u16());
  if ((w & 1u) == 0) {
    int32_t d = static_cast<int32_t>(w >> 1);
    if (d >= 1 << 14) d -= 1 << 15;
    return base + d;
  }
  int32_t v = static_cast<int32_t>((w | static_cast<uint32_t>(m.u8()) << 16) >> 1);
  if (v >= 1 << 22) v -= 1 << 23;
  return v;
}
float coord(DemoMsg& m) { return static_cast<float>(m.int23 ? int23(m, 0) : m.i16()) * 0.125f; }
sh::Vec3f pos(DemoMsg& m) {
  const float x = coord(m), y = coord(m), z = coord(m);
  return {x, y, z};
}
// An entity's origin coordinate: a short, or on 3435 on an int23 against its base's (the base
// is a whole number of eighths, exact in a float under 2^24 eighths, so the cast is exact).
float origin_coord(DemoMsg& m, float base) {
  if (!m.int23) return static_cast<float>(m.i16()) * 0.125f;
  return static_cast<float>(int23(m, static_cast<int32_t>(base * 8.0f))) * 0.125f;
}
float angle8(DemoMsg& m) { return static_cast<float>(m.i8()) * (360.0f / 256.0f); }
float angle16(int16_t a) { return static_cast<float>(a) * (360.0f / 65536.0f); }

// The extended header's bits (3434 on): bit 31 a fifth byte (bits 32 to 39).
constexpr uint64_t kMore4 = 1ull << 31, kModel16 = 1ull << 28, kMoreFxLow = 1ull << 29,
                   kAlpha = 1ull << 30, kScale = 1ull << 32, kMoreFxHigh = 1ull << 33;

// The entity header: its flags and number. `ext` (3434 on) reads a fifth flags byte.
uint64_t entity_bits(DemoMsg& m, int& number, int ext) {
  uint64_t bits = static_cast<uint32_t>(m.u8());
  if ((bits & kMore1) != 0) bits |= static_cast<uint64_t>(m.u8()) << 8;
  if ((bits & kMore2) != 0) bits |= static_cast<uint64_t>(m.u8()) << 16;
  if ((bits & kMore3) != 0) bits |= static_cast<uint64_t>(m.u8()) << 24;
  if (ext > 0 && (bits & kMore4) != 0) bits |= static_cast<uint64_t>(m.u8()) << 32;
  number = (bits & kNumber16) != 0 ? m.u16() : m.u8();
  return bits;
}

// A size picked by two flags of the header: low alone a byte, high alone a short, both a long.
int32_t sized64(DemoMsg& m, uint64_t bits, uint64_t low, uint64_t high) {
  if ((bits & low) != 0 && (bits & high) != 0) return m.i32();
  if ((bits & low) != 0) return m.u8();
  if ((bits & high) != 0) return m.i16();
  return 0;
}

// The entity's fields after its number, onto `e` (its base). `ext` as `ext_of`: model indices
// shorts under bit 28, the sound a word, the solid a long, then more effects, alpha and scale.
void entity_delta(DemoMsg& m, uint64_t bits, DemoEntity& e, int ext) {
  const bool wide_model = ext > 0 && (bits & kModel16) != 0;
  const auto model = [&] { return wide_model ? m.u16() : m.u8(); };
  if ((bits & kModel) != 0) e.model = model();
  if ((bits & kModel2) != 0) model();
  if ((bits & kModel3) != 0) model();
  if ((bits & kModel4) != 0) model();
  if ((bits & kFrame8) != 0) e.frame = m.u8();
  if ((bits & kFrame16) != 0) e.frame = m.i16();
  if ((bits & (kSkinLow | kSkinHigh)) != 0) e.skin = sized64(m, bits, kSkinLow, kSkinHigh);
  if ((bits & (kEffectsLow | kEffectsHigh)) != 0)
    e.effects = sized64(m, bits, kEffectsLow, kEffectsHigh);
  if ((bits & (kRenderFxLow | kRenderFxHigh)) != 0) sized64(m, bits, kRenderFxLow, kRenderFxHigh);
  if ((bits & kOriginX) != 0) e.origin.x = origin_coord(m, e.origin.x);
  if ((bits & kOriginY) != 0) e.origin.y = origin_coord(m, e.origin.y);
  if ((bits & kOriginZ) != 0) e.origin.z = origin_coord(m, e.origin.z);
  if ((bits & kAngle1) != 0) e.angles.x = angle8(m);
  if ((bits & kAngle2) != 0) e.angles.y = angle8(m);
  if ((bits & kAngle3) != 0) e.angles.z = angle8(m);
  if ((bits & kOldOrigin) != 0) pos(m);
  if ((bits & kSoundBit) != 0) {
    if (ext > 0) {  // the index in bits 0 to 13; a loop volume (bit 14), a loop attenuation (15)
      const int w = m.u16();
      if ((w & 0x4000) != 0) m.u8();
      if ((w & 0x8000) != 0) m.u8();
    } else {
      m.u8();
    }
  }
  if ((bits & kEvent) != 0) m.u8();
  if ((bits & kSolid) != 0) {
    if (ext > 0)
      m.i32();
    else
      m.i16();
  }
  if (ext == 0) return;
  if ((bits & (kMoreFxLow | kMoreFxHigh)) != 0) sized64(m, bits, kMoreFxLow, kMoreFxHigh);
  if ((bits & kAlpha) != 0) e.alpha = static_cast<float>(m.u8()) / 255.0f;
  if ((bits & kScale) != 0) e.scale = static_cast<float>(m.u8()) / 16.0f;
}

std::string at_byte(const DemoMsg& m) { return " at byte " + std::to_string(m.at); }

bool bad_number(int n, int most, const DemoMsg& m, const char* what, std::string& error) {
  if (n >= 0 && n < most) return false;
  error = std::string("the ") + what + " names entity " + std::to_string(n) + at_byte(m) +
          ", past the game's " + std::to_string(most - 1) + ": the recording is damaged";
  return true;
}

void set_string(Demo& d, int index, std::string text) {
  DemoState& s = d.state;
  const CsLayout& cs = layout_of(s.protocol);
  const auto put = [](std::vector<std::string>& list, int i, std::string v) {
    if (static_cast<size_t>(i) >= list.size()) list.resize(static_cast<size_t>(i) + 1);
    list[static_cast<size_t>(i)] = std::move(v);
  };
  if (index == cs.max_clients) {
    // Entities 1 to this are the players: kept by slot, so the rest number past `players`.
    d.max_clients = std::clamp(std::atoi(text.c_str()), 1, 32);
    s.players.assign(static_cast<size_t>(d.max_clients), DemoEntity{});
  } else if (index >= cs.models && index < cs.sounds) {
    // The world is model 1; a recording that puts it elsewhere is read by the first map named.
    const std::string map = map_of_model(text);
    if (!map.empty() && (index == cs.models + 1 || s.map.empty())) s.map = map;
    put(s.models, index - cs.models, std::move(text));
  } else if (index >= cs.sounds && index < cs.images) {
    put(s.sounds, index - cs.sounds, std::move(text));
  } else if (index >= cs.images && index < cs.images + (cs.sounds - cs.models)) {
    put(s.images, index - cs.images, std::move(text));
  } else if (index >= cs.skins && index < cs.skins + 32) {
    // "<name>\<model>/<skin>": the name, then the look.
    const size_t cut = text.find('\\');
    const size_t slot = static_cast<size_t>(index - cs.skins);
    s.names[slot] = text.substr(0, cut);
    s.looks[slot] = cut == std::string::npos ? "" : text.substr(cut + 1);
  }
}

void server_data(Demo& d, DemoMsg& m) {
  DemoState& s = d.state;
  s.protocol = m.i32();
  d.server_count = m.i32();
  m.u8();  // the recording's kind
  s.game = m.str();
  s.player = std::max(0, m.i16());
  m.str();  // the level's full name
  s.levels++;
  s.places = {};
  s.map.clear();
  s.models.assign(1, "");
  s.sounds.assign(1, "");
  s.images.assign(1, "");
  s.intermission = false;
  s.entities.clear();
  s.statics.clear();
  s.static_sounds.clear();
  d.baselines.assign(static_cast<size_t>(most_entities(s.protocol)), DemoEntity{});
  for (Frame& f : d.ring) f.clear();
  d.views.fill(SequelView{});
  d.framed = false;
  d.last_frame = -1;
  // Until the server says how many it takes: the recording's own player alone, as a game for one
  // player says nothing.
  s.players.assign(static_cast<size_t>(std::min(s.player + 1, 32)), DemoEntity{});
  d.max_clients = 0;
  s.frags.fill(0);
  for (std::string& n : s.names) n.clear();
  for (std::string& n : s.looks) n.clear();
}

// A base-128 varint, least significant group first (3435's stats mask).
uint64_t varint(DemoMsg& m) {
  uint64_t v = 0;
  for (int shift = 0; shift < 64 && !m.bad; shift += 7) {
    const int b = m.u8();
    v |= static_cast<uint64_t>(b & 127) << shift;
    if ((b & 128) == 0) break;
  }
  return v;
}

// The player's state. `ext` as `ext_of`: the gun model a short (3434 on); int23 origin and
// velocity, words of movement time and flags, a blend by channel and 64 stats under a varint mask
// (3435 on); fog under a third flags byte (3436).
void player_info(DemoMsg& m, SequelView& v, int ext) {
  int bits = m.u16();
  if (ext >= 3 && (bits & 0x8000) != 0) bits |= m.u8() << 16;
  const auto point = [&](int32_t& c) { c = ext >= 2 ? int23(m, c) : m.i16(); };
  if ((bits & kPmType) != 0) v.pm_type = m.u8();
  if ((bits & kPmOrigin) != 0)
    for (int32_t& c : v.origin) point(c);
  if ((bits & kPmVelocity) != 0)
    for (int32_t& c : v.velocity) point(c);
  if ((bits & kPmTime) != 0) v.pm_time = ext >= 2 ? m.u16() : m.u8();
  if ((bits & kPmFlags) != 0) v.pm_flags = ext >= 2 ? m.u16() : m.u8();
  if ((bits & kPmGravity) != 0) v.gravity = m.i16();
  if ((bits & kPmDelta) != 0)
    for (int16_t& c : v.delta) c = static_cast<int16_t>(m.i16());
  if ((bits & kViewOffset) != 0)
    for (int8_t& c : v.offset) c = static_cast<int8_t>(m.i8());
  if ((bits & kViewAngles) != 0)
    for (int16_t& c : v.angles) c = static_cast<int16_t>(m.i16());
  if ((bits & kKick) != 0)
    for (int8_t& c : v.kick) c = static_cast<int8_t>(m.i8());
  if ((bits & kGunModel) != 0) v.gun_model = ext >= 1 ? m.u16() : m.u8();
  if ((bits & kGunFrame) != 0) {
    v.gun_frame = m.u8();
    for (int8_t& c : v.gun_offset) c = static_cast<int8_t>(m.i8());
    for (int8_t& c : v.gun_angles) c = static_cast<int8_t>(m.i8());
  }
  if ((bits & kBlend) != 0) {
    if (ext >= 2) {  // a flag byte: bits 0 to 3 the blend's channels, 4 to 7 the damage blend's
      const int which = m.u8();
      for (int i = 0; i < 4; i++)
        if ((which >> i & 1) != 0) v.blend[i] = static_cast<uint8_t>(m.u8());
      for (int i = 4; i < 8; i++)
        if ((which >> i & 1) != 0) m.u8();
    } else {
      for (uint8_t& c : v.blend) c = static_cast<uint8_t>(m.u8());
    }
  }
  if ((bits & (1 << 16)) != 0) {  // fog: a flag byte, then the parts it names
    const int f = m.u8();
    if ((f & 1) != 0) m.skip(3);      // colour
    if ((f & 2) != 0) m.skip(4);      // density, sky factor
    if ((f & 4) != 0) m.skip(2);      // height density
    if ((f & 8) != 0) m.skip(2);      // height falloff
    if ((f & 16) != 0) m.skip(3);     // height start colour
    if ((f & 32) != 0) m.skip(3);     // height end colour
    if ((f & 64) != 0) int23(m, 0);   // height start distance
    if ((f & 128) != 0) int23(m, 0);  // height end distance
  }
  if ((bits & kFov) != 0) v.fov = m.u8();
  if ((bits & kRdFlags) != 0) v.rdflags = m.u8();
  const uint64_t mask = ext >= 2 ? varint(m) : static_cast<uint32_t>(m.i32());
  for (int i = 0; i < (ext >= 2 ? 64 : 32); i++)
    if ((mask >> i & 1u) != 0) v.stats[i] = static_cast<int16_t>(m.i16());
}

// The entity list, merged with `from` (the delta frame's, or none) into `to`.
bool packet_entities(Demo& d, DemoMsg& m, const Frame* from, Frame& to, std::string& error) {
  to.clear();
  size_t old = 0;
  const int ext = ext_of(d.state.protocol);
  for (;;) {
    int n = 0;
    const uint64_t bits = entity_bits(m, n, ext);
    if (m.bad) {
      error = "the entity list runs past its block's end" + at_byte(m) +
              ": the recording is cut or damaged";
      return false;
    }
    if (bad_number(n, most_entities(d.state.protocol), m, "entity list", error)) return false;
    for (; from != nullptr && old < from->size() && (n == 0 || (*from)[old].first < n); old++)
      to.push_back((*from)[old]);
    if (n == 0) break;
    DemoEntity e = d.baselines[static_cast<size_t>(n)];
    if (from != nullptr && old < from->size() && (*from)[old].first == n) e = (*from)[old++].second;
    if ((bits & kRemove) != 0) continue;
    entity_delta(m, bits, e, ext);
    to.emplace_back(n, e);
  }
  return true;
}

// The frame: its header, the player's state and the entity list.
bool frame(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  const int32_t number = m.i32();
  const int32_t delta = m.i32();
  if (s.protocol != 26) m.u8();         // the packets held back
  m.skip(static_cast<size_t>(m.u8()));  // the areas seen
  if (m.u8() != kPlayerInfo) {
    error = "the frame" + at_byte(m) +
            " is not followed by the player's state: the recording is damaged";
    return false;
  }
  const size_t slot = static_cast<size_t>(number) & 15;
  const size_t base_slot = static_cast<size_t>(delta) & 15;
  const bool have_base = delta > 0 && d.views[base_slot].frame == delta;
  SequelView v = have_base ? d.views[base_slot] : SequelView{};
  player_info(m, v, ext_of(s.protocol));
  v.frame = number;
  if (m.u8() != kPacketEntities) {
    error =
        "the frame" + at_byte(m) + " is not followed by its entity list: the recording is damaged";
    return false;
  }
  Frame to;
  if (!packet_entities(d, m, have_base ? &d.ring[base_slot] : nullptr, to, error)) return false;
  d.views[slot] = v;
  d.ring[slot] = to;
  d.framed = true;
  d.last_frame = have_base || delta <= 0 ? number : -1;

  // Into the state the window reads.
  std::vector<DemoEntity>& ents = s.entities;
  for (DemoEntity& e : ents) e.present = false;
  for (auto& [n, e] : to) {
    e.present = true;
    if (static_cast<size_t>(n) >= ents.size()) ents.resize(static_cast<size_t>(n) + 1);
    ents[static_cast<size_t>(n)] = e;
  }
  for (size_t k = 0; k < s.players.size(); k++) {
    DemoEntity& p = s.players[k];
    const size_t n = k + 1;
    p = n < ents.size() ? ents[n] : DemoEntity{};
  }
  s.time = static_cast<double>(number) * 0.1;
  s.view_height = static_cast<float>(v.offset[2]) * 0.25f;
  s.angles = {angle16(v.angles[0]), angle16(v.angles[1]), angle16(v.angles[2])};
  s.kick = {static_cast<float>(v.kick[0]) * 0.25f, static_cast<float>(v.kick[1]) * 0.25f,
            static_cast<float>(v.kick[2]) * 0.25f};
  if (static_cast<size_t>(s.player) < s.players.size()) {
    DemoEntity& me = s.players[static_cast<size_t>(s.player)];
    me.present = true;
    me.origin = {static_cast<float>(v.origin[0]) * 0.125f, static_cast<float>(v.origin[1]) * 0.125f,
                 static_cast<float>(v.origin[2]) * 0.125f};
    me.angles = s.angles;
    me.weapon_frame = v.gun_frame;
    s.eye = me.origin;
    s.eye.x += static_cast<float>(v.offset[0]) * 0.25f;
    s.eye.y += static_cast<float>(v.offset[1]) * 0.25f;
    s.eye.z += s.view_height;
  }
  std::fill(std::begin(s.stats), std::end(s.stats), 0);
  s.stats[kStatHealth] = v.stats[kSeqHealth];
  s.stats[kStatAmmo] = v.stats[kSeqAmmo];
  s.stats[kStatArmour] = v.stats[kSeqArmour];
  s.stats[kStatWeapon] = v.gun_model;
  return true;
}

void sound(DemoState& s, DemoMsg& m) {
  const int flags = m.u8();
  DemoSound o;
  // An extended recording's index is a word under flag 32.
  o.sound = ext_of(s.protocol) > 0 && (flags & 32) != 0 ? m.u16() : m.u8();
  if ((flags & 1) != 0) o.volume = byte_volume(m.u8());
  if ((flags & 2) != 0) o.attenuation = byte_attenuation(m.u8());
  if ((flags & 16) != 0) m.u8();  // the offset in the frame, ms
  if ((flags & 8) != 0) {
    const int v = m.u16();
    o.entity = v >> 3;
    o.channel = v & 7;
  }
  if ((flags & 4) != 0) {
    o.origin = pos(m);
  } else if (o.entity > 0 && static_cast<size_t>(o.entity) < s.entities.size()) {
    o.origin = s.entities[static_cast<size_t>(o.entity)].origin;
  } else {
    o.origin = s.eye;  // the listener's own
  }
  s.started.push_back(o);
}

bool direction(DemoMsg& m, std::string& error) {
  const int i = m.u8();
  if (i < kDirections) return true;
  error = "a direction " + std::to_string(i) + at_byte(m) + " is past the table's " +
          std::to_string(kDirections - 1) + ": the recording is damaged";
  return false;
}

// A temporary entity: its payload read by its kind, and the ones the window draws kept.
bool temp_entity(DemoState& s, DemoMsg& m, std::string& error) {
  const int kind = m.u8();
  DemoEffect e;
  e.code = kind;
  bool keep = true;
  const auto point_dir = [&](DemoEffectKind k) {
    e.kind = k;
    e.at = e.end = pos(m);
    return direction(m, error);
  };
  const auto point = [&](DemoEffectKind k) {
    e.kind = k;
    e.at = e.end = pos(m);
  };
  const auto line = [&](DemoEffectKind k) {
    e.kind = k;
    e.at = pos(m);
    e.end = pos(m);
  };
  switch (kind) {
    case 0:   // gunshot
    case 4:   // shotgun
    case 14:  // bullet sparks
      if (!point_dir(DemoEffectKind::gunshot)) return false;
      break;
    case 1:   // blood
    case 26:  // green blood
    case 42:  // more blood
      if (!point_dir(DemoEffectKind::blood)) return false;
      break;
    case 2:
    case 9:
    case 12:
    case 13:
    case 30:
    case 43:
    case 44:
    case 46:
    case 55:
      if (!point_dir(DemoEffectKind::spike)) return false;
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 17:
    case 18:
    case 20:
    case 21:
    case 28:
    case 35:
    case 47:
    case 51:
    case 53:
    case 54:
      point(DemoEffectKind::explosion);
      break;
    case 22:
    case 48:  // teleports
      point(DemoEffectKind::teleport);
      break;
    case 45:
    case 49:
    case 52:
      point(DemoEffectKind::spike);
      keep = false;
      break;
    case 3:
    case 23:
    case 34:  // the rail's line, a laser, a trail
      line(DemoEffectKind::beam);
      break;
    case 11:
    case 41:  // bubbles
    case 27:  // a point and a direction sent as a point
      line(DemoEffectKind::beam);
      keep = false;
      break;
    case 10:
    case 15:
    case 25:
    case 29:  // count, point, direction, colour
      m.u8();
      if (!point_dir(DemoEffectKind::spike)) return false;
      m.u8();
      keep = false;
      break;
    case 16:
    case 19:
    case 38:
    case 39:  // entity, start, end
      e.entity = m.i16();
      line(DemoEffectKind::beam);
      break;
    case 24:  // the grapple: entity, start, end, offset
      e.entity = m.i16();
      line(DemoEffectKind::beam);
      pos(m);
      break;
    case 33:  // lightning: two entities, start, end
      e.entity = m.i16();
      m.i16();
      line(DemoEffectKind::beam);
      break;
    case 36:  // a flashlight: point, entity
      point(DemoEffectKind::spike);
      m.i16();
      keep = false;
      break;
    case 37:  // a force wall: start, end, colour
      line(DemoEffectKind::beam);
      m.u8();
      keep = false;
      break;
    case 40: {  // steam: id, count, point, direction, colour, magnitude, and a time with an id
      const int id = m.i16();
      m.u8();
      if (!point_dir(DemoEffectKind::spike)) return false;
      m.u8();
      m.i16();
      if (id != -1) m.i32();
      keep = false;
      break;
    }
    case 50:  // an id and a point
      m.i16();
      point(DemoEffectKind::spike);
      keep = false;
      break;
    // The kinds a newer game's server sends, in any protocol's recording.
    case 31:  // a lighter rail's line
    case 57:  // a bolt's zap
      line(DemoEffectKind::beam);
      break;
    case 56:  // a blue bolt's hit
    case 58:  // a slam on the ground
      if (!point_dir(DemoEffectKind::spike)) return false;
      break;
    case 59:  // a cable or a beam: entity, start, end
    case 61:
      e.entity = m.i16();
      line(DemoEffectKind::beam);
      break;
    case 60:  // a power splash: entity, size
      m.i16();
      m.u8();
      keep = false;
      break;
    case 62:
    case 63:  // explosions that give no light
      point(DemoEffectKind::explosion);
      break;
    case 128:  // damage dealt: a number for the screen alone
      m.i16();
      keep = false;
      break;
    default:
      error = "a temporary entity of kind " + std::to_string(kind) + at_byte(m) +
              " that this game's recordings do not carry: the recording is damaged";
      return false;
  }
  if (keep && !m.bad) s.effects.push_back(e);
  return true;
}

// One block's messages into the state.
bool read_block(Demo& d, DemoMsg& m, bool& framed, std::string& error) {
  DemoState& s = d.state;
  if (d.spans != nullptr) d.spans->clear();
  while (!m.done()) {
    const size_t at = m.at;
    m.int23 = ext_of(s.protocol) >= 2;  // set again after a server data
    const int id = m.u8();
    switch (id) {
      case kMuzzleFlash:
      case kMuzzleFlash2:
        m.skip(3);
        break;
      case kTempEntity:
        if (!temp_entity(s, m, error)) return false;
        break;
      case kLayout:
        m.str();
        break;
      case kStuffText: {  // a place name the recording kept read; the rest kept for the caller
        std::string text = m.str();
        if (sh::read_recorded_loc(text, s.places)) break;
        if (d.stuffed.size() < kSequelStuffedKept) d.stuffed.push_back(std::move(text));
        break;
      }
      case kInventory:
        m.skip(512);
        break;
      case kNop:
        break;
      case kReconnect:
        d.reconnect = true;
        break;
      case kDisconnect:
        d.ended = true;
        return true;
      case kSound:
        sound(s, m);
        break;
      case kPrint: {
        DemoPrint p;
        p.level = m.u8();
        p.text = m.str();
        s.prints.push_back(std::move(p));
        break;
      }
      case kCenterPrint:
        s.centre = m.str();
        break;
      case kServerData:
        server_data(d, m);
        break;
      case kConfigString: {
        const int index = m.i16();
        std::string text = m.str();
        const int most = layout_of(s.protocol).max;
        if (index < 0 || index >= most) {
          error = "a settings string numbered " + std::to_string(index) + at_byte(m) +
                  ", past the game's " + std::to_string(most - 1) + ": the recording is damaged";
          return false;
        }
        set_string(d, index, std::move(text));
        break;
      }
      case kSpawnBaseline: {
        int n = 0;
        const int ext = ext_of(s.protocol);
        const uint64_t bits = entity_bits(m, n, ext);
        const int most = most_entities(s.protocol);
        if (bad_number(n, most, m, "baseline", error)) return false;
        if (d.baselines.size() < static_cast<size_t>(most))
          d.baselines.resize(static_cast<size_t>(most));
        DemoEntity& e = d.baselines[static_cast<size_t>(n)];
        e = DemoEntity{};
        entity_delta(m, bits, e, ext);
        break;
      }
      case kDownload: {
        const int size = m.i16();
        m.u8();
        if (size > 0) m.skip(static_cast<size_t>(size));
        break;
      }
      case kFrame:
        if (!frame(d, m, error)) return false;
        framed = true;
        break;
      default:
        error = "a message " + std::to_string(id) + " at byte " + std::to_string(at) +
                (id == kBad || id > kFrame ? " that this game's recordings do not have"
                 : id == kDeltaPacketEntities || id == kPlayerInfo || id == kPacketEntities
                     ? " outside a frame"
                     : "") +
                ": the recording is damaged";
        return false;
    }
    if (m.bad) {
      error = "the message " + std::to_string(id) + " at byte " + std::to_string(at) +
              " runs past its block's end: the recording is cut or damaged";
      return false;
    }
    if (d.spans != nullptr)
      d.spans->push_back(
          {static_cast<uint8_t>(id), static_cast<uint32_t>(at), static_cast<uint32_t>(m.at)});
  }
  return true;
}

int32_t i32_at(std::span<const uint8_t> b, size_t at) {
  uint32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);  // little-endian on every platform the game runs on
  return static_cast<int32_t>(v);
}

}  // namespace

void sequel_stream_begin(Demo& d) {
  d.state.format = DemoFormat::sequel;
  d.state.protocol = 34;
  d.ring.resize(16);
  d.baselines.resize(kMostEntitiesExt);
}

bool is_sequel_demo(std::span<const uint8_t> bytes) {
  if (bytes.size() < 13) return false;
  const int32_t len = i32_at(bytes, 0);
  if (len < 9 || bytes[4] != kServerData) return false;
  const int32_t protocol = i32_at(bytes, 5);
  const bool extended = ext_of(protocol) > 0;
  if (len > (extended ? kMostBlockExt : kMostBlock)) return false;
  return (extended || (protocol >= 26 && protocol <= 34)) && bytes[13] <= 2;
}

DemoRead next_sequel(Demo& d, std::string& error) {
  const std::span<const uint8_t> b(d.bytes);
  for (;;) {
    if (d.pos + 4 > b.size()) return DemoRead::end;
    const size_t block = d.pos;
    const int32_t len = i32_at(b, block);
    if (len == -1) {
      d.ended = true;
      return DemoRead::end;
    }
    // An extended recording's blocks run to 32768 bytes (its first, the server data, says so).
    const bool extended =
        ext_of(d.state.protocol) > 0 ||
        (block + 9 <= b.size() && b[block + 4] == kServerData && ext_of(i32_at(b, block + 5)) > 0);
    const int32_t most = extended ? kMostBlockExt : kMostBlock;
    if (len <= 0 || len > most) {
      error = "the block length " + std::to_string(len) + " at byte " + std::to_string(block) +
              " is not a block's (1 to " + std::to_string(most) + "): the recording is damaged";
      return DemoRead::error;
    }
    if (block + 4 + static_cast<size_t>(len) > b.size()) return DemoRead::end;  // cut short
    d.pos = block + 4 + static_cast<size_t>(len);
    DemoMsg m{b.subspan(block + 4, static_cast<size_t>(len))};
    bool framed = false;
    if (!read_block(d, m, framed, error)) {
      error += " (in the block at byte " + std::to_string(block) + ")";
      return DemoRead::error;
    }
    if (framed) {
      d.state.frames++;
      return d.ended ? DemoRead::end : DemoRead::frame;
    }
    if (d.ended) return DemoRead::end;
  }
}

}  // namespace shooters_edge::client
