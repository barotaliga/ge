// A CLIENT RECORDING CONVERTED TO A MULTIVIEW RECORDING (`.mvd`) as its records arrive
// (demo_convert.hpp): one player's recording of a game, written in the form every multiview
// player and the TV read, so a recording a player made plays wherever a server's does.
//
// THE SOURCE: records of a time and a type (a command, a packet, the sequences); each packet is
// read by the window's own reader (`read_client_messages`, its message spans kept), and the
// players and the entity lists are read here from the same messages, so a field the reader does
// not keep (an entity's colormap) is kept; a delta list is resolved against the list of the
// packet its sequence byte names. Commands carry the recorder's view: the angles he is written
// with.
//
// THE TARGET: the server data in the multiview form (the recorder's slot is not in it: a
// multiview recording names no player of its own); the signon's messages as they came; a frame a
// packet: its messages, every player the packet named in the multiview form (his angles from his
// command; the recorder's from his own command records), the entity list (whole at a level's
// first frame, then a delta against the last), the recorder's status as his stats record, the
// view's own messages (a view set, a kick, damage) to him alone.
//
// WHAT IS DROPPED OR REWRITTEN: a stuffed line a client must not run from a recording; the choke
// count; a message the multiview form has no place for. A recording made with the other family's
// extensions is refused: its players and entities are laid out otherwise.
//
// ge:summary A client recording converted to a multiview recording as its records arrive
// ge:keywords demo-convert client-to-multiview recording-to-mvd recorder playerinfo-rewrite
// ge:keywords packetentities deltapacketentities stats-record setangle-slot
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/demo_convert_client.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kRecSingle = 4, kRecStats = 5, kRecAll = 6;
constexpr int kSlots = 32;
constexpr uint32_t kMostPacket = 1u << 16;

// The entity list's flags.
constexpr int kOrigin1 = 1 << 9, kOrigin2 = 1 << 10, kOrigin3 = 1 << 11, kAngle2 = 1 << 12;
constexpr int kFrame = 1 << 13, kRemove = 1 << 14, kMoreBits = 1 << 15;
constexpr int kAngle1 = 1 << 0, kAngle3 = 1 << 1, kModel = 1 << 2, kColormap = 1 << 3;
constexpr int kSkin = 1 << 4, kEffects = 1 << 5;

// The multiview player form's bits.
constexpr int kMvOrigin = 1, kMvAngles = 8, kMvEffects = 64, kMvSkin = 128, kMvDead = 256;
constexpr int kMvGib = 512, kMvWeaponFrame = 1024, kMvModel = 2048;

// The extension tags a server data may open with.
constexpr uint32_t kFamilyFte = 0x58455446, kFamilyFte2 = 0x32455446, kFamilyMvd = 0x3144564D;

uint32_t u32_at(std::span<const uint8_t> b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}
float f32_at(std::span<const uint8_t> b, size_t at) {
  const uint32_t v = u32_at(b, at);
  float f = 0;
  std::memcpy(&f, &v, 4);
  return f;
}
int16_t i16_at(std::span<const uint8_t> b, size_t at) {
  return static_cast<int16_t>(static_cast<uint16_t>(b[at] | b[at + 1] << 8));
}

// A player as the source names him, and as last written.
struct Player {
  bool any = false;
  std::array<int16_t, 3> origin{};
  std::array<int, 3> angles{};  // 16-bit
  int frame = 0, model = 0, skin = 0, effects = 0, weapon_frame = 0;
  bool dead = false, gib = false;
};

// The client records from `at` read with commands of `size` bytes: false when one in the next
// `count` is not a record (the bytes running out is not a failure).
bool chain_ok(std::span<const uint8_t> b, size_t at, int size, int count) {
  for (int i = 0; i < count; i++) {
    if (at + 5 > b.size()) return true;
    const int type = b[at + 4];
    at += 5;
    if (type == 0) {
      at += static_cast<size_t>(size) + 12;
    } else if (type == 2) {
      at += 8;
    } else if (type == 1) {
      if (at + 4 > b.size()) return true;
      const uint32_t len = u32_at(b, at);
      if (len < 8 || len > kMostPacket) return false;
      at += 4 + len;
    } else {
      return false;
    }
  }
  return true;
}

