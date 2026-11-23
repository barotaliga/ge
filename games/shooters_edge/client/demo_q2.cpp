// Quake II's client recording read frame by frame (demo.hpp § QUAKE II): its blocks walked, its
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
// ge:summary Quake II's .dm2 client recording read frame by frame into the demo state
// ge:keywords q2 dm2 demo playback frame playerinfo packetentities configstring baseline
// ge:keywords temp_entity sound print centerprint next_q2 is_q2_demo delta ring
// ge:keywords extended-limits protocol-3436 int23 varint fog alpha scale
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_q2_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "classic/protocol/directions.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/q2_flash_fx.hpp"
#include "shooters_edge/client/q2_temp_fx.hpp"

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
  kGeVoice = 121,       // the game's own: a voice (net/qw_voice_route.hpp)
  kGeFeel = 122,        // the game's own: the move's feel (net/q2_net_server.cpp)
  kGeOrigin = 123,      // the game's own, only to a client that agreed it (q2_net_server.hpp)
  kGeMoves = 124,       // the game's own too: the other players' moves
  kGeStats = 125,       // and every player's state, in a multiview recording's view (demo_mvd2.hpp)
  kGeShare = 126,       // and the easy jump's share (net/q2_net_server.hpp)
  kGeAccountKey = 127,  // and the tag of its logins' key
  kGeBack = 119,        // and an item taken and when it comes back
  kGeSheet = 120,       // and a part of the settings sheet (net/q2_votes.hpp)
};

// The settings strings' ranges: protocol 34's, and the extended limits' (3434 to 3436), whose
// lists are longer and start further on.
struct CsLayout {
  int max_clients, models, sounds, images, skins, general, max;
};
constexpr CsLayout kClassicCs{30, 32, 288, 544, 1312, 1568, 2080};
constexpr CsLayout kExtendedCs{60, 62, 8254, 10302, 12862, 13118, 13630};
constexpr int kCsStatusBar = 5;  // the bar's program, in either layout (its items: skins - 256)
constexpr int kMostEntities = 1024, kMostEntitiesExt = 8192;
constexpr int kMostBlock = 1400;      // the protocol's largest message
constexpr int kMostBlockExt = 32768;  // an extended recording's

// The extended recording's level: 0 protocol 34 and older, 1 3434 (wider fields), 2 3435 (int23
// points, 64 stats), 3 3436 (fog).
int ext_of(int protocol) { return protocol >= 3434 && protocol <= 3436 ? protocol - 3433 : 0; }
const CsLayout& layout_of(int protocol) { return ext_of(protocol) > 0 ? kExtendedCs : kClassicCs; }
int most_entities(int protocol) { return ext_of(protocol) > 0 ? kMostEntitiesExt : kMostEntities; }
constexpr int kDirections = ge::classic::protocol::kDirections;

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
// The entity events (the event byte) and the sound channels they play on.
constexpr int kSqTeleporterBit = 131072;  // an entity's effect: the pad's fountain
constexpr int kEvItemRespawn = 1, kEvFootstep = 2, kEvFallShort = 3, kEvFall = 4, kEvFallFar = 5,
              kEvPlayerTeleport = 6;
constexpr int kChanAuto = 0, kChanWeapon = 1, kChanBody = 4;

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
// One entity's update over `e`; `event` (when given) its event byte, 0 when the update has none.
void entity_delta(DemoMsg& m, uint64_t bits, DemoEntity& e, int ext, int* event = nullptr) {
  const bool wide_model = ext > 0 && (bits & kModel16) != 0;
  const auto model = [&] { return wide_model ? m.u16() : m.u8(); };
  if ((bits & kModel) != 0) e.model = model();
  if ((bits & kModel2) != 0) e.more[0] = model();
  if ((bits & kModel3) != 0) e.more[1] = model();
  if ((bits & kModel4) != 0) e.more[2] = model();
  if ((bits & kFrame8) != 0) e.frame = m.u8();
  if ((bits & kFrame16) != 0) e.frame = m.i16();
  if ((bits & (kSkinLow | kSkinHigh)) != 0) e.skin = sized64(m, bits, kSkinLow, kSkinHigh);
  if ((bits & (kEffectsLow | kEffectsHigh)) != 0)
    e.effects = sized64(m, bits, kEffectsLow, kEffectsHigh);
  if ((bits & (kRenderFxLow | kRenderFxHigh)) != 0)
    e.render_fx = sized64(m, bits, kRenderFxLow, kRenderFxHigh);
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
      e.loop = w & 0x3fff;
      e.loop_volume = (w & 0x4000) != 0 ? m.u8() : -1;
      e.loop_attenuation = (w & 0x8000) != 0 ? m.u8() : -1;
    } else {
      e.loop = m.u8();
      e.loop_volume = e.loop_attenuation = -1;
    }
  }
  if ((bits & kEvent) != 0) {
    const int ev = m.u8();
    if (event != nullptr) *event = ev;
  }
  if ((bits & kSolid) != 0) {
    if (ext > 0)
      m.i32();  // the extension's wider box, not read: no box predicted against
    else
      e.solid = m.i16() & 0xffff;
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

}  // namespace

