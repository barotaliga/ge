// A SINGLE-PLAYER DEMO (`.dem`, protocol 15) CONVERTED TO A MULTIVIEW RECORDING (`.mvd`) as its
// blocks arrive (demo_convert.hpp): each block's messages read in order and written as the
// multiplayer protocol's, so a client that plays recordings of matches plays the demo, and a
// broadcast serves it.
//
// THE SOURCE, as demo.hpp reads it: the music track as text and a newline, then blocks of an i32
// length, three f32 view angles (degrees) and the messages.
//
// THE NUMBERS: the players are entities 1 to the level's most clients in the source, and slots 0
// up in the target (a slot the entity's number less one); every other entity is renumbered past
// the target's 32 player slots (n - most clients + 32), in every message that names one (an
// update, a baseline, a sound, a stopped sound, a beam). An entity renumbered to 512 or more has
// no place in the target's entity list and is dropped.
//
// THE SIGNON: the level's start (`serverinfo`) becomes the server data (protocol 28, the game
// folder `ConvertOptions::game`, the level's title and the movement settings of a deathmatch
// server), the server info stuffed as `fullserverinfo` (most clients, deathmatch or coop off the
// game type, the map), the model and sound lists in chunks, then what the source sent before its
// first frame (the baselines, the statics, the light styles, the names) as records of time 0, and
// a stuffed `skins`, after which a client draws.
//
// A FRAME is a block that carries the server's time (`time`): one record to everyone of its
// milliseconds since the last frame (at least 1; a gap over 255 is written as records of 255
// first; the level's first frame 0, so a reader reads the signon and it as one frame; a block at
// the same server time as the last one 0, so it joins that frame), each player whose entity the
// block updated in the recorder's player form (origin, view angles, model, skin, effects, the held
// weapon's frame; only the fields that changed since he was last written), the entity list as a
// delta against the last one written (against the baselines at a level's first frame), then the
// block's other messages; then a stats record for the recorder's slot with the stats that changed
// (from `clientdata` and `updatestat`).
//
// WHAT IS REWRITTEN (`ConvertNotes::rewritten`): a print gets the multiplayer protocol's level
// (chat, off its leading byte 1; else high); a name or a colour change becomes the slot's user
// info; the intermission names the view's place and angles; the music message keeps its track
// alone; the cutscene's text becomes the finale's; the view set (`setangle`) names the recorder's
// slot; another player's view pitch is his body's times -3 (the body leans a third of the view);
// the recorder's own view is the block's view angles; a gunshot gets a count of 1; blood thrown as
// particles of the blood colours (73 and 225) becomes the blood effects, the count over 20.
//
// WHAT IS DROPPED (`ConvertNotes::dropped`): the version, the sign-on stage numbers, the
// signon's stuffed commands (`cmd ...`, `reconnect`) a client must not run from a recording,
// other particles, the coloured explosion's colour range (it is a plain explosion), the beam
// effect, the view height and the punch angles, an entity numbered past the list, a view set on
// an entity that is not a player.
//
// ge:summary A single-player demo converted to a multiview recording block by block, as it is read
// ge:keywords demo-convert dem mvd single-to-multiview protocol-15 protocol-28 renumber signon
// ge:keywords fullserverinfo playerinfo deltapacketentities clientdata stats-record tv-demo
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "shooters_edge/client/demo_convert.hpp"

