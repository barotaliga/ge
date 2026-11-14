// The single-player protocol's messages (versions 15, 666 and 999) as a demo block carries them
// (demo.hpp):
// every message read to its end so the next one is found, and the few that matter kept: the
// level's start and its precache lists, the view entity, the entities' baselines and updates,
// the clientdata's view height and status, the clock, the intermission.
//
// THE WIRE: little-endian; a coordinate is an i16 of eighths of a unit, an angle an i8 of 1/256
// of a turn. A message id with bit 7 set is an ENTITY UPDATE (its low 7 bits are its first flag
// byte); every other id is one of the table below. An entity update's fields are deltas from the
// entity's BASELINE (a field not sent is the baseline's), never from the last update.
//
// THE LARGER LIMITS (666 and 999, the facts in porting/shooters-edge/demo-formats-single.md § A):
// an entity update's and the clientdata's flags run on to a third and fourth byte (bit 15, then
// 23), carrying an entity's alpha, scale, the high bytes of its model and frame and its next
// step's time, and the high bytes of the status's numbers; baselines and statics come in a
// second form with flags (wide model and frame, alpha, scale); a sound may name its entity and
// index by a word; and the skybox, fog, bonus flash, static sound by a word, achievement and
// local sound messages. 999 adds a flags word that picks the coordinates' (eighths, sixteenths in
// an i32, an i16 and a fraction byte, or a float) and the angles' (a byte, a short or a float)
// encodings. Protocol 15's own bit 15 is the older transparency (two or three floats).
//
// ge:summary The single-player protocol's messages read off a demo block: level, view, entities
// ge:keywords single-player protocol-15 serverinfo setview clientdata entity-update baseline
// ge:keywords spawnbaseline spawnstatic temp-entity svc read_single_messages
// ge:keywords protocol-666 protocol-999 larger-limits alpha scale skybox fog wide-coordinates
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp tests/shooters_edge_demo_single_test.cpp
#include <algorithm>
#include <cstdint>
#include <string>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace {

// Message ids.
enum : uint8_t {
  kBad = 0,
  kNop = 1,
  kDisconnect = 2,
  kUpdateStat = 3,
  kVersion = 4,
  kSetView = 5,
  kSound = 6,
  kTime = 7,
  kPrint = 8,
  kStuffText = 9,
  kSetAngle = 10,
  kServerInfo = 11,
  kLightStyle = 12,
  kUpdateName = 13,
  kUpdateFrags = 14,
  kClientData = 15,
  kStopSound = 16,
  kUpdateColors = 17,
  kParticle = 18,
  kDamage = 19,
  kSpawnStatic = 20,
  kSpawnBinary = 21,
  kSpawnBaseline = 22,
  kTempEntity = 23,
  kSetPause = 24,
  kSignonNum = 25,
  kCenterPrint = 26,
  kKilledMonster = 27,
  kFoundSecret = 28,
  kSpawnStaticSound = 29,
  kIntermission = 30,
  kFinale = 31,
  kCdTrack = 32,
  kSellScreen = 33,
  kCutscene = 34,
  // The larger limits' (666 and 999).
  kSkybox = 37,
  kBonusFlash = 40,
  kFog = 41,
  kSpawnBaseline2 = 42,
  kSpawnStatic2 = 43,
  kSpawnStaticSound2 = 44,
  kAchievement = 52,
  kLocalSound = 56,
};

// An entity update's flags: the first byte's low 7 bits, then a second byte when kMoreBits.
enum : uint32_t {
  kMoreBits = 1 << 0,
  kOrigin1 = 1 << 1,
  kOrigin2 = 1 << 2,
  kOrigin3 = 1 << 3,
  kAngle2 = 1 << 4,
  kFrame = 1 << 6,
  kAngle1 = 1 << 8,
  kAngle3 = 1 << 9,
  kModel = 1 << 10,
  kColormap = 1 << 11,
  kSkin = 1 << 12,
  kEffects = 1 << 13,
  kLongEntity = 1 << 14,
  kExtend1 = 1 << 15,  // 666 and 999: a third byte (15: the older transparency)
  kAlpha = 1 << 16,
  kFrameHigh = 1 << 17,
  kModelHigh = 1 << 18,
  kLerpFinish = 1 << 19,
  kScale = 1 << 20,
  kExtend2 = 1 << 23,  // a fourth byte
};