int q2_music_track(const std::string& text, int last) {
  if (text.empty()) return 0;
  char* end = nullptr;
  const long n = std::strtol(text.c_str(), &end, 10);
  return *end == '\0' && n >= 0 && n < 100 ? static_cast<int>(n) : last;
}

namespace {

void set_string(Demo& d, int index, std::string text) {
  DemoState& s = d.state;
  const CsLayout& cs = layout_of(s.protocol);
  const auto put = [](std::vector<std::string>& list, int i, std::string v) {
    if (static_cast<size_t>(i) >= list.size()) list.resize(static_cast<size_t>(i) + 1);
    list[static_cast<size_t>(i)] = std::move(v);
  };
  if (index == 1) {
    s.cdtrack = q2_music_track(text, s.cdtrack);
    // A name rather than a number: `music/<name>` (q2_music_track keeps the last number for it).
    const bool number = !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
    s.cdname = text.empty() || number ? std::string{} : text;
    if (!s.cdname.empty()) s.cdtrack = -1;
  } else if (index == 2) {
    s.sky = std::move(text);
  } else if (index == cs.max_clients) {
    // Entities 1 to this are the players: kept by slot, so the rest number past `players`.
    d.max_clients = std::clamp(std::atoi(text.c_str()), 1, 32);
    s.players.assign(static_cast<size_t>(d.max_clients), DemoEntity{});
  } else if (index == cs.max_clients + 1) {  // the map's checksum, just after the slots
    s.map_checksum = std::move(text);
  } else if (index >= cs.models && index < cs.sounds) {
    // The world is model 1; a recording that puts it elsewhere is read by the first map named.
    const std::string map = map_of_model(text);
    if (!map.empty() && (index == cs.models + 1 || s.map.empty())) s.map = map;
    put(s.models, index - cs.models, std::move(text));
  } else if (index >= cs.sounds && index < cs.images) {
    put(s.sounds, index - cs.sounds, std::move(text));
  } else if (index >= cs.skins - 512 && index < cs.skins - 512 + int(kDemoLightStyles)) {
    s.light_styles[static_cast<size_t>(index - (cs.skins - 512))] =
        std::move(text);  // the first 64 of 256
  } else if (index >= cs.images && index < cs.images + (cs.sounds - cs.models)) {
    put(s.images, index - cs.images, std::move(text));
  } else if (index == cs.max_clients - 1) {  // the air acceleration, just before the slots
    s.air_accelerate = static_cast<float>(std::atof(text.c_str()));
  } else if (index == kCsStatusBar) {
    s.status_bar = std::move(text);
  } else if (index >= cs.skins - 256 && index < cs.skins) {
    put(s.item_names, index - (cs.skins - 256), std::move(text));
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
  s.light_styles = {};
  s.places = {};
  s.map.clear();
  s.models.assign(1, "");
  s.sounds.assign(1, "");
  s.images.assign(1, "");
  s.sky.clear();
  s.item_names.assign(1, "");
  s.status_bar.clear();
  s.air_accelerate = 0;
  s.layout.clear();
  s.inventory.clear();
  s.intermission = false;
  s.entities.clear();
  s.statics.clear();
  s.static_sounds.clear();
  s.items_back.clear();
  d.baselines.assign(static_cast<size_t>(most_entities(s.protocol)), DemoEntity{});
  for (Frame& f : d.ring) f.clear();
  d.views.fill(Q2View{});
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
void player_info(DemoMsg& m, Q2View& v, int ext) {
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
// The entity list over the base frame `from`; each entity's event this frame (an event is never
// carried from one frame to the next) in `events`, by entity.
bool packet_entities(Demo& d, DemoMsg& m, const Frame* from, Frame& to,
                     std::vector<std::pair<int, int>>& events, std::string& error) {
  to.clear();
  events.clear();
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
    int event = 0;
    entity_delta(m, bits, e, ext, &event);
    if (event != 0) events.emplace_back(n, event);
    to.emplace_back(n, e);
  }
  return true;
}

// Where entity `n` is: the player's own place for him, else the entity's, else the eye.
sh::Vec3f entity_at(const DemoState& s, int n) {
  if (n - 1 == s.player && static_cast<size_t>(s.player) < s.players.size())
    return s.players[static_cast<size_t>(s.player)].origin;
  if (n >= 0 && static_cast<size_t>(n) < s.entities.size())
    return s.entities[static_cast<size_t>(n)].origin;
  return s.eye;
}

constexpr int kPlaceAtFrame = -1;  // an arrival's code: placed by the next frame (not yet seen)

bool seen(const DemoState& s, int n) {
  return (n - 1 == s.player && static_cast<size_t>(s.player) < s.players.size()) ||
         (n >= 0 && static_cast<size_t>(n) < s.entities.size() &&
          s.entities[static_cast<size_t>(n)].present);
}

// Quake II's own arrival effect `name` at entity `n` (effects.hpp's q2_teleport_burst and
// kin, drawn by world_fx.cpp); one not yet seen (a player entering) is placed by the next frame,
// unless `last_place` (a player leaving, already gone from the frame: where he was last).
void arrival(DemoState& s, int n, const char* name, bool last_place = false) {
  DemoEffect e;
  e.kind = DemoEffectKind::named;
  e.name = name;
  e.entity = n;
  e.at = e.end = entity_at(s, n);
  if (!seen(s, n) && !last_place) e.code = kPlaceAtFrame;
  s.effects.push_back(e);
}

// A player's entry, exit or respawn flash heard at `at`: the same sound for all three, at full
// volume (Quake II's own client plays it with the flash).
void flash_sound(DemoState& s, int n, const sh::Vec3f& at) {
  DemoSound o;
  o.entity = n;
  o.channel = kChanWeapon;
  o.name = "weapons/grenlf1a.wav";
  o.origin = at;
  s.started.push_back(o);
}

// Entity `n`'s gun flash `value` (the protocol's message 1, or 2 for a monster's): its sounds and
// the flash at its gun; the own player's plays its sounds alone (the hand draws his flash).
void gun_flash(DemoState& s, int n, int value, bool monster) {
  const bool own = n - 1 == s.player;
  const sh::Vec3f angles = own ? s.angles
                           : n >= 0 && static_cast<size_t>(n) < s.entities.size()
                               ? s.entities[static_cast<size_t>(n)].angles
                               : sh::Vec3f{};
  const auto pick = static_cast<uint32_t>(n * 7919 + static_cast<int>(s.time * 10) * 31 + value);
  q2_flash_fx(value, monster, n, entity_at(s, n), angles, own, pick, s.started, s.effects);
}

bool is_flash(const std::string& name) {
  return name == "sq_login" || name == "sq_logout" || name == "sq_respawn";
}

// The sound the client plays itself for entity `n`'s event `event` in frame `number`: an item's
// respawn, a footstep (one of four, picked by the frame and the entity), a landing short, hard or
// far, a player's teleport. None for the other teleport (a thing moved at once) or an unknown one.
void event_sound(DemoState& s, int n, int event, int32_t number) {
  static constexpr const char* kSteps[4] = {"player/step1.wav", "player/step2.wav",
                                            "player/step3.wav", "player/step4.wav"};
  DemoSound o;
  o.entity = n;
  o.channel = kChanAuto;
  switch (event) {
    case kEvItemRespawn:
      arrival(s, n, "sq_item_back");
      o.name = "items/respawn1.wav";
      o.channel = kChanWeapon;
      o.attenuation = 2;
      break;
    case kEvFootstep:
      // A hash of the frame and the entity: steps come once a stride, so a plain sum repeats.
      o.name = kSteps[((static_cast<uint32_t>(number) * 2654435761u) ^
                       (static_cast<uint32_t>(n) * 40503u)) >>
                      30];
      o.channel = kChanBody;
      o.step = true;
      break;
    case kEvFallShort:
      o.name = "player/land1.wav";
      break;
    case kEvFall:
      o.name = "*fall2.wav";
      break;
    case kEvFallFar:
      o.name = "*fall1.wav";
      break;
    case kEvPlayerTeleport:
      arrival(s, n, "sq_teleport");
      o.name = "misc/tele1.wav";
      o.channel = kChanWeapon;
      o.attenuation = 2;
      break;
    default:
      return;
  }
  const size_t at = static_cast<size_t>(n);
  if (n - 1 == s.player && static_cast<size_t>(s.player) < s.players.size())
    o.origin = s.players[static_cast<size_t>(s.player)].origin;
  else if (at < s.entities.size())
    o.origin = s.entities[at].origin;
  else
    o.origin = s.eye;
  s.started.push_back(o);
}

// The frame: its header, the player's state and the entity list.
bool frame(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  const int32_t number = m.i32();
  const int32_t delta = m.i32();
  const int held_back = s.protocol != 26 ? m.u8() : 0;  // the packets held back for the rate
  {  // the areas the eye's room joins (one bit an area); drawn only these
    const size_t n = m.u8();
    if (n > 32) {
      error = "the frame" + at_byte(m) + " names " + std::to_string(n) +
              " bytes of areas, past the 32 the game allows: the recording is damaged";
      return false;
    }
    s.areas.resize(n);
    for (uint8_t& b : s.areas) b = static_cast<uint8_t>(m.u8());
  }
  if (m.u8() != kPlayerInfo) {
    error = "the frame" + at_byte(m) +
            " is not followed by the player's state: the recording is damaged";
    return false;
  }
  const size_t slot = static_cast<size_t>(number) & 15;
  const size_t base_slot = static_cast<size_t>(delta) & 15;
  const bool have_base = delta > 0 && d.views[base_slot].frame == delta;
  s.q2_trouble = static_cast<uint8_t>(
      (delta <= 0 ? kQ2FrameNoDelta : 0) |
      (delta > 0 && !have_base ? (number - delta >= 16 ? kQ2FrameOld : kQ2FrameBad) : 0) |
      (held_back > 0 ? kQ2FrameSuppressed : 0));
  Q2View v = have_base ? d.views[base_slot] : Q2View{};
  player_info(m, v, ext_of(s.protocol));
  v.frame = number;
  if (m.u8() != kPacketEntities) {
    error =
        "the frame" + at_byte(m) + " is not followed by its entity list: the recording is damaged";
    return false;
  }
  Frame to;
  std::vector<std::pair<int, int>> events;
  if (!packet_entities(d, m, have_base ? &d.ring[base_slot] : nullptr, to, events, error))
    return false;
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
  s.gun_angles = {static_cast<float>(v.gun_angles[0]) * 0.25f,
                  static_cast<float>(v.gun_angles[1]) * 0.25f,
                  static_cast<float>(v.gun_angles[2]) * 0.25f};
  s.gun_offset = {static_cast<float>(v.gun_offset[0]) * 0.25f,
                  static_cast<float>(v.gun_offset[1]) * 0.25f,
                  static_cast<float>(v.gun_offset[2]) * 0.25f};
  s.fov = static_cast<float>(v.fov);
  s.rdflags = v.rdflags;
  s.teleport_bit = ext_of(s.protocol) > 0 ? (v.rdflags & 16) != 0 : (v.pm_flags & 128) != 0;
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
  std::copy(v.stats, v.stats + s.q2_stats.size(), s.q2_stats.begin());  // the bar reads 32
  s.stats[kStatHealth] = v.stats[kSeqHealth];
  s.stats[kStatAmmo] = v.stats[kSeqAmmo];
  s.stats[kStatArmour] = v.stats[kSeqArmour];
  s.stats[kStatWeapon] = v.gun_model;
  for (DemoEffect& e : s.effects)  // the arrivals of players not seen until this frame
    if (e.code == kPlaceAtFrame && e.kind == DemoEffectKind::named) {
      e.at = e.end = entity_at(s, e.entity);
      e.code = 0;
      if (is_flash(e.name)) flash_sound(s, e.entity, e.at);  // heard where he is, now known
    }
  for (const auto& [n, event] : events) event_sound(s, n, event, number);
  // A teleporter pad's fountain, a burst each frame (its effect bit).
  for (size_t n = 1; n < ents.size(); n++)
    if (ents[n].present && (ents[n].effects & kSqTeleporterBit) != 0)
      arrival(s, static_cast<int>(n), "sq_fountain");
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
  // A server that sends a hit's or a blast's sound beside its temporary entity (ours does) is
  // heard once: the client already plays that sound for the entity (q2_temp_fx.hpp).
  std::string_view name;
  if (o.sound > 0 && static_cast<size_t>(o.sound) < s.sounds.size())
    name = s.sounds[size_t(o.sound)];
  for (const DemoSound& t : s.started)
    if (t.entity == -1 && t.name != nullptr && !name.empty() && name == t.name) return;
  s.started.push_back(o);
}

// A direction by its index into the protocol's table, into `way` (left alone when null).
bool direction(DemoMsg& m, std::string& error, sh::Vec3f* way = nullptr) {
  const int i = m.u8();
  if (i < kDirections) {
    if (way != nullptr) {
      const auto& d = ge::classic::protocol::directions()[size_t(i)];
      *way = {d[0], d[1], d[2]};
    }
    return true;
  }
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
    return direction(m, error, &e.way);
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
    case 2:  // the blaster's bolt landing: its yellow sparks and the mark, as our own game's
      if (!point_dir(DemoEffectKind::named)) return false;
      e.name = "te_blaster";
      break;
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
    case 28:
    case 35:
    case 47:
    case 51:
    case 53:
    case 54:
      point(DemoEffectKind::explosion);
      break;
    case 21: {  // the BFG's big burst: an explosion, and its green burst and mark beside it
      point(DemoEffectKind::explosion);
      DemoEffect burst = e;
      burst.kind = DemoEffectKind::named;
      burst.name = "te_bfg_bigexplosion";
      if (!m.bad) s.effects.push_back(std::move(burst));
      break;
    }
    case 22:  // a boss teleporting out: its swirl (its sound is q2_temp_sound's)
      point(DemoEffectKind::named);
      e.name = "sq_boss_teleport";
      break;
    case 48:  // Quake II's teleport burst (the base game sends none; a mod may)
      point(DemoEffectKind::named);
      e.name = "sq_teleport";
      break;
    case 45:  // the chain fist's smoke: straight up
      point(DemoEffectKind::named);
      e.name = "sq_smoke";
      break;
    case 49:  // a goal's burst: the teleport's
      point(DemoEffectKind::named);
      e.name = "sq_teleport";
      break;
    case 52:
      point(DemoEffectKind::named);
      e.name = "sq_widow_splash";
      break;
    case 3:  // the rail's line: its core and spiral as cl_railtrail_type asks, as our own game's
      line(DemoEffectKind::named_trail);
      e.name = "te_railtrail";
      break;
    case 23:
    case 34:  // a laser, a trail
      line(DemoEffectKind::beam);
      break;
    case 11:
    case 41:  // bubbles along a line; the second kind with its sound at the start
      line(DemoEffectKind::named_trail);
      e.name = kind == 11 ? "sq_bubbles" : "sq_bubbles2";
      if (kind == 41) {
        DemoSound o;
        o.entity = -1;
        o.name = "weapons/lashit.wav";
        o.origin = e.at;
        s.started.push_back(o);
      }
      break;
    case 27:  // a blue bolt's hit, its way sent as a second point (taken as it is)
      line(DemoEffectKind::named);
      e.name = "sq_blue_hit";
      e.way = e.end;
      e.end = e.at;
      break;
    case 10:    // a splash: count, point, direction, its kind (sparks, water, mud, ...)
    case 15: {  // a laser's sparks: count, point, direction, a palette colour
      constexpr int kSplashColour[7] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};
      const int count = m.u8();
      if (!point_dir(DemoEffectKind::named)) return false;
      const int c = m.u8();
      e.name = kind == 10 ? "sq_splash" : "sq_laser_sparks";
      e.count = count;
      e.colour = kind == 15 ? c : c < 7 ? kSplashColour[c] : 0;
      break;
    }
    case 25:
    case 29: {  // welding or tunnel sparks: count, point, direction, colour
      const int count = m.u8();
      if (!point_dir(DemoEffectKind::named)) return false;
      e.colour = m.u8();
      e.count = count;
      e.name = kind == 25 ? "sq_welding" : "sq_tunnel_sparks";
      break;
    }
    case 16:
    case 19:  // a parasite's tongue, a medic's cable: entity, start, end, as our own game's
      e.entity = m.i16();
      line(DemoEffectKind::named_trail);
      e.name = kind == 16 ? "te_parasite_attack" : "te_medic_cable_attack";
      break;
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
      point(DemoEffectKind::named);
      e.name = "sq_flashlight";
      e.entity = m.i16();
      break;
    case 37:  // a force wall: start, end, colour
      line(DemoEffectKind::named_trail);
      e.name = "sq_force_wall";
      e.colour = m.u8();
      break;
    case 40: {  // steam: id, count, point, direction, colour, magnitude, and a time with an id
      const int id = m.i16();
      const int count = m.u8();
      if (!point_dir(DemoEffectKind::named)) return false;
      e.colour = m.u8();
      const int magnitude = m.i16();
      if (id != -1) m.i32();  // a lasting jet: its first puff drawn, the rest not (yet)
      e.name = "sq_steam";
      e.count = count;
      e.way = e.way * static_cast<float>(magnitude);  // the jet: its way times its speed
      break;
    }
    case 50:  // the widow's beam-out: an id and a point, a sphere growing for 2.1 s
      e.entity = m.i16();
      point(DemoEffectKind::named);
      e.name = "sq_widow_beam";
      break;
    // The kinds a newer game's server sends, in any protocol's recording.
    case 31:  // a lighter rail's line
    case 57:  // a bolt's zap
      line(DemoEffectKind::beam);
      break;
    case 56:  // a blue bolt's hit: point, direction
      if (!point_dir(DemoEffectKind::named)) return false;
      e.name = "sq_blue_hit";
      break;
    case 58:  // a slam on the ground
      if (!point_dir(DemoEffectKind::spike)) return false;
      break;
    case 59:  // a cable or a beam: entity, start, end
    case 61:
      e.entity = m.i16();
      line(DemoEffectKind::beam);
      break;
    case 60: {  // a power splash: entity, a count not drawn; round the entity's box as it stands
      const int n = m.i16();
      m.u8();
      keep = false;
      if (n < 1 || static_cast<size_t>(n) >= s.entities.size() || !s.entities[size_t(n)].present)
        break;
      const DemoEntity& d = s.entities[size_t(n)];
      // The packed box: half width, depth below and height above, in units.
      const float x = 8.0f * float(d.solid & 31), below = 8.0f * float((d.solid >> 5) & 31),
                  above = 8.0f * float((d.solid >> 10) & 63) - 32;
      const bool boxed = d.solid != 0 && d.solid != 31;
      e.kind = DemoEffectKind::named;
      e.name = "sq_power_splash";
      e.at = d.origin + sh::Vec3f{0, 0, boxed ? (above - below) / 2 : 0.0f};
      e.end = e.at;
      const float h = above + below;
      e.way = {boxed ? 0.5f * std::sqrt(8 * x * x + h * h) : 0.0f, 0, 0};  // its reach, units
      keep = true;
      break;
    }
    case 62:
    case 63:  // explosions that give no light
      point(DemoEffectKind::explosion);
      break;
    case 128: {  // damage dealt: the hit marker's (q2_bar.hpp) and its sound, heard everywhere
      if (m.i16() > 0 && !m.bad) {
        s.hits++;
        DemoSound o;
        o.name = "weapons/marker.wav";  // DemoSounds plays it only with cl_hit_markers 2
        o.attenuation = 0;
        o.origin = s.eye;
        s.started.push_back(o);
      }
      keep = false;
      break;
    }
    default:
      error = "a temporary entity of kind " + std::to_string(kind) + at_byte(m) +
              " that this game's recordings do not carry: the recording is damaged";
      return false;
  }
  // Quake II's puff for a hit (its specks' count and colours), and the sound the client itself
  // plays for one (the server sends none: q2_temp_fx.hpp).
  if (const Q2Puff* puff = q2_puff(kind); puff != nullptr) {
    e.q2_specks = puff->count;
    e.colour = puff->colour;
  }
  const auto pick = static_cast<uint32_t>(static_cast<int>(s.time * 1000) * 7919 +
                                          static_cast<int>(e.at.x) * 31 + static_cast<int>(e.at.y));
  if (const Q2TempSound ts = q2_temp_sound(kind, pick); ts.name != nullptr && !m.bad) {
    const bool busy =
        ts.alone && std::any_of(s.started.begin(), s.started.end(), [&](const DemoSound& o) {
          return o.name != nullptr && std::string_view(o.name) == ts.name;
        });
    if (!busy) {
      DemoSound o;
      o.entity = -1;
      o.name = ts.name;
      o.attenuation = ts.attenuation;
      o.origin = ts.at_end ? e.end : e.at;
      s.started.push_back(o);
    }
  }
  if (keep && !m.bad) s.effects.push_back(e);
  return true;
}

// One message into the state (its id first); false with the reason. A disconnect sets `ended`.
bool read_message(Demo& d, DemoMsg& m, bool& framed, std::string& error) {
  DemoState& s = d.state;
  {
    const size_t at = m.at;
    m.int23 = ext_of(s.protocol) >= 2;  // set again after a server data
    const int id = m.u8();
    switch (id) {
      case kMuzzleFlash: {  // a player entering (9), leaving (10) or respawned (11): its burst
        const int n = m.i16(), value = m.u8(), flash = value & 127;
        if (flash >= 9 && flash <= 11) {
          const bool leaving = flash == 10;
          arrival(s, n, flash == 9 ? "sq_login" : leaving ? "sq_logout" : "sq_respawn", leaving);
          if (seen(s, n) || leaving) flash_sound(s, n, entity_at(s, n));  // else when placed
        } else {  // a gun's: its sounds and the look's flash (q2_flash_fx.hpp)
          gun_flash(s, n, value, false);
        }
        break;
      }
      case kMuzzleFlash2: {  // a monster's gun
        const int n = m.i16(), value = m.u8();
        gun_flash(s, n, value, true);
        break;
      }
      case kTempEntity:
        if (!temp_entity(s, m, error)) return false;
        break;
      case kLayout:  // the screen the server put up (a scoreboard, the help computer)
        s.layout = m.str();
        break;
      case kStuffText: {  // a place name the recording kept read; the rest kept for the caller
        std::string text = m.str();
        if (ge::classic::read_recorded_loc(text, s.places)) break;
        if (d.stuffed.size() < kQ2StuffedKept) d.stuffed.push_back(std::move(text));
        break;
      }
      case kInventory:  // the counts by item number
        s.inventory.resize(256);
        for (int16_t& n : s.inventory) n = static_cast<int16_t>(m.i16());
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
        if (ext_of(s.protocol) > 0 && p.level >= 4) {  // the extended levels 4 and 5: centred
          s.centre = std::move(p.text);
          break;
        }
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
        Demo::DownloadPiece p;
        p.size = m.i16();
        p.percent = m.u8();
        if (p.size > 0 && m.at + static_cast<size_t>(p.size) <= m.b.size()) {
          p.bytes.assign(m.b.begin() + static_cast<std::ptrdiff_t>(m.at),
                         m.b.begin() + static_cast<std::ptrdiff_t>(m.at) + p.size);
        }
        if (p.size > 0) m.skip(static_cast<size_t>(p.size));
        if (d.download_pieces.size() < 64) d.download_pieces.push_back(std::move(p));
        break;
      }
      case kGeOrigin: {  // the game's own: the archive the download that follows comes out of
        std::string path;
        ge::vfs::ArchiveOrigin o;
        if (!ge::vfs::read_origin(m.b, &m.at, &path, &o)) {
          m.bad = true;
          break;
        }
        if (d.download_origins.size() < 64) d.download_origins.emplace_back(path, std::move(o));
        break;
      }
      case kGeMoves: {  // the game's own: the other players' moves after the frame
        d.other_moves.clear();
        const int n = m.u8();
        for (int k = 0; k < n && !m.bad; k++) {
          Demo::OtherMove o;
          o.slot = m.u8();
          o.msec = m.u8();
          o.state.type = static_cast<ge::classic::Q2Type>(std::min(m.u8(), 4));
          for (int16_t& v : o.state.origin) v = static_cast<int16_t>(m.i16());
          for (int16_t& v : o.state.velocity) v = static_cast<int16_t>(m.i16());
          o.state.flags = static_cast<uint8_t>(m.u8());
          o.state.time = static_cast<uint8_t>(m.u8());
          o.state.gravity = static_cast<int16_t>(m.i16());
          for (int16_t& v : o.state.delta_angles) v = static_cast<int16_t>(m.i16());
          o.cmd.buttons = static_cast<uint8_t>(m.u8());
          for (int16_t& v : o.cmd.angles) v = static_cast<int16_t>(m.i16());
          o.cmd.forward = static_cast<int16_t>(m.i16());
          o.cmd.side = static_cast<int16_t>(m.i16());
          o.cmd.up = static_cast<int16_t>(m.i16());
          if (o.slot < 256) d.other_moves.push_back(o);
        }
        d.other_moves_read++;
        break;
      }
      case kGeBack: {  // an item taken: where it will lie, as what, and when it comes back
        DemoItemBack b;
        b.entity = m.i16();
        b.model = m.i16();
        b.skin = m.u8();
        const float x = static_cast<float>(static_cast<int16_t>(m.i16())) * 0.125f;
        const float y = static_cast<float>(static_cast<int16_t>(m.i16())) * 0.125f;
        const float z = static_cast<float>(static_cast<int16_t>(m.i16())) * 0.125f;
        b.origin = {x, y, z};
        b.yaw = static_cast<float>(static_cast<int16_t>(m.i16())) * (360.0f / 65536);
        b.at = s.time + static_cast<double>(static_cast<uint32_t>(m.i32())) * 0.001;
        if (!m.bad) take_item_back(s, b);
        break;
      }
      case kGeAccountKey:  // the logins' key tag
        d.account_tag = m.str();
        break;
      case kGeFeel: {  // the move's feel: the floats this client knows, the rest passed over
        const int count = m.u8();
        float got[6] = {300, 6, 0, 0.5f, 1, 350};
        for (int i = 0; i < count; i++) {
          uint32_t u = static_cast<uint32_t>(m.u8());
          for (int k = 1; k < 4; k++) u |= static_cast<uint32_t>(m.u8()) << (8 * k);
          float v = 0;
          std::memcpy(&v, &u, 4);
          if (i < 6) got[i] = v;
        }
        const int flags = m.u8();
        ge::classic::Q2Feel& f = d.feel;
        f.max_speed = got[0];
        f.friction = got[1];
        f.air_accelerate = got[2];
        f.swim_scale = got[3];
        f.water_friction = got[4];
        f.water_jump = got[5];
        f.land_pause = (flags & 1) != 0;
        f.air_step = (flags & 2) != 0;
        f.surface_lift = (flags & 4) == 0;  // a server that never sends the bit keeps the lift
        d.feel_told = true;
        break;
      }
      case kGeVoice: {  // a voice relayed, or the empty one that says voice is taken here
        DemoVoice v;
        if (!read_voice(m, v)) break;
        if (v.sender == 255)
          d.voice_taken = true;
        else if (!v.data.empty())
          d.state.voice.push_back(std::move(v));
        break;
      }
      case kGeSheet: {  // a part of the settings sheet: offered, or sent as asked
        const int part = m.u8();
        const std::string text = m.str();
        d.sheet_offered = true;
        ge::server::sheet_take(d.sheet, part, text);
        break;
      }
      case kGeShare: {  // the easy jump's share, thousandths
        const int lo = m.u8(), hi = m.u8();
        d.speedjump_share = static_cast<float>(std::min(1000, lo | hi << 8)) * 0.001f;
        break;
      }
      case kGeStats: {  // every player's state, in the multiplayer recordings' stat layout
        if (d.player_stats.size() < 32) d.player_stats.resize(32);
        const int n = m.u8();
        for (int k = 0; k < n && !m.bad; k++) {
          const int slot = m.u8(), type = m.u8();
          const int health = m.i16(), armour = m.i16(), frags = m.i16(), bit = m.i16();
          const int rockets = m.i16();
          if (slot >= 32) continue;
          auto& st = d.player_stats[static_cast<size_t>(slot)];
          st[kStatHealth] = health;
          st[kStatArmour] = armour;
          st[kStatActiveWeapon] = bit;
          st[kStatItems] = bit;  // he holds at least the weapon in hand
          st[kStatRockets] = rockets;
          s.frags[static_cast<size_t>(slot)] = frags;
          if (static_cast<size_t>(slot) < s.players.size()) {
            s.players[static_cast<size_t>(slot)].dead = type >= 2;
            s.players[static_cast<size_t>(slot)].gib = type == 3;
          }
        }
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

// One block's messages into the state.
bool read_block(Demo& d, DemoMsg& m, bool& framed, std::string& error) {
  if (d.spans != nullptr) d.spans->clear();
  while (!m.done()) {
    if (!read_message(d, m, framed, error)) return false;
    if (d.ended) return true;
  }
  return true;
}

int32_t i32_at(std::span<const uint8_t> b, size_t at) {
  uint32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);  // little-endian on every platform the game runs on
  return static_cast<int32_t>(v);
}

}  // namespace

void q2_stream_begin(Demo& d) {
  d.state.format = DemoFormat::q2;
  d.state.protocol = 34;
  d.ring.resize(16);
  d.baselines.resize(kMostEntitiesExt);
}

bool q2_message_end(Demo& scratch, std::span<const uint8_t> b, size_t at, size_t& end,
                    std::string& error) {
  DemoMsg m{b};
  m.at = at;
  bool framed = false;
  scratch.spans = nullptr;
  scratch.ended = false;
  const bool ok = read_message(scratch, m, framed, error);
  end = m.at;
  return ok;
}

void q2_scratch_clear(Demo& d) {
  DemoState& s = d.state;
  s.effects.clear();
  s.prints.clear();
  s.started.clear();
  s.stopped.clear();
  s.voice.clear();
  s.centre.clear();
  d.stuffed.clear();
  d.download_pieces.clear();
  d.download_origins.clear();
  d.ended = false;
  d.reconnect = false;
}

bool is_q2_demo(std::span<const uint8_t> bytes) {
  // The server data opens the recording, or follows a few short blocks a recording tool put
  // first (a stuffed command such as "hand 3"); each block before it is a whole one.
  size_t at = 0;
  for (int skipped = 0; skipped < 4; skipped++) {
    if (at + 13 > bytes.size()) return false;
    const int32_t len = i32_at(bytes, at);
    if (len <= 0 || len > kMostBlockExt || at + 4 + size_t(len) > bytes.size()) return false;
    if (bytes[at + 4] == kServerData) {
      if (len < 9) return false;
      const int32_t protocol = i32_at(bytes, at + 5);
      const bool extended = ext_of(protocol) > 0;
      if (len > (extended ? kMostBlockExt : kMostBlock)) return false;
      return (extended || (protocol >= 26 && protocol <= 34)) && bytes[at + 13] <= 2;
    }
    if (len > kMostBlock) return false;
    at += 4 + size_t(len);
  }
  return false;
}

DemoRead next_q2(Demo& d, std::string& error) {
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
        d.wide_blocks || ext_of(d.state.protocol) > 0 ||
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