namespace shooters_edge::client {

namespace {

// The target's record kinds.
constexpr int kRecSingle = 4, kRecStats = 5, kRecAll = 6;
constexpr int kMostTarget = 512;  // the target's entity numbers end before this
constexpr int kSlots = 32;
constexpr size_t kListChunk = 1024;  // a list message's names before the next chunk, bytes
constexpr int32_t kNoStat = INT32_MIN;

struct Out {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void i32(int32_t v) {
    const auto u = static_cast<uint32_t>(v);
    for (int i = 0; i < 4; i++) u8(static_cast<int>((u >> (8 * i)) & 0xFF));
  }
  void f32(float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof u);
    i32(static_cast<int32_t>(u));
  }
  void str(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
  }
  void bytes(std::span<const uint8_t> s) { b.insert(b.end(), s.begin(), s.end()); }
  bool empty() const { return b.empty(); }
};

// An entity's state as the wire carries it: the bytes, so nothing is rounded twice.
struct Ent {
  int model = 0, frame = 0, colormap = 0, skin = 0, effects = 0;
  std::array<int16_t, 3> origin{};
  std::array<int8_t, 3> angles{};
  bool operator==(const Ent&) const = default;
};

// A player as last written in the recorder's form.
struct Written {
  bool any = false;
  std::array<int16_t, 3> origin{};
  std::array<int, 3> angles{};
  int model = 0, skin = 0, effects = 0, weapon_frame = 0;
};

// The source's entity-update flags (demo_single.cpp's table).
enum : uint16_t {
  kSrcMoreBits = 1 << 0,
  kSrcOrigin1 = 1 << 1,
  kSrcOrigin2 = 1 << 2,
  kSrcOrigin3 = 1 << 3,
  kSrcAngle2 = 1 << 4,
  kSrcFrame = 1 << 6,
  kSrcAngle1 = 1 << 8,
  kSrcAngle3 = 1 << 9,
  kSrcModel = 1 << 10,
  kSrcColormap = 1 << 11,
  kSrcSkin = 1 << 12,
  kSrcEffects = 1 << 13,
  kSrcLongEntity = 1 << 14,
};

// The target's entity-delta flags (demo_client.cpp's table): the word's, then the more byte's.
enum : uint16_t {
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
};

// The target's player-form bits.
constexpr int kMvOrigin = 1, kMvAngles = 8, kMvEffects = 64, kMvSkin = 128, kMvDead = 256;
constexpr int kMvWeaponFrame = 1024, kMvModel = 2048;

// The clientdata flags (demo_single.cpp's).
enum : uint16_t {
  kCdViewHeight = 1 << 0,
  kCdIdealPitch = 1 << 1,
  kCdPunch1 = 1 << 2,
  kCdVelocity1 = 1 << 5,
  kCdItems = 1 << 9,
  kCdWeaponFrame = 1 << 12,
  kCdArmour = 1 << 13,
  kCdWeapon = 1 << 14,
};

int angle16_of(float deg) {
  return static_cast<int>(std::lround(deg * 65536.0f / 360.0f)) & 0xFFFF;
}

struct SingleToMultiview final : ConvertStream {
  bool header = false;   // the music line read
  bool level = false;    // a level's signon written
  bool framed = false;   // the level's first frame written
  bool stopped = false;  // a disconnect read: the rest is ignored
  bool end_written = false;
  int max_clients = 1;
  int pov = 0;  // the recorder's slot
  int view_entity = 1;
  uint8_t seq = 0;  // the entity lists' sequence byte
  std::string map, title;
  std::vector<Ent> baselines;           // by source number
  std::map<int, Ent> updated;           // this block's entity updates, by source number
  std::map<int, Ent> last_list;         // the last list written, by target number
  std::array<Written, kSlots> players;  // by slot
  std::array<std::string, kSlots> names;
  std::array<int, kSlots> colours{};
  std::array<int32_t, 32> stats{}, stats_written{};
  int weapon_frame = 0;
  std::array<float, 3> view{};  // the block's view angles, degrees
  // The clock: the source's time at the level's first frame, the milliseconds written since.
  double t0 = 0, now = 0, last_now = 0;
  int64_t ms_written = 0;
  bool timed = false;

  int target_number(int n) const { return n <= max_clients ? n : n - max_clients + kSlots; }

  void drop(DemoConvert& c, const char* what, int64_t n = 1) { c.notes.dropped[what] += n; }
  void rewrite(DemoConvert& c, const char* what) { c.notes.rewritten[what]++; }

  static void record(std::vector<uint8_t>& out, int ms, int kind, int target, const Out& body) {
    Out o;
    o.u8(ms);
    o.u8(kind | target << 3);
    o.i32(static_cast<int32_t>(body.b.size()));
    o.bytes(body.b);
    out.insert(out.end(), o.b.begin(), o.b.end());
  }

  std::string user_info(int slot) const {
    const int c = colours[static_cast<size_t>(slot)];
    return "\\name\\" + names[static_cast<size_t>(slot)] + "\\topcolor\\" +
           std::to_string(c >> 4 & 15) + "\\bottomcolor\\" + std::to_string(c & 15);
  }

  void user_info_msg(Out& m, int slot) const {
    m.u8(40);  // updateuserinfo: slot, user id, the info
    m.u8(slot);
    m.i32(slot + 1);
    m.str(user_info(slot));
  }