// clientdata's flags.
enum : uint32_t {
  kViewHeight = 1 << 0,
  kIdealPitch = 1 << 1,
  kPunch1 = 1 << 2,
  kVelocity1 = 1 << 5,
  kItems = 1 << 9,
  kWeaponFrame = 1 << 12,
  kArmour = 1 << 13,
  kWeapon = 1 << 14,
  // 666 and 999: the high bytes after the status, and the view model's alpha.
  kMore = 1 << 15,
  kWeaponHigh = 1 << 16,
  kArmourHigh = 1 << 17,
  kAmmoHigh = 1 << 18,
  kShellsHigh = 1 << 19,
  kNailsHigh = 1 << 20,
  kRocketsHigh = 1 << 21,
  kCellsHigh = 1 << 22,
  kMore2 = 1 << 23,
  kWeaponFrameHigh = 1 << 24,
  kWeaponAlpha = 1 << 25,
};

// Protocol 999's encoding flags.
enum : uint8_t {
  kShortAngles = 1 << 1,
  kFloatAngles = 1 << 2,
  kCoord24 = 1 << 3,
  kFloatCoords = 1 << 4,
  kIntCoords = 1 << 7,
};

constexpr int kMostEntities = 8192;

// A coordinate in the stream's encoding: a float, sixteenths in an i32, an i16 and a fraction
// byte, or (protocol 15 and 666) eighths in an i16.
float coord(DemoMsg& m) {
  const uint32_t f = m.single_flags;
  if ((f & kFloatCoords) != 0) return m.f32();
  if ((f & kIntCoords) != 0) return static_cast<float>(m.i32()) * 0.0625f;
  if ((f & kCoord24) != 0) {
    const float whole = static_cast<float>(m.i16());
    return whole + static_cast<float>(m.u8()) / 255.0f;
  }
  return static_cast<float>(m.i16()) * 0.125f;
}
// An angle: a float in degrees, a short of 1/65536 turn, or a byte of 1/256.
float angle(DemoMsg& m) {
  const uint32_t f = m.single_flags;
  if ((f & kFloatAngles) != 0) return m.f32();
  if ((f & kShortAngles) != 0) return static_cast<float>(m.i16()) * (360.0f / 65536.0f);
  return static_cast<float>(m.i8()) * (360.0f / 256.0f);
}
// The larger limits' alpha byte: 0 the default (whole), 1 unseen, else (b - 1) / 254.
float wide_alpha(int b) { return b == 0 ? 1.0f : static_cast<float>(b - 1) / 254.0f; }
bool larger_limits(const DemoState& s) { return s.protocol == 666 || s.protocol == 999; }

DemoEntity& slot(std::vector<DemoEntity>& v, int n) {
  if (static_cast<size_t>(n) >= v.size()) v.resize(static_cast<size_t>(n) + 1);
  return v[static_cast<size_t>(n)];
}

// The second form's flags (666 and 999): a word's model, a word's frame, alpha, scale.
enum : uint8_t { kLargeModel = 1, kLargeFrame = 2, kSpawnAlpha = 4, kSpawnScale = 8 };

// model, frame, colormap, skin, then origin and angle by axis: a baseline's or a static's body,
// the second form's `flags` widening it.
DemoEntity read_spawn(DemoMsg& m, int flags = 0) {
  DemoEntity e;
  e.model = (flags & kLargeModel) != 0 ? m.u16() : m.u8();
  e.frame = (flags & kLargeFrame) != 0 ? m.u16() : m.u8();
  m.u8();  // colormap
  e.skin = m.u8();
  float o[3], a[3];
  for (int i = 0; i < 3; i++) {
    o[i] = coord(m);
    a[i] = angle(m);
  }
  e.origin = {o[0], o[1], o[2]};
  e.angles = {a[0], a[1], a[2]};
  if ((flags & kSpawnAlpha) != 0) e.alpha = wide_alpha(m.u8());
  if ((flags & kSpawnScale) != 0) e.scale = byte_scale(m.u8());
  return e;
}

