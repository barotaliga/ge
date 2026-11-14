// A MULTIVIEW RECORDING (`.mvd`) CONVERTED TO A CLIENT RECORDING OF ONE PLAYER'S VIEW as its
// records arrive (demo_convert.hpp), written by demo_convert_client.hpp for the oldest clients.
//
// THE SOURCE: records of a milliseconds byte, a kind and a body (demo_multiview.cpp); a frame is
// every record from one whose milliseconds are not 0 to the next. Each record is read by the
// window's own reader (`next_multiview`, its message spans kept), so the stats of every player
// and the messages the view's player saw (to everyone, to a set of players that holds him, to him
// alone) are known; the players and the entity list are read here from the same messages, so a
// field the reader does not keep (an entity's colormap) is kept.
//
// THE VIEW: `ConvertOptions::pov` (the lowest playing slot by default) or `convert_set_pov`. The
// whole recording is read whether or not a view is chosen, so a view chosen partway (a TV that
// converts a player's view only while someone watches it) starts at the next frame with a whole
// signon (the level's own, then every light style, name, score and static sent since, kept by
// what they set) and a whole frame; a view changed starts the same way.
//
// WHAT IS REWRITTEN OR DROPPED: the server data in the client's form (the view's slot as the
// player); the view's angles from his player state into the command records (a view set names a
// player first in the source and none in the target, so it is dropped: the angles carry it); the
// view's velocity from his origin frame to frame; a stuffed line a client must not run from a
// recording; a message the oldest client does not read (`ConvertNotes::dropped`). A recording
// that uses the protocol's extensions is refused: its messages are laid out otherwise.
//
// ge:summary A multiview recording converted to a client recording of one view, startable midway
// ge:keywords demo-convert mvd multiview-to-client one-view pov convert_set_pov lazy-view tv
// ge:keywords player-view switch-view signon-replay
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/demo_convert_client.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kRecCommand = 0, kRecRead = 1, kRecSet = 2, kRecMultiple = 3, kRecSingle = 4;
constexpr int kRecStats = 5, kRecAll = 6;
constexpr double kListsLater = 0.05;  // the model list's packet after the server's info
constexpr uint32_t kMostBody = 1u << 20;
constexpr int kSlots = 32;

// The entity list's flags.
constexpr int kOrigin1 = 1 << 9, kOrigin2 = 1 << 10, kOrigin3 = 1 << 11, kAngle2 = 1 << 12;
constexpr int kFrame = 1 << 13, kRemove = 1 << 14, kMoreBits = 1 << 15;
constexpr int kAngle1 = 1 << 0, kAngle3 = 1 << 1, kModel = 1 << 2, kColormap = 1 << 3;
constexpr int kSkin = 1 << 4, kEffects = 1 << 5;

// The multiview player form's bits.
constexpr int kMvAngles = 8, kMvEffects = 64, kMvSkin = 128, kMvDead = 256, kMvGib = 512;
constexpr int kMvWeaponFrame = 1024, kMvModel = 2048;
constexpr uint32_t kMvdFloatCoords = 0x1;  // the multiview family's extension bits

uint32_t u32_at(std::span<const uint8_t> b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}

struct Record {
  int ms = 0, kind = 0, target = 0;
  uint32_t mask = 0;
  size_t body = 0, size = 0, next = 0;
};

// The record at `at`: 1 read, 0 the bytes end inside it, -1 not a record.
int record_at(std::span<const uint8_t> b, size_t at, Record& r) {
  if (at + 2 > b.size()) return 0;
  r.ms = b[at];
  r.kind = b[at + 1] & 7;
  r.target = b[at + 1] >> 3;
  at += 2;
  if (r.kind == kRecSet) {
    if (at + 8 > b.size()) return 0;
    r.body = at;
    r.size = 8;
    r.next = at + 8;
    return 1;
  }
  if (r.kind == kRecCommand || r.kind > kRecAll) return -1;
  if (r.kind == kRecMultiple) {
    if (at + 4 > b.size()) return 0;
    r.mask = u32_at(b, at);
    at += 4;
  }
  if (at + 4 > b.size()) return 0;
  const uint32_t len = u32_at(b, at);
  if (len > kMostBody) return -1;
  if (at + 4 + len > b.size()) return 0;
  r.body = at + 4;
  r.size = len;
  r.next = r.body + len;
  return 1;
}