  static void name_lists(std::vector<uint8_t>& out, int id, const std::vector<std::string>& names) {
    size_t i = 0;
    do {
      Out m;
      m.u8(id);
      m.u8(static_cast<int>(i));
      size_t bytes = 0;
      for (; i < names.size() && i < 255 && bytes < kListChunk; i++) {
        m.str(names[i]);
        bytes += names[i].size() + 1;
      }
      m.str("");
      m.u8(i < names.size() && i < 255 ? static_cast<int>(i) : 0);
      record(out, 0, kRecAll, 0, m);
    } while (i < names.size() && i < 255);
  }

  // The level's start: the server data and the lists.
  bool server_info(DemoConvert& c, DemoMsg& m, std::vector<uint8_t>& out, std::string& error) {
    const int32_t protocol = m.i32();
    if (protocol != 15) {
      error = "the level starts in protocol " + std::to_string(protocol) +
              ", and only protocol 15 demos convert";
      return false;
    }
    max_clients = std::clamp(m.u8(), 1, kSlots);
    const int game_type = m.u8();
    title = m.str();
    std::vector<std::string> models, sounds;
    for (std::string s = m.str(); !s.empty() && !m.bad; s = m.str()) models.push_back(s);
    for (std::string s = m.str(); !s.empty() && !m.bad; s = m.str()) sounds.push_back(s);
    map = !models.empty() ? map_of_model(models[0]) : "";
    level = true;
    framed = false;
    timed = false;
    pov = 0;
    view_entity = 1;
    baselines.clear();
    last_list.clear();
    players = {};
    stats.fill(0);
    stats_written.fill(kNoStat);
    c.notes.levels++;
    Out sd;
    sd.u8(11);  // serverdata: protocol, server count, game folder, time, title, movement
    sd.i32(28);
    sd.i32(static_cast<int32_t>(c.notes.levels));
    sd.str(c.options.game);
    sd.f32(0);
    sd.str(title);
    static constexpr float kMove[10] = {800, 100, 320, 500, 10, 10, 10, 4, 4, 1};
    for (float f : kMove) sd.f32(f);
    sd.u8(9);  // stufftext: the server's info
    sd.str("fullserverinfo \"\\maxclients\\" + std::to_string(max_clients) + "\\deathmatch\\" +
           std::to_string(game_type != 0 ? 1 : 0) + "\\coop\\" +
           std::to_string(game_type == 0 && max_clients > 1 ? 1 : 0) + "\\*gamedir\\" +
           c.options.game + "\\map\\" + map + "\"\n");
    record(out, 0, kRecAll, 0, sd);
    name_lists(out, 46, sounds);  // soundlist
    name_lists(out, 45, models);  // modellist
    return !m.bad;
  }

  bool entity_update(DemoConvert& c, DemoMsg& m, int first, std::string& error) {
    int bits = first & 0x7F;
    if ((bits & kSrcMoreBits) != 0) bits |= m.u8() << 8;
    const int n = (bits & kSrcLongEntity) != 0 ? m.u16() : m.u8();
    if (n <= 0 || n >= 8192) {
      error = "an entity update names entity " + std::to_string(n);
      return false;
    }
    Ent e = static_cast<size_t>(n) < baselines.size() ? baselines[static_cast<size_t>(n)] : Ent{};
    if ((bits & kSrcModel) != 0) e.model = m.u8();
    if ((bits & kSrcFrame) != 0) e.frame = m.u8();
    if ((bits & kSrcColormap) != 0) e.colormap = m.u8();
    if ((bits & kSrcSkin) != 0) e.skin = m.u8();
    if ((bits & kSrcEffects) != 0) e.effects = m.u8();
    const int ob[3] = {kSrcOrigin1, kSrcOrigin2, kSrcOrigin3};
    const int ab[3] = {kSrcAngle1, kSrcAngle2, kSrcAngle3};
    for (int i = 0; i < 3; i++) {
      if ((bits & ob[i]) != 0) e.origin[static_cast<size_t>(i)] = static_cast<int16_t>(m.i16());
      if ((bits & ab[i]) != 0) e.angles[static_cast<size_t>(i)] = static_cast<int8_t>(m.i8());
    }
    (void)c;
    updated[n] = e;
    return true;
  }

  static Ent read_spawn(DemoMsg& m) {
    Ent e;
    e.model = m.u8();
    e.frame = m.u8();
    e.colormap = m.u8();
    e.skin = m.u8();
    for (size_t i = 0; i < 3; i++) {
      e.origin[i] = static_cast<int16_t>(m.i16());
      e.angles[i] = static_cast<int8_t>(m.i8());
    }
    return e;
  }