bool entity_update(Demo& d, DemoMsg& m, int first, std::string& error) {
  const bool wide = larger_limits(d.state);
  uint32_t bits = static_cast<uint32_t>(first & 0x7F);
  if ((bits & kMoreBits) != 0) bits |= static_cast<uint32_t>(m.u8()) << 8;
  if (wide && (bits & kExtend1) != 0) bits |= static_cast<uint32_t>(m.u8()) << 16;
  if (wide && (bits & kExtend2) != 0) bits |= static_cast<uint32_t>(m.u8()) << 24;
  const int n = (bits & kLongEntity) != 0 ? m.i16() : m.u8();
  if (n < 0 || n >= kMostEntities) {
    error = "an entity update names entity " + std::to_string(n);
    return false;
  }
  DemoEntity e = static_cast<size_t>(n) < d.baselines.size() ? d.baselines[static_cast<size_t>(n)]
                                                             : DemoEntity{};
  if ((bits & kModel) != 0) e.model = m.u8();
  if ((bits & kFrame) != 0) e.frame = m.u8();
  if ((bits & kColormap) != 0) m.u8();
  if ((bits & kSkin) != 0) e.skin = m.u8();
  if ((bits & kEffects) != 0) e.effects = m.u8();
  if ((bits & kOrigin1) != 0) e.origin.x = coord(m);
  if ((bits & kAngle1) != 0) e.angles.x = angle(m);
  if ((bits & kOrigin2) != 0) e.origin.y = coord(m);
  if ((bits & kAngle2) != 0) e.angles.y = angle(m);
  if ((bits & kOrigin3) != 0) e.origin.z = coord(m);
  if ((bits & kAngle3) != 0) e.angles.z = angle(m);
  if (!wide && (bits & kExtend1) != 0) {  // the older transparency: a mode, the alpha, a glow
    const float mode = m.f32();
    e.alpha = std::clamp(m.f32(), 0.0f, 1.0f);
    if (mode == 2.0f) m.f32();
  }
  if (wide) {
    if ((bits & kAlpha) != 0) e.alpha = wide_alpha(m.u8());
    if ((bits & kScale) != 0) e.scale = byte_scale(m.u8());
    if ((bits & kFrameHigh) != 0) e.frame |= m.u8() << 8;
    if ((bits & kModelHigh) != 0) e.model |= m.u8() << 8;
    if ((bits & kLerpFinish) != 0) m.u8();  // the next step's time, for the lerp
  }
  e.present = true;
  slot(d.state.entities, n) = e;
  return true;
}

void client_data(DemoState& s, DemoMsg& m) {
  uint32_t bits = static_cast<uint32_t>(m.u16());
  const bool wide = larger_limits(s);
  if (wide && (bits & kMore) != 0) bits |= static_cast<uint32_t>(m.u8()) << 16;
  if (wide && (bits & kMore2) != 0) bits |= static_cast<uint32_t>(m.u8()) << 24;
  s.view_height = (bits & kViewHeight) != 0 ? static_cast<float>(m.i8()) : 22.0f;
  if ((bits & kIdealPitch) != 0) m.i8();
  for (int i = 0; i < 3; i++) {
    if ((bits & (kPunch1 << i)) != 0) m.i8();
    if ((bits & (kVelocity1 << i)) != 0) m.i8();
  }
  if ((bits & kItems) != 0) s.stats[kStatItems] = m.i32();
  s.weapon_frame = (bits & kWeaponFrame) != 0 ? m.u8() : 0;
  s.stats[kStatArmour] = (bits & kArmour) != 0 ? m.u8() : 0;
  s.stats[kStatWeapon] = (bits & kWeapon) != 0 ? m.u8() : 0;
  s.stats[kStatHealth] = m.i16();
  s.stats[kStatAmmo] = m.u8();
  s.stats[kStatShells] = m.u8();
  s.stats[kStatNails] = m.u8();
  s.stats[kStatRockets] = m.u8();
  s.stats[kStatCells] = m.u8();
  s.stats[kStatActiveWeapon] = m.u8();
  if (!wide) return;
  const auto high = [&](uint32_t bit, auto& v) {
    if ((bits & bit) != 0) v |= m.u8() << 8;
  };
  high(kWeaponHigh, s.stats[kStatWeapon]);
  high(kArmourHigh, s.stats[kStatArmour]);
  high(kAmmoHigh, s.stats[kStatAmmo]);
  high(kShellsHigh, s.stats[kStatShells]);
  high(kNailsHigh, s.stats[kStatNails]);
  high(kRocketsHigh, s.stats[kStatRockets]);
  high(kCellsHigh, s.stats[kStatCells]);
  if ((bits & kWeaponFrameHigh) != 0) s.weapon_frame |= m.u8() << 8;
  if ((bits & kWeaponAlpha) != 0) m.u8();
}

