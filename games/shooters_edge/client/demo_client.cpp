// The multiplayer protocol's messages (version 28) as a client recording's packets and a
// multiview recording's records carry them (demo.hpp): every message read to its end so the next
// one is found, and the few that matter kept: the server data (the protocol's extensions, the
// recording player's slot), the model list's first entry (the map), the players' origins, the
// entity lists against their baselines and earlier frames, the stats, the intermission.
// `docs/shooters-edge.md` § Demos lists what is read; the protocol's own tables are the protocol
// note's (M18g).
//
// EXTENSIONS: the server data may open with extension families (a 4-byte tag and a bit set
// each) before the version. The ones that change how a message reads are honoured: wider
// coordinates (every coordinate a float and every angle 16 bits; or only origins as floats),
// the teleport kind byte on a view set, the entity number's two high bits, the 16-bit model
// index, an entity's transparency and colour bytes, the extended sound, the entity-delta forms
// of statics and baselines. A message id outside the table is refused with its number.
//
// ge:summary The multiplayer protocol's messages read off a client recording's packets
// ge:keywords multiplayer protocol-28 serverdata modellist playerinfo packet-entities delta
// ge:keywords extensions float-coords baseline updatestat intermission read_client_messages
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include <algorithm>
#include <string>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace {

// A `\key\value\key\value` info string's value for `key` ("" when absent).
std::string info_field(const std::string& info, const std::string& key) {
  size_t at = 0;
  while (at < info.size() && info[at] == '\\') {
    const size_t k_end = info.find('\\', at + 1);
    if (k_end == std::string::npos) return "";
    const size_t v_end = std::min(info.find('\\', k_end + 1), info.size());
    if (info.compare(at + 1, k_end - at - 1, key) == 0)
      return info.substr(k_end + 1, v_end - k_end - 1);
    at = v_end;
  }
  return "";
}

// A colour key's value (`topcolor`, `bottomcolor`) as the palette's colour row, 0..13: its
// leading digits, held to the rows a player can wear; 0 when it has none.
int colour_of(const std::string& v) {
  int n = 0;
  for (char ch : v) {
    if (ch < '0' || ch > '9' || n > 99) break;
    n = n * 10 + (ch - '0');
  }
  return std::clamp(n, 0, 13);
}

// Extension families' tags, the four letters read little-endian.
constexpr uint32_t kFamilyFte = 0x58455446, kFamilyFte2 = 0x32455446, kFamilyMvd = 0x3144564D;
// Their bits that change how a message reads.
constexpr uint32_t kFteTrans = 0x8, kFteFloatCoords = 0x8000, kFteChunkedDownloads = 0x20000000;
constexpr size_t kChunkBytes = 1024;  // a chunked download's chunk
constexpr uint32_t kMvdFloatCoords = 0x1, kMvdHighLagTeleport = 0x2;

constexpr int kMostEntities = 2048;

// An entity delta's flag word: the number in bits 0..8, these above it; the more-bits byte's
// flags land in bits 0..7 once the number is masked off, the even-more byte's in 16..23.
enum : int {
  kOrigin1 = 1 << 9,
  kOrigin2 = 1 << 10,
  kOrigin3 = 1 << 11,
  kAngle2 = 1 << 12,
  kFrame = 1 << 13,
  kRemove = 1 << 14,
  kMoreBits = 1 << 15,
  kAngle1 = 1 << 0,
  kAngle3 = 1 << 1,
  kModel = 1 << 2,
  kColormap = 1 << 3,
  kSkin = 1 << 4,
  kEffects = 1 << 5,
  kEvenMore = 1 << 7,
  kTrans = 1 << 17,
  kModelDbl = 1 << 19,
  kEntityPlus512 = 1 << 21,
  kEntityPlus1024 = 1 << 22,
  kYetMore = 1 << 23,
  kColourMod = 1 << 26,
};

struct Wire {
  bool float_coords = false;   // every coordinate a float, every angle 16 bits
  bool float_origins = false;  // origins alone as floats
  bool fte = false;            // the even-more byte may follow
};

Wire wire_of(const Demo& d) {
  return {(d.ext_fte & kFteFloatCoords) != 0, (d.ext_mvd & kMvdFloatCoords) != 0, d.ext_fte != 0};
}