// A player as the source's frames have set him.
struct Player {
  bool present = false;
  std::array<int16_t, 3> origin{};
  std::array<int, 3> angles{};  // 16-bit
  int frame = 0, model = 0, skin = 0, effects = 0, weapon_frame = 0;
  bool dead = false, gib = false;
};

// Whether the oldest client reads message `id` (the ones it refuses end its playback).
bool old_client_reads(int id) {
  switch (id) {
    case 0:
    case 4:
    case 5:
    case 7:
    case 13:
    case 15:
    case 17:
    case 18:
    case 21:
    case 25:
      return false;
    default:
      return id < 54;
  }
}

struct MultiviewToClient final : ConvertStream {
  Demo d;
  std::vector<DemoSpan> spans;
  ClientRecording rec;
  bool begun = false, stopped = false, end_written = false;
  // The level.
  bool level = false, in_signon = false;
  int levels = 0;
  std::string title;
  ConvMessages signon;     // its signon but the server data
  size_t signon_late = 0;  // its messages from the model list's end
  std::array<float, 10> movevars{};
  std::map<int, std::vector<uint8_t>> keyed;   // what changed since, by what it sets
  std::vector<std::vector<uint8_t>> appended;  // statics, static sounds, infos since
  std::map<int, ConvEnt> baselines, list;      // by number
  std::array<Player, kSlots> players{};
  // The view.
  int pov = -1, written_level = -1;
  ConvMessages events;  // the frame's messages the view saw
  bool frame_pending = false;
  double t_level = 0, level_base = 0, clock = 0, shift = 0;
  bool level_timed = false;
  std::array<int16_t, 3> last_origin{};
  double last_t = 0;
  bool have_last = false;

  void drop(DemoConvert& c, const char* what) { c.notes.dropped[what]++; }

  static int i16_at(std::span<const uint8_t> b, size_t at) {
    return static_cast<int16_t>(static_cast<uint16_t>(b[at] | b[at + 1] << 8));
  }

  // The server data: the title, and the plain protocol checked.
  bool server_data(DemoConvert& c, std::span<const uint8_t> m, std::string& error) {
    // The extensions' tags before the protocol: those that lay the messages out otherwise (the
    // other family's, coordinates as floats) are refused; the multiview family's others are not.
    size_t at = 1;
    while (at + 8 <= m.size() && u32_at(m, at) != 28) at += 8;
    if (at + 8 > m.size() || d.ext_fte != 0 || (d.ext_mvd & kMvdFloatCoords) != 0) {
      error =
          "the recording uses protocol extensions that lay its messages out otherwise, which a "
          "client recording for the oldest clients cannot carry yet";
      return false;
    }
    at += 8;                                   // the protocol, the spawn count
    while (at < m.size() && m[at] != 0) at++;  // the game folder
    at += 1 + 4;                               // its end, the server's time
    title.clear();
    while (at < m.size() && m[at] != 0) title.push_back(static_cast<char>(m[at++]));
    at++;
    for (size_t i = 0; i < movevars.size(); i++, at += 4) {
      const uint32_t v = at + 4 <= m.size() ? u32_at(m, at) : 0;
      movevars[i] = std::bit_cast<float>(v);
    }
    level = true;
    in_signon = true;
    levels++;
    c.notes.levels++;
    signon.clear();
    signon_late = 0;
    keyed.clear();
    appended.clear();
    baselines.clear();
    list.clear();
    players = {};
    level_timed = false;
    level_base = levels > 1 ? clock + 0.1 : clock;
    have_last = false;
    return true;
  }

  static ConvEnt read_spawn(std::span<const uint8_t> m, size_t at) {
    ConvEnt e;
    e.model = m[at];
    e.frame = m[at + 1];
    e.colormap = m[at + 2];
    e.skin = m[at + 3];
    for (size_t i = 0; i < 3; i++) {
      e.origin[i] = static_cast<int16_t>(i16_at(m, at + 4 + 3 * i));
      e.angles[i] = static_cast<int8_t>(m[at + 6 + 3 * i]);
    }
    return e;
  }