bool server_info(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  s.protocol = m.i32();
  if (s.protocol == 0x58455446 || s.protocol == 0x32455446) {
    error =
        "the level starts with a protocol extension's marker, whose entity messages this "
        "player does not read";
    return false;
  }
  if (s.protocol != 15 && s.protocol != 666 && s.protocol != 999) {
    error = "the level starts in protocol " + std::to_string(s.protocol) +
            ", and only protocols 15, 666 and 999 can be played";
    return false;
  }
  d.single_flags = s.protocol == 999 ? static_cast<uint32_t>(m.i32()) : 0u;
  m.single_flags = d.single_flags;
  s.max_clients = m.u8();  // the most clients: the players' entities
  m.u8();                  // the game type
  m.str();                 // the level's title
  s.models.assign(1, "");
  for (std::string name = m.str(); !name.empty() && !m.bad; name = m.str())
    s.models.push_back(name);
  s.sounds.assign(1, "");
  for (std::string name = m.str(); !name.empty() && !m.bad; name = m.str())
    s.sounds.push_back(name);
  s.map = s.models.size() > 1 ? map_of_model(s.models[1]) : "";
  s.levels++;
  s.intermission = false;
  s.stats[kStatMonsters] = s.stats[kStatSecrets] = 0;  // the new level's counts start over
  s.entities.clear();
  s.statics.clear();
  s.static_sounds.clear();
  d.baselines.clear();
  return true;
}

// The sound message: a mask (volume, attenuation), its fields, the entity and channel packed in
// a short, the sound's index, the point. 666 and 999: mask bit 3 the entity a word and the
// channel a byte, bit 4 the index a word.
void sound(DemoState& s, DemoMsg& m) {
  DemoSound e;
  const int mask = m.u8();
  e.volume = byte_volume((mask & 1) != 0 ? m.u8() : 255);
  e.attenuation = byte_attenuation((mask & 2) != 0 ? m.u8() : 64);
  if ((mask & 8) != 0) {
    e.entity = m.u16();
    e.channel = m.u8();
  } else {
    const int packed = m.u16();
    e.entity = packed >> 3;
    e.channel = packed & 7;
  }
  e.sound = (mask & 16) != 0 ? m.u16() : m.u8();
  const float x = coord(m), y = coord(m), z = coord(m);
  e.origin = {x, y, z};
  if (!m.bad) s.started.push_back(e);
}

bool temp_entity(DemoMsg& m, DemoState& s, std::string& error) {
  DemoEffect e;
  e.code = m.u8();
  const auto point = [&] {
    const float x = coord(m), y = coord(m);
    return sh::Vec3f{x, y, coord(m)};
  };
  switch (e.code) {
    case 0:  // the spikes
    case 1:
    case 7:
    case 8:
      e.kind = DemoEffectKind::spike;
      e.count = e.code == 1 ? 2 : 1;
      break;
    case 2:
      e.kind = DemoEffectKind::gunshot;
      break;
    case 3:  // the explosions (12: one in a colour range)
    case 4:
    case 12:
      e.kind = DemoEffectKind::explosion;
      break;
    case 10:
      e.kind = DemoEffectKind::lava;
      break;
    case 11:
      e.kind = DemoEffectKind::teleport;
      break;
    case 5:  // the beams: an entity and two points
    case 6:
    case 9:
    case 13:
      e.kind = DemoEffectKind::beam;
      e.entity = m.i16();
      break;
    default:
      error =
          "a temporary entity of kind " + std::to_string(e.code) + " is not one the protocol has";
      return false;
  }
  e.at = point();
  e.end = e.kind == DemoEffectKind::beam ? point() : e.at;
  if (e.code == 12) m.skip(2);  // its colour range
  if (!m.bad) s.effects.push_back(e);
  return true;
}

}  // namespace