float coord(DemoMsg& m, const Wire& w) {
  return w.float_coords ? m.f32() : static_cast<float>(m.i16()) * 0.125f;
}
float origin(DemoMsg& m, const Wire& w) { return w.float_origins ? m.f32() : coord(m, w); }
float angle(DemoMsg& m, const Wire& w) {
  return w.float_coords ? static_cast<float>(m.i16()) * (360.0f / 65536.0f)
                        : static_cast<float>(m.i8()) * (360.0f / 256.0f);
}
void skip_point(DemoMsg& m, const Wire& w) {
  for (int i = 0; i < 3; i++) coord(m, w);
}
float angle16(DemoMsg& m) { return static_cast<float>(m.i16()) * (360.0f / 65536.0f); }

using Frame = std::vector<std::pair<int, DemoEntity>>;

// The flags of an entity delta after its word (the more, even-more and yet-more bytes), and its
// number with the high bits.
int delta_flags(DemoMsg& m, int word, const Wire& w, int& number) {
  number = word & 511;
  int bits = word & ~511;
  if ((bits & kMoreBits) != 0) bits |= m.u8();
  if (w.fte && (bits & kEvenMore) != 0) bits |= m.u8() << 16;
  if (w.fte && (bits & kYetMore) != 0) bits |= m.u8() << 24;
  if ((bits & kEntityPlus512) != 0) number += 512;
  if ((bits & kEntityPlus1024) != 0) number += 1024;
  return bits;
}

// An entity delta's fields onto `e`.
void delta_fields(DemoMsg& m, int bits, const Wire& w, DemoEntity& e) {
  if ((bits & kModel) != 0)
    e.model = (bits & kModelDbl) != 0 ? m.u16() : m.u8();
  else if ((bits & kModelDbl) != 0)
    e.model = m.u8() + 256;
  if ((bits & kFrame) != 0) e.frame = m.u8();
  if ((bits & kColormap) != 0) m.u8();
  if ((bits & kSkin) != 0) e.skin = m.u8();
  if ((bits & kEffects) != 0) e.effects = m.u8();
  if ((bits & kOrigin1) != 0) e.origin.x = origin(m, w);
  if ((bits & kAngle1) != 0) e.angles.x = angle(m, w);
  if ((bits & kOrigin2) != 0) e.origin.y = origin(m, w);
  if ((bits & kAngle2) != 0) e.angles.y = angle(m, w);
  if ((bits & kOrigin3) != 0) e.origin.z = origin(m, w);
  if ((bits & kAngle3) != 0) e.angles.z = angle(m, w);
  if ((bits & kTrans) != 0) m.u8();
  if ((bits & kColourMod) != 0) m.skip(3);
}

DemoEntity read_spawn(DemoMsg& m, const Wire& w) {
  DemoEntity e;
  e.model = m.u8();
  e.frame = m.u8();
  m.u8();  // colormap
  e.skin = m.u8();
  float o[3], a[3];
  for (int i = 0; i < 3; i++) {
    o[i] = origin(m, w);
    a[i] = angle(m, w);
  }
  e.origin = {o[0], o[1], o[2]};
  e.angles = {a[0], a[1], a[2]};
  return e;
}

const DemoEntity& baseline(const Demo& d, int n) {
  static const DemoEntity kNone;
  return static_cast<size_t>(n) < d.baselines.size() ? d.baselines[static_cast<size_t>(n)] : kNone;
}