  static void write_spawn(Out& o, const Ent& e) {
    o.u8(e.model);
    o.u8(e.frame);
    o.u8(e.colormap);
    o.u8(e.skin);
    for (size_t i = 0; i < 3; i++) {
      o.i16(e.origin[i]);
      o.u8(static_cast<uint8_t>(e.angles[i]));
    }
  }

  // An entity's delta from `from` to `to` under target number `q`; nothing when it did not change
  // and `always` is off.
  static void write_delta(Out& o, int q, const Ent& from, const Ent& to, bool always) {
    int bits = 0;
    if (to.origin[0] != from.origin[0]) bits |= kOrigin1;
    if (to.origin[1] != from.origin[1]) bits |= kOrigin2;
    if (to.origin[2] != from.origin[2]) bits |= kOrigin3;
    if (to.angles[0] != from.angles[0]) bits |= kAngle1;
    if (to.angles[1] != from.angles[1]) bits |= kAngle2;
    if (to.angles[2] != from.angles[2]) bits |= kAngle3;
    if (to.frame != from.frame) bits |= kFrame;
    if (to.model != from.model) bits |= kModel;
    if (to.colormap != from.colormap) bits |= kColormap;
    if (to.skin != from.skin) bits |= kSkin;
    if (to.effects != from.effects) bits |= kEffects;
    if (bits == 0 && !always) return;
    const int more = bits & 0xFF;
    if (more != 0) bits |= kMoreBits;
    o.i16(q | (bits & 0xFE00));
    if (more != 0) o.u8(more);
    if ((bits & kModel) != 0) o.u8(to.model);
    if ((bits & kFrame) != 0) o.u8(to.frame);
    if ((bits & kColormap) != 0) o.u8(to.colormap);
    if ((bits & kSkin) != 0) o.u8(to.skin);
    if ((bits & kEffects) != 0) o.u8(to.effects);
    const int ob[3] = {kOrigin1, kOrigin2, kOrigin3};
    const int ab[3] = {kAngle1, kAngle2, kAngle3};
    for (size_t i = 0; i < 3; i++) {
      if ((bits & ob[i]) != 0) o.i16(to.origin[i]);
      if ((bits & ab[i]) != 0) o.u8(static_cast<uint8_t>(to.angles[i]));
    }
  }

  const Ent& baseline_target(int q) const {
    static const Ent kNone;
    const int n = q < kSlots ? q : q - kSlots + max_clients;
    return static_cast<size_t>(n) < baselines.size() ? baselines[static_cast<size_t>(n)] : kNone;
  }