bool read_single_messages(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  m.single_flags = d.single_flags;
  while (!m.done() && !m.bad) {
    const size_t at = m.at;
    const int id = m.u8();
    bool ok = true;
    if ((id & 0x80) != 0) {
      ok = entity_update(d, m, id, error);
    } else {
      switch (id) {
        case kNop:
          break;
        case kDisconnect:
          d.ended = true;
          return true;
        case kUpdateStat: {
          const int k = m.u8();
          const int32_t v = m.i32();
          if (k >= 0 && k < kStatCount) s.stats[k] = v;
          break;
        }
        case kVersion:
          m.i32();
          break;
        case kSetView:
          s.view_entity = m.i16();
          break;
        case kSound:
          sound(s, m);
          break;
        case kTime:
          s.time = m.f32();
          break;
        case kPrint: {  // no level in this protocol: a chat line starts with byte 1
          std::string text = m.str();
          const bool chat = !text.empty() && text[0] == 1;
          if (chat) text.erase(0, 1);
          s.prints.push_back({chat ? 3 : 2, std::move(text)});
          break;
        }
        case kCenterPrint:
        case kFinale:
        case kCutscene:
          s.centre = m.str();
          break;
        case kStuffText:
          m.str();
          break;
        case kSetAngle:
          s.angles = {angle(m), angle(m), angle(m)};
          break;
        case kServerInfo:
          ok = server_info(d, m, error);
          break;
        case kLightStyle:
        case kUpdateName:
          m.u8();
          m.str();
          break;
        case kUpdateFrags:
          m.u8();
          m.i16();
          break;
        case kClientData:
          client_data(s, m);
          break;
        case kStopSound: {
          const int packed = m.u16();
          s.stopped.emplace_back(packed >> 3, packed & 7);
          break;
        }
        case kUpdateColors: {  // a slot's colours: the shirt's row high, the trousers' low
          const int slot = m.u8(), colours = m.u8();
          if (slot >= 0 && static_cast<size_t>(slot) < s.top.size()) {
            s.top[static_cast<size_t>(slot)] = std::min(colours >> 4, 13);
            s.bottom[static_cast<size_t>(slot)] = std::min(colours & 15, 13);
          }
          break;
        }
        case kParticle:
          coord(m);
          coord(m);
          coord(m);
          m.skip(3 + 2);
          break;
        case kDamage: {  // the armour, the blood, where it came from
          DemoBlow b;
          b.armor = m.u8();
          b.blood = m.u8();
          const float x = coord(m), y = coord(m), z = coord(m);
          b.from = {x, y, z};
          take_blow(s, b);
          break;
        }
        case kSpawnStatic:
          s.statics.push_back(read_spawn(m));
          break;
        case kSpawnStatic2: {
          const int flags = m.u8();
          s.statics.push_back(read_spawn(m, flags));
          break;
        }
        case kSpawnBaseline:
        case kSpawnBaseline2: {
          const int n = m.i16();
          if (n < 0 || n >= kMostEntities) {
            error = "a baseline names entity " + std::to_string(n);
            ok = false;
            break;
          }
          const int flags = id == kSpawnBaseline2 ? m.u8() : 0;
          slot(d.baselines, n) = read_spawn(m, flags);
          break;
        }
        case kTempEntity:
          ok = temp_entity(m, d.state, error);
          break;
        case kSetPause:
        case kSignonNum:
          m.u8();
          break;
        case kKilledMonster:  // the level's count of kills, kept by the client
          s.stats[kStatMonsters]++;
          break;
        case kFoundSecret:  // and of secrets found
          s.stats[kStatSecrets]++;
          break;
        case kSellScreen:
          break;
        case kSpawnStaticSound:
        case kSpawnStaticSound2: {
          DemoStaticSound e;
          const float x = coord(m), y = coord(m), z = coord(m);
          e.origin = {x, y, z};
          e.sound = id == kSpawnStaticSound2 ? m.u16() : m.u8();
          e.volume = byte_volume(m.u8());
          e.attenuation = byte_attenuation(m.u8());
          s.static_sounds.push_back(e);
          break;
        }
        case kIntermission:
          s.intermission = true;
          break;
        case kCdTrack:
          m.skip(2);
          break;
        case kSkybox:
        case kAchievement:
          m.str();
          break;
        case kBonusFlash:
          break;
        case kFog:  // density, red, green, blue, the fade's time
          m.skip(4 + 2);
          break;
        case kLocalSound: {  // a sound for this player alone
          const int mask = m.u8();
          DemoSound e;
          e.sound = (mask & 16) != 0 ? m.u16() : m.u8();
          e.origin = s.eye;
          if (!m.bad) s.started.push_back(e);
          break;
        }
        default:
          error = "message id " + std::to_string(id) +
                  (id == kBad || id == kSpawnBinary ? " is never valid"
                                                    : " is not one the protocol has");
          ok = false;
      }
    }
    if (!ok || m.bad) {
      if (ok) error = "message id " + std::to_string(id) + " runs past the block's end";
      error += " (message at offset " + std::to_string(at) +
               "): the demo is damaged or in another protocol";
      return false;
    }
  }
  return true;
}

}  // namespace shooters_edge::client