struct ClientToMultiview final : ConvertStream {
  Demo d;
  std::vector<DemoSpan> spans;
  int cmd_size = 0;
  bool begun = false, stopped = false, end_written = false;
  // The level.
  bool level = false, framed = false;
  int pov = 0;  // the recorder's slot
  std::array<float, 3> view{};
  std::map<int, ConvEnt> baselines;
  std::array<std::map<int, ConvEnt>, 64> ring{};  // the lists by their packet's sequence
  std::map<int, ConvEnt> list, last_list;
  bool list_read = false;
  std::array<Player, kSlots> players{}, written{};
  std::array<bool, kSlots> named{};  // in this packet's players
  std::array<int32_t, kSlots> stats_written{};
  bool stats_any = false;
  // The clock.
  bool timed = false;
  double t0 = 0, last_t = 0;
  int64_t ms_written = 0;
  uint8_t seq = 0;

  void drop(DemoConvert& c, const char* what) { c.notes.dropped[what]++; }

  static void record(std::vector<uint8_t>& out, int ms, int kind, int target, const ConvOut& body) {
    ConvOut o;
    o.u8(ms);
    o.u8(kind | target << 3);
    o.i32(static_cast<int32_t>(body.size()));
    o.bytes(body.b);
    out.insert(out.end(), o.b.begin(), o.b.end());
  }

  // The milliseconds of a frame at `t`: records of 255 first for a long gap.
  int frame_ms(std::vector<uint8_t>& out, double t) {
    if (!timed) {  // the first frame a millisecond past the signon, so it reads as a frame
      t0 = t - 0.001;
      last_t = t;
      timed = true;
      ms_written = 1;
      return 1;
    }
    if (t <= last_t) return 0;
    last_t = t;
    int64_t ms = std::llround((t - t0) * 1000.0) - ms_written;
    while (ms > 255) {
      record(out, 255, kRecAll, 0, ConvOut{});
      ms_written += 255;
      ms -= 255;
    }
    ms = std::max<int64_t>(ms, 1);
    ms_written += ms;
    return static_cast<int>(ms);
  }

  // The server data in the multiview form: the extensions' tags as they came, the protocol, the
  // server count, the game folder, the server's time where the recorder's slot was, the rest.
  bool server_data(DemoConvert& c, std::span<const uint8_t> m, ConvOut& o, std::string& error) {
    size_t at = 1;
    o.u8(11);
    while (at + 8 <= m.size() && u32_at(m, at) != 28) {
      const uint32_t tag = u32_at(m, at);
      if (tag == kFamilyFte || (tag != kFamilyFte2 && tag != kFamilyMvd)) {
        error =
            "the recording uses protocol extensions that lay its players and entities out "
            "otherwise, which a multiview recording cannot carry yet";
        return false;
      }
      o.bytes(m.subspan(at, 8));
      at += 8;
    }
    if (at + 8 > m.size()) {
      error = "the server data runs past its packet's end: the file is damaged";
      return false;
    }
    o.bytes(m.subspan(at, 8));  // the protocol, the server count
    at += 8;
    const size_t game = at;
    while (at < m.size() && m[at] != 0) at++;
    o.bytes(m.subspan(game, std::min(at + 1, m.size()) - game));
    at++;
    pov = at < m.size() ? m[at] & 127 : 0;  // a spectator's slot has its top bit set
    at++;
    o.f32(0);
    if (at < m.size()) o.bytes(m.subspan(at));  // the title, the movement
    level = true;
    framed = false;
    list_read = false;
    baselines.clear();
    for (auto& r : ring) r.clear();
    list.clear();
    last_list.clear();
    players = {};
    written = {};
    stats_any = false;
    c.notes.levels++;
    return true;
  }

