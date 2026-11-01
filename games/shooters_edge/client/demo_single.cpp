// The single-player protocol's messages (version 15) as a demo block carries them (demo.hpp):
// every message read to its end so the next one is found, and the few that matter kept: the
// level's start and its precache lists, the view entity, the entities' baselines and updates,
// the clientdata's view height and status, the clock, the intermission.
//
// THE WIRE: little-endian; a coordinate is an i16 of eighths of a unit, an angle an i8 of 1/256
// of a turn. A message id with bit 7 set is an ENTITY UPDATE (its low 7 bits are its first flag
// byte); every other id is one of the table below. An entity update's fields are deltas from the
// entity's BASELINE (a field not sent is the baseline's), never from the last update.
//
// ge:summary The single-player protocol's messages read off a demo block: level, view, entities
// ge:keywords single-player protocol-15 serverinfo setview clientdata entity-update baseline
// ge:keywords spawnbaseline spawnstatic temp-entity svc read_single_messages
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
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
};

// An entity update's flags: the first byte's low 7 bits, then a second byte when kMoreBits.
enum : uint16_t {
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
};

// clientdata's flags.
enum : uint16_t {
  kViewHeight = 1 << 0,
  kIdealPitch = 1 << 1,
  kPunch1 = 1 << 2,
  kVelocity1 = 1 << 5,
  kItems = 1 << 9,
  kWeaponFrame = 1 << 12,
  kArmour = 1 << 13,
  kWeapon = 1 << 14,
};

constexpr int kMostEntities = 8192;

float coord(DemoMsg& m) { return static_cast<float>(m.i16()) * 0.125f; }
float angle(DemoMsg& m) { return static_cast<float>(m.i8()) * (360.0f / 256.0f); }

DemoEntity& slot(std::vector<DemoEntity>& v, int n) {
  if (static_cast<size_t>(n) >= v.size()) v.resize(static_cast<size_t>(n) + 1);
  return v[static_cast<size_t>(n)];
}

// model, frame, colormap, skin, then origin and angle by axis: a baseline's or a static's body.
DemoEntity read_spawn(DemoMsg& m) {
  DemoEntity e;
  e.model = m.u8();
  e.frame = m.u8();
  m.u8();  // colormap
  e.skin = m.u8();
  float o[3], a[3];
  for (int i = 0; i < 3; i++) {
    o[i] = coord(m);
    a[i] = angle(m);
  }
  e.origin = {o[0], o[1], o[2]};
  e.angles = {a[0], a[1], a[2]};
  return e;
}

bool entity_update(Demo& d, DemoMsg& m, int first, std::string& error) {
  int bits = first & 0x7F;
  if ((bits & kMoreBits) != 0) bits |= m.u8() << 8;
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
  e.present = true;
  slot(d.state.entities, n) = e;
  return true;
}

void client_data(DemoState& s, DemoMsg& m) {
  const int bits = m.u16();
  s.view_height = (bits & kViewHeight) != 0 ? static_cast<float>(m.i8()) : 22.0f;
  if ((bits & kIdealPitch) != 0) m.i8();
  for (int i = 0; i < 3; i++) {
    if ((bits & (kPunch1 << i)) != 0) m.i8();
    if ((bits & (kVelocity1 << i)) != 0) m.i8();
  }
  if ((bits & kItems) != 0) s.stats[kStatItems] = m.i32();
  if ((bits & kWeaponFrame) != 0) m.u8();
  s.stats[kStatArmour] = (bits & kArmour) != 0 ? m.u8() : 0;
  if ((bits & kWeapon) != 0) m.u8();  // the view model's index
  s.stats[kStatHealth] = m.i16();
  s.stats[kStatAmmo] = m.u8();
  s.stats[kStatShells] = m.u8();
  s.stats[kStatNails] = m.u8();
  s.stats[kStatRockets] = m.u8();
  s.stats[kStatCells] = m.u8();
  s.stats[kStatActiveWeapon] = m.u8();
}

bool server_info(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  s.protocol = m.i32();
  if (s.protocol != 15) {
    error = "the level starts in protocol " + std::to_string(s.protocol) +
            "; next: only protocol 15 is read (re-record the demo with a client that writes it)";
    return false;
  }
  m.u8();   // the most clients
  m.u8();   // the game type
  m.str();  // the level's title
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
// a short, the sound's index, the point.
void sound(DemoState& s, DemoMsg& m) {
  DemoSound e;
  const int mask = m.u8();
  e.volume = byte_volume((mask & 1) != 0 ? m.u8() : 255);
  e.attenuation = byte_attenuation((mask & 2) != 0 ? m.u8() : 64);
  const int packed = m.u16();
  e.entity = packed >> 3;
  e.channel = packed & 7;
  e.sound = m.u8();
  const float x = coord(m), y = coord(m), z = coord(m);
  e.origin = {x, y, z};
  if (!m.bad) s.started.push_back(e);
}

bool temp_entity(DemoMsg& m, std::string& error) {
  const int kind = m.u8();
  switch (kind) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
    case 10:
    case 11:
      m.skip(6);  // a point
      return true;
    case 5:
    case 6:
    case 9:
    case 13:
      m.skip(2 + 12);  // an entity and two points
      return true;
    case 12:
      m.skip(6 + 2);  // a point and a colour range
      return true;
    default:
      error = "a temporary entity of kind " + std::to_string(kind) + " is not one the protocol has";
      return false;
  }
}

}  // namespace

bool read_single_messages(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
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
        case kPrint:
        case kStuffText:
        case kCenterPrint:
        case kFinale:
        case kCutscene:
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
        case kUpdateColors:
          m.skip(2);
          break;
        case kParticle:
          m.skip(6 + 3 + 2);
          break;
        case kDamage:
          m.skip(2 + 6);
          break;
        case kSpawnStatic:
          s.statics.push_back(read_spawn(m));
          break;
        case kSpawnBaseline: {
          const int n = m.i16();
          if (n < 0 || n >= kMostEntities) {
            error = "a baseline names entity " + std::to_string(n);
            ok = false;
            break;
          }
          slot(d.baselines, n) = read_spawn(m);
          break;
        }
        case kTempEntity:
          ok = temp_entity(m, error);
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
        case kSpawnStaticSound: {
          DemoStaticSound e;
          const float x = coord(m), y = coord(m), z = coord(m);
          e.origin = {x, y, z};
          e.sound = m.u8();
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
               "); next: the demo is damaged or in another protocol";
      return false;
    }
  }
  return true;
}

}  // namespace shooters_edge::client