// A full list (47: each entity from its baseline) or a delta list (48: from the frame named by
// its sequence byte, an entity not named kept as it was), into the ring's slot for this packet
// and the state's entities.
bool packet_entities(Demo& d, DemoMsg& m, bool delta, std::string& error) {
  const Wire w = wire_of(d);
  // A multiview recording deltas each list against the one written before it, whatever its
  // sequence byte says, so it keeps one slot.
  const bool one = d.state.format == DemoFormat::multiview;
  Frame from;
  if (delta) {
    const int seq = m.u8();
    from = d.ring[one ? 0 : static_cast<size_t>(seq & 63)];
  }
  Frame to;
  size_t old = 0;
  for (;;) {
    const int word = m.u16();
    if (m.bad) break;
    int n = 0;
    const int bits = word == 0 ? 0 : delta_flags(m, word, w, n);
    if (word != 0 && n >= kMostEntities) {
      error = "an entity list names entity " + std::to_string(n);
      return false;
    }
    // A delta list keeps every older entity it does not name, in number order.
    for (; delta && old < from.size() && (word == 0 || from[old].first < n); old++)
      to.push_back(from[old]);
    if (word == 0) break;
    DemoEntity e = baseline(d, n);
    if (delta && old < from.size() && from[old].first == n) e = from[old++].second;
    if ((bits & kRemove) != 0) continue;
    delta_fields(m, bits, w, e);
    to.emplace_back(n, e);
  }
  std::vector<DemoEntity>& ents = d.state.entities;
  for (DemoEntity& e : ents) e.present = false;
  for (auto& [n, e] : to) {
    e.present = true;
    if (static_cast<size_t>(n) >= ents.size()) ents.resize(static_cast<size_t>(n) + 1);
    ents[static_cast<size_t>(n)] = e;
  }
  d.ring[one ? 0 : d.incoming & 63] = std::move(to);
  return true;
}

void player_info(Demo& d, DemoMsg& m) {
  const Wire w = wire_of(d);
  const int slot = m.u8();
  int flags = m.u16();
  if ((d.ext_fte & kFteTrans) != 0 && (flags & 0x8000) != 0) flags |= m.u8() << 16;
  DemoEntity p;
  const float x = origin(m, w), y = origin(m, w), z = origin(m, w);
  p.origin = {x, y, z};
  p.frame = m.u8();
  if ((flags & 0x1) != 0) m.u8();  // msec
  if ((flags & 0x2) != 0) {        // a delta usercmd from the null command
    const int bits = m.u8();
    float a[3] = {0, 0, 0};
    if ((bits & 0x01) != 0) a[0] = angle16(m);
    if ((bits & 0x80) != 0) a[1] = angle16(m);
    if ((bits & 0x02) != 0) a[2] = angle16(m);
    p.angles = {a[0], a[1], a[2]};
    if ((bits & 0x04) != 0) m.i16();
    if ((bits & 0x08) != 0) m.i16();
    if ((bits & 0x10) != 0) m.i16();
    if ((bits & 0x20) != 0) m.u8();
    if ((bits & 0x40) != 0) m.u8();
    m.u8();  // msec
  }
  for (int i = 0; i < 3; i++)
    if ((flags & (0x4 << i)) != 0) m.i16();  // velocity
  p.model = (flags & 0x20) != 0 ? m.u8() : 0;
  if ((flags & 0x40) != 0) p.skin = m.u8();
  if ((flags & 0x80) != 0) p.effects = m.u8();
  if ((flags & 0x100) != 0) m.u8();      // weapon frame
  if ((flags & (1 << 17)) != 0) m.u8();  // transparency
  p.dead = (flags & 0x200) != 0;
  p.gib = (flags & 0x400) != 0;
  p.present = true;
  if (slot >= 0 && slot < 32) d.state.players[static_cast<size_t>(slot)] = p;
}

// A multiview recording's `playerinfo`: the slot, the field bits, the frame, then the fields the
// bits name in the order origin, angles (16 bits each, the pitch the view's own), model, skin,
// effects, weapon frame. A field left out keeps the player's last value; the dead and gib bits
// are the player's state this frame.
void multiview_player(Demo& d, DemoMsg& m) {
  constexpr int kMvAngles = 8;  // 8, 16, 32: the three angles; 1, 2, 4: the three origins
  constexpr int kMvEffects = 64, kMvSkin = 128, kMvDead = 256, kMvGib = 512;
  constexpr int kMvWeaponFrame = 1024, kMvModel = 2048;
  const Wire w = wire_of(d);
  const int slot = m.u8();
  const int bits = m.u16();
  DemoEntity scratch;
  DemoEntity& p = slot >= 0 && slot < 32 ? d.state.players[static_cast<size_t>(slot)] : scratch;
  p.frame = m.u8();
  float* o[3] = {&p.origin.x, &p.origin.y, &p.origin.z};
  float* a[3] = {&p.angles.x, &p.angles.y, &p.angles.z};
  for (int i = 0; i < 3; i++)
    if ((bits & (1 << i)) != 0) *o[i] = origin(m, w);
  for (int i = 0; i < 3; i++)
    if ((bits & (kMvAngles << i)) != 0) *a[i] = angle16(m);
  if ((bits & kMvModel) != 0) p.model = m.u8();
  if ((bits & kMvSkin) != 0) p.skin = m.u8();
  if ((bits & kMvEffects) != 0) p.effects = m.u8();
  if ((bits & kMvWeaponFrame) != 0) m.u8();
  p.dead = (bits & kMvDead) != 0;
  p.gib = (bits & kMvGib) != 0;
  p.present = true;
}