  // The frame's players and entity list into `m`.
  void frame_state(DemoConvert& c, Out& m) {
    for (const auto& [n, e] : updated) {
      if (n > max_clients) continue;
      const int slot = n - 1;
      Written& w = players[static_cast<size_t>(slot)];
      Written cur;
      cur.any = true;
      cur.origin = e.origin;
      if (slot == pov) {
        for (size_t i = 0; i < 3; i++) cur.angles[i] = angle16_of(view[i]);
      } else {
        cur.angles[0] = angle16_of(static_cast<float>(e.angles[0]) * (360.0f / 256.0f) * -3.0f);
        cur.angles[1] = angle16_of(static_cast<float>(e.angles[1]) * (360.0f / 256.0f));
        cur.angles[2] = angle16_of(static_cast<float>(e.angles[2]) * (360.0f / 256.0f));
        rewrite(c, "another player's view pitch from his body's");
      }
      cur.model = e.model;
      cur.skin = e.skin;
      cur.effects = e.effects;
      cur.weapon_frame = slot == pov ? weapon_frame : 0;
      int bits = 0;
      for (size_t i = 0; i < 3; i++) {
        if (!w.any || cur.origin[i] != w.origin[i]) bits |= kMvOrigin << i;
        if (!w.any || cur.angles[i] != w.angles[i]) bits |= kMvAngles << i;
      }
      if (!w.any || cur.model != w.model) bits |= kMvModel;
      if (!w.any || cur.skin != w.skin) bits |= kMvSkin;
      if (!w.any || cur.effects != w.effects) bits |= kMvEffects;
      if (!w.any || cur.weapon_frame != w.weapon_frame) bits |= kMvWeaponFrame;
      if (slot == pov && stats[kStatHealth] <= 0) bits |= kMvDead;
      m.u8(42);  // playerinfo, the recorder's form
      m.u8(slot);
      m.i16(bits);
      m.u8(e.frame);
      for (size_t i = 0; i < 3; i++)
        if ((bits & (kMvOrigin << i)) != 0) m.i16(cur.origin[i]);
      for (size_t i = 0; i < 3; i++)
        if ((bits & (kMvAngles << i)) != 0) m.i16(cur.angles[i]);
      if ((bits & kMvModel) != 0) m.u8(cur.model);
      if ((bits & kMvSkin) != 0) m.u8(cur.skin);
      if ((bits & kMvEffects) != 0) m.u8(cur.effects);
      if ((bits & kMvWeaponFrame) != 0) m.u8(cur.weapon_frame);
      w = cur;
    }
    std::map<int, Ent> list;
    for (const auto& [n, e] : updated) {
      if (n <= max_clients) continue;
      const int q = target_number(n);
      if (q >= kMostTarget) {
        drop(c, "an entity numbered past the list's end");
        continue;
      }
      list.emplace(q, e);
    }
    if (!framed) {
      m.u8(47);  // packetentities: the whole list from the baselines
      for (const auto& [q, e] : list) write_delta(m, q, baseline_target(q), e, true);
    } else {
      m.u8(48);  // deltapacketentities: against the last list
      m.u8(seq);
      auto old = last_list.begin();
      for (const auto& [q, e] : list) {
        for (; old != last_list.end() && old->first < q; ++old) m.i16(old->first | kRemove);
        if (old != last_list.end() && old->first == q) {
          write_delta(m, q, old->second, e, false);
          ++old;
        } else {
          write_delta(m, q, baseline_target(q), e, true);
        }
      }
      for (; old != last_list.end(); ++old) m.i16(old->first | kRemove);
    }
    m.i16(0);
    seq++;
    last_list = std::move(list);
  }

  void stats_record(std::vector<uint8_t>& out) {
    Out st;
    for (size_t k = 0; k < stats.size(); k++) {
      if (stats[k] == stats_written[k]) continue;
      stats_written[k] = stats[k];
      if (stats[k] >= 0 && stats[k] < 256) {
        st.u8(3);  // updatestat
        st.u8(static_cast<int>(k));
        st.u8(stats[k]);
      } else {
        st.u8(38);  // updatestatlong
        st.u8(static_cast<int>(k));
        st.i32(stats[k]);
      }
    }
    if (!st.empty()) record(out, 0, kRecStats, pov, st);
  }

  // The milliseconds of a frame at `now`: records of 255 first for a long gap.
  int frame_ms(std::vector<uint8_t>& out) {
    if (!timed) {
      t0 = last_now = now;
      timed = true;
      ms_written = 0;
      return 0;  // the signon and the first frame read as one
    }
    if (now <= last_now) return 0;  // the same server frame again: it joins the last one
    last_now = now;
    const int64_t want = std::llround((now - t0) * 1000.0);
    int64_t ms = want - ms_written;
    while (ms > 255) {
      record(out, 255, kRecAll, 0, Out{});
      ms_written += 255;
      ms -= 255;
    }
    ms = std::max<int64_t>(ms, 1);
    ms_written += ms;
    return static_cast<int>(ms);
  }

  // A stuffed text's lines, those a recording may run kept.
  std::string kept_lines(DemoConvert& c, const std::string& text) {
    std::string kept;
    size_t at = 0;
    while (at < text.size()) {
      size_t nl = text.find('\n', at);
      if (nl == std::string::npos) nl = text.size() - 1;
      const std::string line = text.substr(at, nl - at + 1);
      at = nl + 1;
      if (line.starts_with("cmd ") || line.starts_with("reconnect") ||
          line.starts_with("changing")) {
        drop(c, "a signon command stuffed to the client");
        continue;
      }
      kept += line;
    }
    return kept;
  }