  // A full (47) or delta (48) entity list onto the current one.
  void entity_list(std::span<const uint8_t> m, bool delta) {
    size_t at = delta ? 2 : 1;
    std::map<int, ConvEnt> to = delta ? list : std::map<int, ConvEnt>{};
    while (at + 2 <= m.size()) {
      const int word = m[at] | m[at + 1] << 8;
      at += 2;
      if (word == 0) break;
      const int n = word & 511;
      // The flags: the word's high bits, the low ones in the byte after it (the word's low bits
      // are the number).
      const int more = (word & kMoreBits) != 0 && at < m.size() ? m[at++] : 0;
      const int bits = (word & ~511) | more;
      if ((bits & kRemove) != 0) {
        to.erase(n);
        continue;
      }
      ConvEnt e;
      if (const auto it = to.find(n); it != to.end())
        e = it->second;
      else if (const auto b = baselines.find(n); b != baselines.end())
        e = b->second;
      auto byte = [&] { return at < m.size() ? m[at++] : 0; };
      auto coord = [&] {
        const int v = at + 2 <= m.size() ? i16_at(m, at) : 0;
        at += 2;
        return static_cast<int16_t>(v);
      };
      if ((bits & kModel) != 0) e.model = byte();
      if ((bits & kFrame) != 0) e.frame = byte();
      if ((bits & kColormap) != 0) e.colormap = byte();
      if ((bits & kSkin) != 0) e.skin = byte();
      if ((bits & kEffects) != 0) e.effects = byte();
      if ((bits & kOrigin1) != 0) e.origin[0] = coord();
      if ((bits & kAngle1) != 0) e.angles[0] = static_cast<int8_t>(byte());
      if ((bits & kOrigin2) != 0) e.origin[1] = coord();
      if ((bits & kAngle2) != 0) e.angles[1] = static_cast<int8_t>(byte());
      if ((bits & kOrigin3) != 0) e.origin[2] = coord();
      if ((bits & kAngle3) != 0) e.angles[2] = static_cast<int8_t>(byte());
      to[n] = e;
    }
    list = std::move(to);
  }

  void player_info(std::span<const uint8_t> m) {
    const int slot = m[1];
    const int bits = m[2] | m[3] << 8;
    if (slot >= kSlots) return;
    Player& p = players[static_cast<size_t>(slot)];
    size_t at = 4;
    p.frame = m[at++];
    for (size_t i = 0; i < 3; i++)
      if ((bits & (1 << i)) != 0) {
        p.origin[i] = static_cast<int16_t>(i16_at(m, at));
        at += 2;
      }
    for (size_t i = 0; i < 3; i++)
      if ((bits & (kMvAngles << i)) != 0) {
        p.angles[i] = i16_at(m, at) & 0xFFFF;
        at += 2;
      }
    p.model = (bits & kMvModel) != 0 ? m[at++] : d.player_model;  // left out: the player model
    if ((bits & kMvSkin) != 0) p.skin = m[at++];
    if ((bits & kMvEffects) != 0) p.effects = m[at++];
    if ((bits & kMvWeaponFrame) != 0) p.weapon_frame = m[at++];
    p.dead = (bits & kMvDead) != 0;
    p.gib = (bits & kMvGib) != 0;
    p.present = true;
  }

  // A stuffed text's lines, those a recording may run kept.
  static std::vector<uint8_t> kept_text(DemoConvert& c, std::span<const uint8_t> m) {
    const std::string text(m.begin() + 1, m.end() - 1);
    std::string kept;
    size_t at = 0;
    while (at < text.size()) {
      size_t nl = text.find('\n', at);
      if (nl == std::string::npos) nl = text.size() - 1;
      const std::string line = text.substr(at, nl - at + 1);
      at = nl + 1;
      if (line.starts_with("skins")) {  // the writer stuffs it after the signon it writes
        c.notes.rewritten["the signon's end written after the signon"]++;
        continue;
      }
      if (line.starts_with("cmd ") || line.starts_with("reconnect") ||
          line.starts_with("changing") || line.starts_with("packet ")) {
        c.notes.dropped["a command stuffed to the client"]++;
        continue;
      }
      kept += line;
    }
    if (kept.empty()) return {};
    std::vector<uint8_t> out{9};
    out.insert(out.end(), kept.begin(), kept.end());
    out.push_back(0);
    return out;
  }