bool server_data(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  d.ext_fte = d.ext_mvd = 0;
  int32_t v = m.i32();
  for (int i = 0; i < 8 && !m.bad; i++, v = m.i32()) {
    const uint32_t tag = static_cast<uint32_t>(v);
    if (tag == kFamilyFte)
      d.ext_fte = static_cast<uint32_t>(m.i32());
    else if (tag == kFamilyMvd)
      d.ext_mvd = static_cast<uint32_t>(m.i32());
    else if (tag == kFamilyFte2)
      m.i32();  // voice among them: its message is read whether or not the bit is set
    else
      break;
  }
  s.protocol = v;
  if (v != 28) {
    error = "the server data names protocol " + std::to_string(v) +
            "; next: only protocol 28 recordings are read";
    return false;
  }
  m.i32();  // the spawn count
  s.game = m.str();
  if (s.format == DemoFormat::multiview) {
    m.f32();  // the server's time where a client's recording has its slot
  } else {
    const int slot = m.u8();
    s.player = slot & 127;  // bit 7: the recorder was a spectator
  }
  m.str();                 // the level's title
  m.skip(size_t{10} * 4);  // the movement settings
  s.levels++;
  s.map.clear();
  s.models.assign(1, "");
  s.sounds.assign(1, "");
  s.intermission = false;
  s.entities.clear();
  s.statics.clear();
  s.static_sounds.clear();
  d.baselines.clear();
  for (Frame& f : d.ring) f.clear();
  for (DemoEntity& p : s.players) p = DemoEntity{};
  for (auto& st : d.player_stats) st.fill(0);
  s.frags.fill(0);
  for (std::string& n : s.names) n.clear();
  return true;
}

void name_list(DemoMsg& m, std::vector<std::string>& list, bool wide_start) {
  size_t at = static_cast<size_t>(wide_start ? m.u16() : m.u8()) + 1;
  for (std::string name = m.str(); !name.empty() && !m.bad; name = m.str(), at++) {
    if (list.size() <= at) list.resize(at + 1);
    list[at] = name;
  }
  m.u8();  // where the next chunk starts, or 0
}

bool temp_entity(DemoMsg& m, const Wire& w, std::string& error) {
  const int kind = m.u8();
  const auto point = [&] { skip_point(m, w); };
  switch (kind) {
    case 2:
    case 12:  // gunshot, blood: a count and a point
      m.u8();
      point();
      return true;
    case 0:
    case 1:
    case 3:
    case 4:
    case 7:
    case 8:
    case 10:
    case 11:
    case 13:
      point();
      return true;
    case 5:
    case 6:
    case 9:  // lightning: an entity and two points
      m.i16();
      point();
      point();
      return true;
    default:
      error = "a temporary entity of kind " + std::to_string(kind) + " is not one the protocol has";
      return false;
  }
}

sh::Vec3f point(DemoMsg& m, const Wire& w) {
  const float x = coord(m, w), y = coord(m, w), z = coord(m, w);
  return {x, y, z};
}

// The sound message: the entity (bits 3..12) and channel (0..2) in a short whose top bits say a
// volume byte and an attenuation byte follow, the sound's index, the point.
void sound(DemoState& s, DemoMsg& m, const Wire& w) {
  DemoSound e;
  const int packed = m.u16();
  e.volume = byte_volume((packed & 0x8000) != 0 ? m.u8() : 255);
  e.attenuation = byte_attenuation((packed & 0x4000) != 0 ? m.u8() : 64);
  e.entity = (packed >> 3) & 1023;
  e.channel = packed & 7;
  e.sound = m.u8();
  e.origin = point(m, w);
  if (!m.bad) s.started.push_back(e);
}