  static ConvEnt read_spawn(std::span<const uint8_t> m, size_t at) {
    ConvEnt e;
    e.model = m[at];
    e.frame = m[at + 1];
    e.colormap = m[at + 2];
    e.skin = m[at + 3];
    for (size_t i = 0; i < 3; i++) {
      e.origin[i] = i16_at(m, at + 4 + 3 * i);
      e.angles[i] = static_cast<int8_t>(m[at + 6 + 3 * i]);
    }
    return e;
  }

  // A full (47) or delta (48) entity list, kept for the packet's sequence.
  void entity_list(std::span<const uint8_t> m, bool delta) {
    size_t at = 1;
    std::map<int, ConvEnt> to;
    if (delta && at < m.size()) to = ring[m[at++] & 63];
    while (at + 2 <= m.size()) {
      const int word = m[at] | m[at + 1] << 8;
      at += 2;
      if (word == 0) break;
      const int n = word & 511;
      const int more = (word & kMoreBits) != 0 && at < m.size() ? m[at++] : 0;
      const int bits = (word & ~511) | more;
      if ((bits & kRemove) != 0) {
        to.erase(n);
        continue;
      }
      ConvEnt e;
      if (const auto it = to.find(n); delta && it != to.end())
        e = it->second;
      else if (const auto b = baselines.find(n); b != baselines.end())
        e = b->second;
      auto byte = [&] { return at < m.size() ? m[at++] : 0; };
      auto coord = [&] {
        const int16_t v = at + 2 <= m.size() ? i16_at(m, at) : 0;
        at += 2;
        return v;
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
    ring[d.incoming & 63] = to;
    list = std::move(to);
    list_read = true;
  }

  // A player in the client's form.
  void player_info(std::span<const uint8_t> m) {
    if (m.size() < 11) return;
    const int slot = m[1];
    const int flags = m[2] | m[3] << 8;
    if (slot >= kSlots) return;
    Player& p = players[static_cast<size_t>(slot)];
    p.any = true;
    named[static_cast<size_t>(slot)] = true;
    for (size_t i = 0; i < 3; i++) p.origin[i] = i16_at(m, 4 + 2 * i);
    size_t at = 10;
    p.frame = m[at++];
    auto byte = [&] { return at < m.size() ? m[at++] : 0; };
    auto word = [&] {
      const int v = at + 2 <= m.size() ? i16_at(m, at) : 0;
      at += 2;
      return v;
    };
    if ((flags & 0x1) != 0) byte();  // msec
    if ((flags & 0x2) != 0) {        // his command: the angles he is drawn with
      const int bits = byte();
      if ((bits & 0x01) != 0) p.angles[0] = word() & 0xFFFF;
      if ((bits & 0x80) != 0) p.angles[1] = word() & 0xFFFF;
      if ((bits & 0x02) != 0) p.angles[2] = word() & 0xFFFF;
      if ((bits & 0x04) != 0) word();
      if ((bits & 0x08) != 0) word();
      if ((bits & 0x10) != 0) word();
      if ((bits & 0x20) != 0) byte();
      if ((bits & 0x40) != 0) byte();
      byte();  // msec
    }
    for (int i = 0; i < 3; i++)
      if ((flags & (0x4 << i)) != 0) word();  // velocity: the multiview form has none
    p.model = (flags & 0x20) != 0 ? byte() : d.player_model;
    p.skin = (flags & 0x40) != 0 ? byte() : 0;
    p.effects = (flags & 0x80) != 0 ? byte() : 0;
    p.weapon_frame = (flags & 0x100) != 0 ? byte() : 0;
    p.dead = (flags & 0x200) != 0;
    p.gib = (flags & 0x400) != 0;
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

  // The frame's players and entity list in the multiview form.
  void frame_state(ConvOut& m) {
    for (size_t s = 0; s < players.size(); s++) {
      if (!named[s]) continue;
      Player cur = players[s];
      if (static_cast<int>(s) == pov)
        for (size_t i = 0; i < 3; i++) cur.angles[i] = conv_angle16(view[i]) & 0xFFFF;
      Player& w = written[s];
      int bits = 0;
      for (size_t i = 0; i < 3; i++) {
        if (!w.any || cur.origin[i] != w.origin[i]) bits |= kMvOrigin << i;
        if (!w.any || cur.angles[i] != w.angles[i]) bits |= kMvAngles << i;
      }
      if (!w.any || cur.model != w.model) bits |= kMvModel;
      if (!w.any || cur.skin != w.skin) bits |= kMvSkin;
      if (!w.any || cur.effects != w.effects) bits |= kMvEffects;
      if (!w.any || cur.weapon_frame != w.weapon_frame) bits |= kMvWeaponFrame;
      if (cur.dead) bits |= kMvDead;
      if (cur.gib) bits |= kMvGib;
      m.u8(42);
      m.u8(static_cast<int>(s));
      m.i16(bits);
      m.u8(cur.frame);
      for (size_t i = 0; i < 3; i++)
        if ((bits & (kMvOrigin << i)) != 0) m.i16(cur.origin[i]);
      for (size_t i = 0; i < 3; i++)
        if ((bits & (kMvAngles << i)) != 0) m.i16(cur.angles[i]);
      if ((bits & kMvModel) != 0) m.u8(cur.model);
      if ((bits & kMvSkin) != 0) m.u8(cur.skin);
      if ((bits & kMvEffects) != 0) m.u8(cur.effects);
      if ((bits & kMvWeaponFrame) != 0) m.u8(cur.weapon_frame);
      cur.any = true;
      w = cur;
    }
    if (!list_read) return;
    auto base = [&](int q) {
      const auto b = baselines.find(q);
      return b != baselines.end() ? b->second : ConvEnt{};
    };
    if (!framed) {
      m.u8(47);  // the whole list from the baselines
      for (const auto& [q, e] : list) conv_write_delta(m, q, base(q), e, true);
    } else {
      m.u8(48);  // against the last list
      m.u8(seq);
      auto old = last_list.begin();
      for (const auto& [q, e] : list) {
        for (; old != last_list.end() && old->first < q; ++old) m.i16(old->first | kRemove);
        if (old != last_list.end() && old->first == q) {
          conv_write_delta(m, q, old->second, e, false);
          ++old;
        } else {
          conv_write_delta(m, q, base(q), e, true);
        }
      }
      for (; old != last_list.end(); ++old) m.i16(old->first | kRemove);
    }
    m.i16(0);
    seq++;
    last_list = list;
    framed = true;
  }

  // One packet: its messages routed, then its frame written.
  bool packet(DemoConvert& c, std::span<const uint8_t> body, double t, std::vector<uint8_t>& out,
              std::string& error) {
    spans.clear();
    named = {};
    list_read = false;
    DemoState& s = d.state;
    s.started.clear();
    s.stopped.clear();
    s.voice.clear();
    s.prints.clear();
    s.effects.clear();
    s.centre.clear();
    DemoMsg msg{body};
    if (!read_client_messages(d, msg, error)) return false;
    ConvOut all, own;
    for (const DemoSpan& sp : spans) {
      const std::span<const uint8_t> m = body.subspan(sp.at, sp.end - sp.at);
      switch (sp.id) {
        case 11:
          if (!server_data(c, m, all, error)) return false;
          break;
        case 42:
          player_info(m);
          break;
        case 47:
        case 48:
          entity_list(m, sp.id == 48);
          break;
        case 3:
        case 38:  // the recorder's stats: his stats record
        case 1:
          break;
        case 44:
          drop(c, "the choke count");
          break;
        case 22:
          if (m.size() >= 16) baselines[m[1] | m[2] << 8] = read_spawn(m, 3);
          all.bytes(m);
          break;
        case 9:
          all.bytes(kept_text(c, m));
          break;
        case 10:  // a view set: the multiview form names the player first; it turns the view
          for (size_t i = 0; i < 3 && i + 1 < m.size(); i++)
            view[i] = static_cast<float>(m[i + 1]) * (360.0f / 256.0f);
          own.u8(10);
          own.u8(pov);
          own.bytes(m.subspan(1));
          break;
        case 19:  // damage, the kicks: the recorder's own
        case 34:
        case 35:
          own.bytes(m);
          break;
        default:
          all.bytes(m);
      }
    }
    if (!level) {
      if (!all.empty()) drop(c, "a message before the server data");
      return true;
    }
    const bool frame =
        list_read || std::any_of(named.begin(), named.end(), [](bool b) { return b; });
    const int ms = frame ? frame_ms(out, t) : 0;
    if (frame) frame_state(all);
    if (d.ended) stopped = true;  // the disconnect: the end is written at the end
    if (!all.empty() || frame) record(out, ms, kRecAll, 0, all);
    if (!own.empty()) record(out, 0, kRecSingle, pov, own);
    if (!frame) return true;
    // The recorder's status, what changed since the last frame.
    ConvOut st;
    for (size_t k = 0; k < stats_written.size(); k++) {
      const int32_t v = s.stats[k];
      if (stats_any && v == stats_written[k]) continue;
      stats_written[k] = v;
      if (v >= 0 && v < 256) {
        st.u8(3);
        st.u8(static_cast<int>(k));
        st.u8(v);
      } else {
        st.u8(38);
        st.u8(static_cast<int>(k));
        st.i32(v);
      }
    }
    stats_any = true;
    if (!st.empty()) record(out, 0, kRecStats, pov, st);
    c.notes.frames++;
    return true;
  }

  bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) override {
    if (!begun) {
      d.state.format = DemoFormat::client;
      d.state.players.resize(kSlots);
      d.ring.resize(64);
      d.spans = &spans;
      begun = true;
    }
    const std::span<const uint8_t> b(c.in);
    size_t pos = 0;
    while (!stopped && pos + 5 <= b.size()) {
      const double t = f32_at(b, pos);
      const int type = b[pos + 4];
      size_t at = pos + 5;
      if (type == 0) {
        if (cmd_size == 0) {
          if (!c.ended && b.size() - pos < size_t{16} * 64)
            break;  // enough records to tell the size
          cmd_size = chain_ok(b, pos, 24, 16) ? 24 : 28;
        }
        const size_t n = static_cast<size_t>(cmd_size) + 12;
        if (at + n > b.size()) break;
        at += static_cast<size_t>(cmd_size);
        view = {f32_at(b, at), f32_at(b, at + 4), f32_at(b, at + 8)};
        pos = at + 12;
      } else if (type == 2) {
        if (at + 8 > b.size()) break;
        pos = at + 8;
      } else if (type == 1) {
        if (at + 4 > b.size()) break;
        const uint32_t len = u32_at(b, at);
        if (len < 8 || len > kMostPacket) {
          error = "the packet length " + std::to_string(len) + " at byte " + std::to_string(pos) +
                  " is not a packet's: the file is damaged";
          return false;
        }
        if (at + 4 + len > b.size()) break;
        const uint32_t sequence = u32_at(b, at + 4);
        if (sequence != 0xFFFFFFFFu) {  // out of band: nothing for the game
          d.incoming = sequence & 0x7FFFFFFFu;
          if (!packet(c, b.subspan(at + 12, len - 8), t, out, error)) {
            error += " (in the packet at byte " + std::to_string(pos) + ")";
            return false;
          }
        }
        pos = at + 4 + len;
      } else {
        error = "the record type " + std::to_string(type) + " at byte " + std::to_string(pos) +
                " is none of command, packet or sequences: the file is damaged";
        return false;
      }
    }
    if (stopped) pos = b.size();
    c.in.erase(c.in.begin(), c.in.begin() + static_cast<std::ptrdiff_t>(pos));
    if ((c.ended || stopped) && !end_written) {
      if (!stopped && !c.in.empty()) drop(c, "a record cut short at the end");
      c.in.clear();
      if (level && !c.options.no_end) {
        ConvOut e;
        e.u8(2);  // the disconnect, its own frame: the last one reads whole
        e.str("EndOfDemo");
        record(out, framed ? 1 : 0, kRecAll, 0, e);
      }
      end_written = true;
    }
    return true;
  }
};

}  // namespace

std::unique_ptr<ConvertStream> client_to_multiview_stream() {
  return std::make_unique<ClientToMultiview>();
}

}  // namespace shooters_edge::client