  // One block's messages, written.
  bool block(DemoConvert& c, std::span<const uint8_t> body, std::vector<uint8_t>& out,
             std::string& error) {
    DemoMsg m{body};
    Out all, single;
    bool has_time = false;
    updated.clear();
    while (!m.done() && !m.bad && !stopped) {
      const size_t at = m.at;
      const int id = m.u8();
      bool ok = true;
      if ((id & 0x80) != 0) {
        ok = entity_update(c, m, id, error);
      } else {
        switch (id) {
          case 1:  // nop
            break;
          case 2:  // disconnect
            stopped = true;
            break;
          case 3: {  // updatestat: a byte and a long
            const int k = m.u8();
            const int32_t v = m.i32();
            if (k >= 0 && k < 32) stats[static_cast<size_t>(k)] = v;
            break;
          }
          case 4:  // version
            m.i32();
            drop(c, "the protocol's version message");
            break;
          case 5: {  // setview
            view_entity = m.u16();
            if (view_entity >= 1 && view_entity <= max_clients)
              pov = view_entity - 1;
            else
              drop(c, "a view set on an entity that is not a player");
            break;
          }
          case 6: {  // sound
            const int mask = m.u8();
            const int volume = (mask & 1) != 0 ? m.u8() : -1;
            const int atten = (mask & 2) != 0 ? m.u8() : -1;
            const int packed = m.u16();
            const int sound = m.u8();
            std::array<int, 3> p{};
            for (int& v : p) v = m.i16();
            const int q = target_number(packed >> 3);
            if (q >= 1024) {
              drop(c, "a sound on an entity numbered past the list's end");
              break;
            }
            all.u8(6);
            all.i16((q << 3 | (packed & 7)) | (volume >= 0 ? 1 << 15 : 0) |
                    (atten >= 0 ? 1 << 14 : 0));
            if (volume >= 0) all.u8(volume);
            if (atten >= 0) all.u8(atten);
            all.u8(sound);
            for (int v : p) all.i16(v);
            break;
          }
          case 7:  // time
            now = m.f32();
            has_time = true;
            break;
          case 8: {  // print: a level now; chat off its leading byte 1
            std::string text = m.str();
            const bool chat = !text.empty() && text[0] == 1;
            if (chat) text.erase(0, 1);
            all.u8(8);
            all.u8(chat ? 3 : 2);
            all.str(text);
            rewrite(c, "a print given the multiplayer protocol's level");
            break;
          }
          case 9: {  // stufftext
            const std::string kept = kept_lines(c, m.str());
            if (!kept.empty()) {
              single.u8(9);
              single.str(kept);
            }
            break;
          }
          case 10: {  // setangle: the recorder's slot first
            all.u8(10);
            all.u8(pov);
            for (int i = 0; i < 3; i++) all.u8(m.u8());
            rewrite(c, "a view set named for the recorder's slot");
            break;
          }
          case 11:
            if (!server_info(c, m, out, error)) {
              error += " (message at offset " + std::to_string(at) + ")";
              return false;
            }
            break;
          case 12: {  // lightstyle
            const int k = m.u8();
            all.u8(12);
            all.u8(k);
            all.str(m.str());
            break;
          }
          case 13: {  // updatename -> the slot's user info
            const int slot = m.u8();
            const std::string name = m.str();
            if (slot >= 0 && slot < kSlots) {
              names[static_cast<size_t>(slot)] = name;
              user_info_msg(all, slot);
              rewrite(c, "a name change as the slot's user info");
            }
            break;
          }
          case 14: {  // updatefrags
            const int slot = m.u8();
            const int frags = m.i16();
            all.u8(14);
            all.u8(slot);
            all.i16(frags);
            break;
          }
          case 15: {  // clientdata -> the recorder's stats and weapon frame
            const int bits = m.u16();
            if ((bits & kCdViewHeight) != 0) m.i8();
            if ((bits & kCdIdealPitch) != 0) m.i8();
            for (int i = 0; i < 3; i++) {
              if ((bits & (kCdPunch1 << i)) != 0) m.i8();
              if ((bits & (kCdVelocity1 << i)) != 0) m.i8();
            }
            if ((bits & kCdItems) != 0) stats[kStatItems] = m.i32();
            weapon_frame = (bits & kCdWeaponFrame) != 0 ? m.u8() : 0;
            stats[kStatArmour] = (bits & kCdArmour) != 0 ? m.u8() : 0;
            stats[kStatWeapon] = (bits & kCdWeapon) != 0 ? m.u8() : 0;
            stats[kStatHealth] = m.i16();
            stats[kStatAmmo] = m.u8();
            stats[kStatShells] = m.u8();
            stats[kStatNails] = m.u8();
            stats[kStatRockets] = m.u8();
            stats[kStatCells] = m.u8();
            stats[kStatActiveWeapon] = m.u8();
            break;
          }
          case 16: {  // stopsound
            const int packed = m.u16();
            const int q = target_number(packed >> 3);
            if (q < 1024) {
              all.u8(16);
              all.i16(q << 3 | (packed & 7));
            }
            break;
          }
          case 17: {  // updatecolors -> the slot's user info
            const int slot = m.u8();
            const int colours_byte = m.u8();
            if (slot >= 0 && slot < kSlots) {
              colours[static_cast<size_t>(slot)] = colours_byte;
              user_info_msg(all, slot);
              rewrite(c, "a colour change as the slot's user info");
            }
            break;
          }
          case 18: {  // particle: blood's colours become the blood effects
            std::array<int, 3> p{};
            for (int& v : p) v = m.i16();
            m.skip(3);  // the direction
            const int count = m.u8();
            const int colour = m.u8();
            if (colour == 73) {
              all.u8(23);
              all.u8(12);  // blood: a count and a point
              all.u8(std::clamp(count / 20, 1, 255));
              for (int v : p) all.i16(v);
              rewrite(c, "blood thrown as particles as the blood effect");
            } else if (colour == 225) {
              all.u8(23);
              all.u8(13);  // the beam's blood: a point
              for (int v : p) all.i16(v);
              rewrite(c, "blood thrown as particles as the blood effect");
            } else {
              drop(c, "particles");
            }
            break;
          }
          case 19:  // damage: the recorder's alone
            single.u8(19);
            single.u8(m.u8());
            single.u8(m.u8());
            for (int i = 0; i < 3; i++) single.i16(m.i16());
            break;
          case 20: {  // spawnstatic
            const Ent e = read_spawn(m);
            all.u8(20);
            write_spawn(all, e);
            break;
          }
          case 22: {  // spawnbaseline
            const int n = m.u16();
            const Ent e = read_spawn(m);
            if (n >= 8192) {
              error = "a baseline names entity " + std::to_string(n);
              return false;
            }
            if (static_cast<size_t>(n) >= baselines.size())
              baselines.resize(static_cast<size_t>(n) + 1);
            baselines[static_cast<size_t>(n)] = e;
            const int q = target_number(n);
            if (q >= kMostTarget) {
              drop(c, "a baseline numbered past the list's end");
              break;
            }
            all.u8(22);
            all.i16(q);
            write_spawn(all, e);
            break;
          }
          case 23:
            if (!temp_entity(c, m, all, error)) {
              error += " (message at offset " + std::to_string(at) + ")";
              return false;
            }
            break;
          case 24:  // setpause
            all.u8(24);
            all.u8(m.u8());
            break;
          case 25:  // signonnum
            m.u8();
            drop(c, "the sign-on stage numbers");
            break;
          case 26:  // centerprint: the recorder's alone
            single.u8(26);
            single.str(m.str());
            break;
          case 27:  // killedmonster
          case 28:  // foundsecret
          case 33:  // sellscreen
            all.u8(id);
            break;
          case 29: {  // spawnstaticsound: the point, sound, volume, attenuation
            all.u8(29);
            for (int i = 0; i < 3; i++) all.i16(m.i16());
            all.u8(m.u8());
            all.u8(m.u8());
            all.u8(m.u8());
            break;
          }
          case 30: {  // intermission: the view's place and angles now
            all.u8(30);
            const auto it = updated.find(view_entity);
            const std::array<int16_t, 3> o =
                it != updated.end() ? it->second.origin : players[static_cast<size_t>(pov)].origin;
            for (size_t i = 0; i < 3; i++) all.i16(o[i]);
            for (size_t i = 0; i < 3; i++)
              all.u8(static_cast<int>(std::lround(view[i] * 256.0f / 360.0f)) & 255);
            rewrite(c, "the intermission given the view's place");
            break;
          }
          case 31:  // finale
          case 34:  // cutscene -> the finale
            all.u8(31);
            all.str(m.str());
            if (id == 34) rewrite(c, "a cutscene's text as the finale's");
            break;
          case 32: {  // cdtrack: the track and the loop -> the track
            const int track = m.u8();
            m.u8();
            all.u8(32);
            all.u8(track);
            rewrite(c, "the music message without its loop track");
            break;
          }
          default:
            error = "message id " + std::to_string(id) + " is not one protocol 15 has";
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
    if (!level) {
      if (!all.empty() || !single.empty() || !updated.empty())
        drop(c, "messages before the level's start");
      return true;
    }
    if (has_time || !updated.empty()) {
      if (!framed) {
        Out skins;
        skins.u8(9);
        skins.str("skins\n");  // a client draws from here
        record(out, 0, kRecAll, 0, skins);
      }
      Out frame;
      frame_state(c, frame);
      frame.bytes(all.b);
      const int ms = has_time ? frame_ms(out) : 0;
      record(out, ms, kRecAll, 0, frame);
      framed = true;
      if (!single.empty()) record(out, 0, kRecSingle, pov, single);
      stats_record(out);
      c.notes.frames++;
    } else {
      if (!all.empty()) record(out, 0, kRecAll, 0, all);
      if (!single.empty()) record(out, 0, kRecSingle, pov, single);
      if (framed) stats_record(out);
    }
    return true;
  }

  bool temp_entity(DemoConvert& c, DemoMsg& m, Out& o, std::string& error) {
    const int code = m.u8();
    auto point = [&] {
      for (int i = 0; i < 3; i++) o.i16(m.i16());
    };
    auto skip_point = [&] {
      for (int i = 0; i < 3; i++) m.i16();
    };
    switch (code) {
      case 0:  // the spikes, the explosions, the splash, the teleport: a point
      case 1:
      case 3:
      case 4:
      case 7:
      case 8:
      case 10:
      case 11:
        o.u8(23);
        o.u8(code);
        point();
        return true;
      case 2:  // the gunshot: a count of 1 here
        o.u8(23);
        o.u8(2);
        o.u8(1);
        point();
        rewrite(c, "a gunshot given a count");
        return true;
      case 5:  // the bolts: an entity and two points
      case 6:
      case 9: {
        const int q = target_number(m.u16());
        o.u8(23);
        o.u8(code);
        o.i16(q);
        point();
        point();
        return true;
      }
      case 12:  // the explosion in a colour range: a plain explosion
        o.u8(23);
        o.u8(3);
        point();
        m.skip(2);
        drop(c, "the coloured explosion's colours");
        return true;
      case 13:  // the beam
        m.u16();
        skip_point();
        skip_point();
        drop(c, "the beam effect");
        return true;
      default:
        error =
            "a temporary entity of kind " + std::to_string(code) + " is not one the protocol has";
        return false;
    }
  }

  bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) override {
    const std::span<const uint8_t> b(c.in);
    size_t pos = 0;
    if (!header) {
      const auto nl = std::find(b.begin(), b.end(), static_cast<uint8_t>('\n'));
      if (nl == b.end()) {
        if (c.ended) {
          error = "the demo ends before its music line";
          return false;
        }
        return true;
      }
      pos = static_cast<size_t>(nl - b.begin()) + 1;
      header = true;
    }
    while (!stopped && pos + 16 <= b.size()) {
      const uint32_t len = static_cast<uint32_t>(b[pos]) | static_cast<uint32_t>(b[pos + 1]) << 8 |
                           static_cast<uint32_t>(b[pos + 2]) << 16 |
                           static_cast<uint32_t>(b[pos + 3]) << 24;
      if (len > (1u << 20)) {
        error = "a block at byte " + std::to_string(pos) + " says it is " + std::to_string(len) +
                " bytes long: the demo is damaged";
        return false;
      }
      if (pos + 16 + len > b.size()) break;
      for (size_t i = 0; i < 3; i++) {
        float f = 0;
        std::memcpy(&f, &b[pos + 4 + 4 * i], 4);
        view[i] = f;
      }
      if (!block(c, b.subspan(pos + 16, len), out, error)) {
        error += " (in the block at byte " + std::to_string(pos) + ")";
        return false;
      }
      pos += 16 + len;
    }
    if (stopped) pos = b.size();
    c.in.erase(c.in.begin(), c.in.begin() + static_cast<std::ptrdiff_t>(pos));
    if (c.ended && !end_written) {
      if (!c.in.empty()) drop(c, "a block cut short at the end");
      c.in.clear();
      if (!c.options.no_end && level) {
        Out end;
        end.u8(2);  // disconnect
        end.str("EndOfDemo");
        record(out, framed ? 1 : 0, kRecAll, 0, end);  // its own frame: the last one reads whole
      }
      end_written = true;
    }
    return true;
  }
};

}  // namespace

std::unique_ptr<ConvertStream> single_to_multiview_stream() {
  return std::make_unique<SingleToMultiview>();
}

}  // namespace shooters_edge::client