// The sound-double form: a flags byte, the volume and attenuation it names, the entity and
// channel (a short and a byte when wide), the sound's index (a short when wide), the point.
void sound_extended(DemoState& s, DemoMsg& m, const Wire& w) {
  DemoSound e;
  const int flags = m.u8();
  e.volume = byte_volume((flags & 1) != 0 ? m.u8() : 255);
  e.attenuation = byte_attenuation((flags & 2) != 0 ? m.u8() : 64);
  if ((flags & 8) != 0) {
    e.entity = m.u16();
    e.channel = m.u8();
  } else {
    const int packed = m.u16();
    e.entity = packed >> 3;
    e.channel = packed & 7;
  }
  e.sound = (flags & 0x10) != 0 ? m.u16() : m.u8();
  e.origin = point(m, w);
  if (!m.bad) s.started.push_back(e);
}

}  // namespace

bool read_client_messages(Demo& d, DemoMsg& m, std::string& error) {
  DemoState& s = d.state;
  while (!m.done() && !m.bad) {
    const size_t at = m.at;
    const int id = m.u8();
    const Wire w = wire_of(d);
    bool ok = true;
    switch (id) {
      case 1:
        break;  // nop
      case 2:
        d.ended = true;
        return true;  // disconnect
      case 3: {       // updatestat
        const int k = m.u8(), v = m.u8();
        if (k < kStatCount) s.stats[k] = v;
        break;
      }
      case 6:
        sound(s, m, w);
        break;
      case 8: {  // print: a level, the text
        const int level = m.u8();
        s.prints.push_back({level, m.str()});
        break;
      }
      case 26:  // centerprint
        s.centre = m.str();
        break;
      case 9:
      case 31:
        m.str();
        break;    // stufftext, finale
      case 10: {  // setangle; a multiview recording's names the player it turns first
        if (s.format == DemoFormat::multiview) m.u8();
        if ((d.ext_mvd & kMvdHighLagTeleport) != 0) m.u8();
        const float a = angle(m, w), b = angle(m, w), c = angle(m, w);
        s.angles = {a, b, c};
        break;
      }
      case 11:
        ok = server_data(d, m, error);
        break;
      case 12:
        m.u8();
        m.str();
        break;    // lightstyle
      case 14: {  // updatefrags
        const int slot = m.u8();
        const int frags = m.i16();
        if (slot < 32) s.frags[static_cast<size_t>(slot)] = frags;
        break;
      }
      case 16: {  // stopsound
        const int packed = m.u16();
        s.stopped.emplace_back((packed >> 3) & 1023, packed & 7);
        break;
      }
      case 19:
        m.skip(2);
        skip_point(m, w);
        break;  // damage
      case 20:
        s.statics.push_back(read_spawn(m, w));
        break;  // spawnstatic
      case 21:
      case 66: {  // spawnstatic2, spawnbaseline2: an entity delta from the null state
        int n = 0;
        const int bits = delta_flags(m, m.u16(), w, n);
        DemoEntity e;
        delta_fields(m, bits, w, e);
        if (id == 21) s.statics.push_back(e);
        if (id == 66 && n < kMostEntities) {
          if (static_cast<size_t>(n) >= d.baselines.size())
            d.baselines.resize(static_cast<size_t>(n) + 1);
          d.baselines[static_cast<size_t>(n)] = e;
        }
        break;
      }
      case 22: {  // spawnbaseline
        const int n = m.i16();
        const DemoEntity e = read_spawn(m, w);
        if (n >= 0 && n < kMostEntities) {
          if (static_cast<size_t>(n) >= d.baselines.size())
            d.baselines.resize(static_cast<size_t>(n) + 1);
          d.baselines[static_cast<size_t>(n)] = e;
        }
        break;
      }
      case 23:
        ok = temp_entity(m, w, error);
        break;
      case 24:
      case 32:
      case 44:
        m.u8();
        break;  // setpause, cdtrack, chokecount
      case 27:
      case 28:
      case 33:
      case 34:
      case 35:
        break;
      case 29: {  // spawnstaticsound
        DemoStaticSound e;
        e.origin = point(m, w);
        e.sound = m.u8();
        e.volume = byte_volume(m.u8());
        e.attenuation = byte_attenuation(m.u8());
        s.static_sounds.push_back(e);
        break;
      }
      case 30: {  // intermission: the eye and the look
        const float x = coord(m, w), y = coord(m, w), z = coord(m, w);
        const float a = angle(m, w), b = angle(m, w), c = angle(m, w);
        s.eye = {x, y, z};
        s.angles = {a, b, c};
        s.intermission = true;
        break;
      }
      case 36:
        m.u8();
        m.i16();
        break;  // updateping
      case 37:
        m.u8();
        m.f32();
        break;    // updateentertime
      case 38: {  // updatestatlong
        const int k = m.u8();
        const int32_t v = m.i32();
        if (k < kStatCount) s.stats[k] = v;
        break;
      }
      case 39:
        m.i16();
        break;    // muzzleflash
      case 40: {  // updateuserinfo: the slot, the user id, the info string (\\key\\value...)
        const int slot = m.u8();
        m.i32();
        const std::string info = m.str();
        if (slot < 32) {
          const size_t k = static_cast<size_t>(slot);
          s.names[k] = info_field(info, "name");
          s.top[k] = colour_of(info_field(info, "topcolor"));
          s.bottom[k] = colour_of(info_field(info, "bottomcolor"));
        }
        break;
      }
      case 41: {  // download: under chunked downloads a chunk number first (-1: a transfer's start)
        if ((d.ext_fte & kFteChunkedDownloads) != 0) {
          const int32_t chunk = m.i32();
          if (chunk < 0) {
            m.i32();  // the size (or why there is none)
            m.str();  // the file
          } else {
            m.skip(kChunkBytes);
          }
          break;
        }
        const int size = m.i16();
        m.u8();
        if (size > 0) m.skip(static_cast<size_t>(size));
        break;
      }
      case 42:
        if (s.format == DemoFormat::multiview)
          multiview_player(d, m);
        else
          player_info(d, m);
        break;
      case 43:
        m.skip(static_cast<size_t>(m.u8()) * 6);
        break;  // nails
      case 54:
        m.skip(static_cast<size_t>(m.u8()) * 7);
        break;  // nails with their numbers
      case 45:
      case 60:  // modellist (60: a 16-bit start)
        name_list(m, s.models, id == 60);
        if (s.models.size() > 1 && s.map.empty()) s.map = map_of_model(s.models[1]);
        break;
      case 46:
      case 56:
        name_list(m, s.sounds, id == 56);
        break;
      case 47:
      case 48:
        ok = packet_entities(d, m, id == 48, error);
        break;
      case 49:
      case 50:
        m.f32();
        break;    // maxspeed, entgravity
      case 51: {  // setinfo: one key of a slot's info changed
        const int slot = m.u8();
        const std::string key = m.str(), value = m.str();
        if (slot < 32) {
          const size_t k = static_cast<size_t>(slot);
          if (key == "name") s.names[k] = value;
          if (key == "topcolor") s.top[k] = colour_of(value);
          if (key == "bottomcolor") s.bottom[k] = colour_of(value);
        }
        break;
      }
      case 52:
        m.str();
        m.str();
        break;  // serverinfo
      case 53:
        m.skip(2);
        break;  // updatepl
      case 55:
        sound_extended(s, m, w);
        break;
      case 84: {  // voice (the x2 family's voice bit)
        DemoVoice v;
        if (read_voice(m, v)) s.voice.push_back(std::move(v));
        break;
      }
      case 78:
        m.u8();
        m.str();
        break;  // updatestatstring
      case 79:
        m.u8();
        m.f32();
        break;  // updatestatfloat
      default:
        error = "message id " + std::to_string(id) + " is not one this reader knows";
        ok = false;
    }
    if (!ok || m.bad) {
      if (ok) error = "message id " + std::to_string(id) + " runs past the packet's end";
      error += " (message at offset " + std::to_string(at) +
               "); next: the recording is damaged or uses an extension not read yet";
      return false;
    }
  }
  return true;
}

}  // namespace shooters_edge::client