  // One message the view saw, routed.
  bool message(DemoConvert& c, int id, std::span<const uint8_t> m, std::string& error) {
    auto pass = [&](std::span<const uint8_t> bytes) {
      ConvMessages& to = in_signon ? signon : events;
      to.out.bytes(bytes);
      to.mark();
    };
    switch (id) {
      case 11:
        return server_data(c, m, error);
      case 42:
        player_info(m);
        return true;
      case 47:
      case 48:
        in_signon = false;  // the first entity list ends the signon
        entity_list(m, id == 48);
        return true;
      case 3:
      case 38:  // the stats: every player's, read into the reader's table
      case 1:
        return true;
      case 10:
        c.notes.rewritten["a view set carried by the command records' angles"]++;
        return true;
      case 9: {
        const std::vector<uint8_t> kept = kept_text(c, m);
        if (!kept.empty()) pass(kept);
        return true;
      }
      case 22: {
        if (m.size() >= 16) baselines[m[1] | m[2] << 8] = read_spawn(m, 3);
        pass(m);
        if (!in_signon) appended.emplace_back(m.begin(), m.end());
        return true;
      }
      case 45:  // a model list: its last part starts the client's load, a moment later
        pass(m);
        if (in_signon && !m.empty() && m.back() == 0) signon_late = signon.ends.size() - 1;
        return true;
      case 49:  // the view's speed and gravity
      case 50:
        pass(m);
        if (!in_signon) keyed[id << 8].assign(m.begin(), m.end());
        return true;
      case 12:  // a light style, by its number
      case 14:  // a slot's frags, ping, loss, entry time, info: by the slot
      case 36:
      case 37:
      case 53:
      case 40:
        if (id == 40 && m.size() > 6 && m[1] < kSlots && m[6] == 0)
          players[m[1]].present = false;  // the slot emptied
        pass(m);
        if (!in_signon && m.size() > 1) keyed[id << 8 | m[1]].assign(m.begin(), m.end());
        return true;
      case 20:
      case 29:
      case 51:
      case 52:
        pass(m);
        if (!in_signon) appended.emplace_back(m.begin(), m.end());
        return true;
      default:
        if (!old_client_reads(id)) {
          drop(c, "a message the oldest client does not read");
          return true;
        }
        pass(m);
        return true;
    }
  }

  // The slot the view rides now, or -1.
  int view_slot(DemoConvert& c) const {
    if (c.options.pov >= 0)
      return c.options.pov < kSlots && players[static_cast<size_t>(c.options.pov)].present
                 ? c.options.pov
                 : -1;
    if (c.options.pov != -1) return -1;
    if (pov >= 0 && players[static_cast<size_t>(pov)].present) return pov;
    for (int s = 0; s < kSlots; s++)
      if (players[static_cast<size_t>(s)].present && !d.state.spectator[static_cast<size_t>(s)])
        return s;
    return -1;
  }

  void emit_frame(DemoConvert& c, std::vector<uint8_t>& out) {
    if (in_signon) return;
    if (!level_timed) {
      t_level = d.state.time;
      level_timed = true;
    }
    clock = std::max(clock, level_base + 0.1 + shift + (d.state.time - t_level));
    const int want = view_slot(c);
    if (want < 0) {
      events.clear();
      return;
    }
    if (want != pov || written_level != levels) {
      ClientLevel l;
      l.game = d.state.game;
      l.title = title;
      l.player = want;
      l.server_count = levels;
      l.movevars = movevars;
      l.lists = false;  // the source's own signon carries the info and the lists
      rec.level(out, clock, l, c.notes);
      for (const auto& [q, e] : baselines) rec.baseline(q, e);
      // The signon up to the model list's last part, then the rest a moment later.
      ConvMessages early, late;
      size_t from = 0;
      for (size_t i = 0; i < signon.ends.size(); i++) {
        ConvMessages& to = signon_late > 0 && i >= signon_late ? late : early;
        to.out.bytes(std::span<const uint8_t>(signon.out.b).subspan(from, signon.ends[i] - from));
        to.mark();
        from = signon.ends[i];
      }
      rec.signon(out, clock, early);
      rec.flush(out, clock);
      shift += kListsLater;
      clock += kListsLater;
      rec.signon(out, clock, late);
      ConvMessages since;
      for (const auto& m : appended) {
        since.out.bytes(m);
        since.mark();
      }
      for (const auto& [k, m] : keyed) {
        since.out.bytes(m);
        since.mark();
      }
      rec.signon(out, clock, since);
      pov = want;
      written_level = levels;
      have_last = false;
    }
    ClientFrame f;
    f.time = clock;
    for (size_t s = 0; s < players.size(); s++) {
      const Player& p = players[s];
      if (!p.present) continue;
      ClientPlayer& o = f.players[s];
      o.present = true;
      o.origin = p.origin;
      for (size_t i = 0; i < 3; i++)
        o.angles[i] = static_cast<float>(p.angles[i]) * (360.0f / 65536.0f);
      o.frame = p.frame;
      o.model = p.model;
      o.skin = p.skin;
      o.effects = p.effects;
      o.weapon_frame = p.weapon_frame;
      o.dead = p.dead;
      o.gib = p.gib;
    }
    ClientPlayer& me = f.players[static_cast<size_t>(pov)];
    f.view = me.angles;
    // The view's velocity: his origin's change since the last frame.
    const double t = d.state.time;
    if (have_last && t > last_t) {
      for (size_t i = 0; i < 3; i++) {
        const double v = (me.origin[i] - last_origin[i]) / 8.0 / (t - last_t);
        me.velocity[i] = static_cast<int16_t>(std::clamp(std::lround(v), -32767L, 32767L));
      }
    }
    last_origin = me.origin;
    last_t = t;
    have_last = true;
    for (const auto& [q, e] : list) f.entities.emplace(q, e);
    const auto& st = d.player_stats[static_cast<size_t>(pov)];
    std::copy(st.begin(), st.end(), f.stats.begin());
    rec.frame(out, f, events, c.notes);
    events.clear();
    c.notes.frames++;
  }

  bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) override {
    if (!begun) {
      d.state.format = DemoFormat::multiview;
      d.state.view_height = 22;
      d.state.player = -1;
      d.state.players.resize(kSlots);
      d.ring.resize(1);
      d.player_stats.resize(kSlots);
      d.spans = &spans;
      begun = true;
    }
    const std::span<const uint8_t> b(c.in);
    size_t pos = 0;
    while (!stopped && pos < b.size()) {
      Record r;
      const int got = record_at(b, pos, r);
      if (got < 0) {
        error = "the record at byte " + std::to_string(pos) +
                " is not one a multiview recording has: the file is damaged";
        return false;
      }
      if (got == 0) break;
      if (r.ms != 0 && frame_pending) {
        emit_frame(c, out);
        frame_pending = false;
      }
      // Whom the record is for: every record is read (each player's stats are kept), but only
      // the messages the view saw are passed on.
      const int now = view_slot(c);
      bool seen = r.kind == kRecRead || r.kind == kRecAll;
      int reader = now;
      if (r.kind == kRecSingle || r.kind == kRecStats) {
        reader = r.target;
        seen = r.kind == kRecSingle && r.target == now;
      } else if (r.kind == kRecMultiple) {
        seen = now >= 0 && (r.mask >> now & 1u) != 0;
        if (!seen && r.mask != 0) reader = std::countr_zero(r.mask);
      }
      d.state.player = reader;
      // The record alone, read by the window's reader, which keeps its state between records.
      d.bytes.assign(b.begin() + static_cast<std::ptrdiff_t>(pos),
                     b.begin() + static_cast<std::ptrdiff_t>(r.next));
      d.pos = 0;
      d.pending = 0;
      spans.clear();
      DemoState& s = d.state;
      s.started.clear();
      s.stopped.clear();
      s.voice.clear();
      s.prints.clear();
      s.effects.clear();
      s.centre.clear();
      if (next_multiview(d, error) == DemoRead::error) {
        error += " (in the record at byte " + std::to_string(pos) + ")";
        return false;
      }
      if (seen) {
        const std::span<const uint8_t> body = b.subspan(r.body, r.size);
        for (const DemoSpan& sp : spans)
          if (!message(c, sp.id, body.subspan(sp.at, sp.end - sp.at), error)) {
            error += " (in the record at byte " + std::to_string(pos) + ")";
            return false;
          }
      }
      if (d.ended)
        stopped = true;  // the end's record ends the frame before it, adding none
      else if (level)
        frame_pending = true;
      pos = r.next;
    }
    if (stopped) pos = b.size();
    c.in.erase(c.in.begin(), c.in.begin() + static_cast<std::ptrdiff_t>(pos));
    if ((c.ended || stopped) && !end_written) {
      if (frame_pending) {
        emit_frame(c, out);
        frame_pending = false;
      }
      if (!stopped && !c.in.empty()) drop(c, "a record cut short at the end");
      c.in.clear();
      if (written_level >= 0) rec.end(out, clock, !c.options.no_end, c.notes);
      end_written = true;
    }
    return true;
  }
};

}  // namespace

std::unique_ptr<ConvertStream> multiview_to_client_stream() {
  return std::make_unique<MultiviewToClient>();
}

void convert_set_pov(DemoConvert& c, int slot) { c.options.pov = slot; }

}  // namespace shooters_edge::client
