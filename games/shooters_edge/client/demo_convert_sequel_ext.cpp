// AN EXTENDED SEQUEL RECORDING (protocols 3434 to 3436) REWRITTEN AS PROTOCOL 34
// (demo_convert.hpp), block by block after the sequel reader has read it (demo_sequel.cpp), so a
// stock client that takes only protocol 34 plays a recording made on a server with the extended
// limits. The facts are in porting/shooters-edge/demo-formats-sequel.md § 1.6 and § 1.7.
//
// Each message is walked by its own layout and written in protocol 34's. The settings strings
// move to protocol 34's slots (the status bar's program, the models, sounds, images, light
// styles, items, skins and general strings each the first of their list that fits). An entity
// keeps every field protocol 34 has: its origin is the value the reader resolved (an int23 delta
// in the source) as eighths in a short, its models and sound a byte, its solid packed in a short.
// The player's state keeps the same: origin and velocity as shorts, the movement time and flags
// as bytes, the blend whole, the first 32 stats. Every point (a sound's, a temporary entity's, an
// old origin) becomes three shorts. The frame numbers stay, so every delta still names a frame
// the client keeps. A block over protocol 34's 1400 bytes is split between its messages.
//
// WHAT IS DROPPED (`ConvertNotes::dropped`): an entity numbered past 1023, a settings string with
// no slot in protocol 34's lists, alpha, scale and the extra effects, the fog, the stats past 31,
// a sound past the 255th or on an entity past 1023, a muzzle flash past them, and the newer
// temporary entities protocol 34 has no kind for. WHAT IS REWRITTEN: a model past 255 as none, a
// point past a short's reach (4096 units) held at its edge, the lighter rail's line as the rail's,
// the explosions without light as the plain ones.
//
// ge:summary An extended sequel recording (3434 to 3436) rewritten as protocol 34, by message
// ge:keywords demo-convert sequel dm2 extended-limits protocol-3436 protocol-34 transcode int23
// ge:keywords configstring-remap entity-delta playerinfo stats temp-entity block-split
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/client/demo_convert.hpp"

namespace shooters_edge::client {

namespace {

enum : uint8_t {
  kMuzzle = 1,
  kMuzzle2 = 2,
  kTemp = 3,
  kSound = 9,
  kServerData = 12,
  kConfig = 13,
  kBaseline = 14,
  kPlayer = 17,
  kEntities = 18,
  kFrame = 20,
};

constexpr size_t kMostBlock34 = 1400;
constexpr int kMostEntities34 = 1024;

// The entity header's bits.
constexpr uint64_t kOx = 1u << 0, kOy = 1u << 1, kA2 = 1u << 2, kA3 = 1u << 3, kF8 = 1u << 4,
                   kEvent = 1u << 5, kRemove = 1u << 6, kMore1 = 1u << 7, kNum16 = 1u << 8,
                   kOz = 1u << 9, kA1 = 1u << 10, kModel = 1u << 11, kFxLo = 1u << 12,
                   kEffLo = 1u << 14, kMore2 = 1u << 15, kSkinLo = 1u << 16, kF16 = 1u << 17,
                   kFxHi = 1u << 18, kEffHi = 1u << 19, kModel2 = 1u << 20, kModel3 = 1u << 21,
                   kModel4 = 1u << 22, kMore3 = 1u << 23, kOld = 1u << 24, kSkinHi = 1u << 25,
                   kSoundBit = 1u << 26, kSolid = 1u << 27, kModel16 = 1ull << 28,
                   kMoreFxLo = 1ull << 29, kAlpha = 1ull << 30, kMore4 = 1ull << 31,
                   kScale = 1ull << 32, kMoreFxHi = 1ull << 33;

struct Out {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void i32(int32_t v) {
    i16(v & 0xFFFF);
    i16((v >> 16) & 0xFFFF);
  }
  void str(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
};

// The transcode of one block: the reader's state for the values it resolved, the notes.
struct Ext {
  const Demo& d;
  ConvertNotes& notes;
  int ext = 0;  // 1 3434, 2 3435, 3 3436

  // A coordinate in eighths, held in a short.
  int short_of(int32_t eighths) {
    if (eighths > 32767 || eighths < -32768) {
      notes.rewritten["a point past a short's reach held at its edge"]++;
      return std::clamp(eighths, -32768, 32767);
    }
    return eighths;
  }
  int32_t source_coord(DemoMsg& m) {
    if (!m.int23) return m.i16();
    const uint32_t w = static_cast<uint32_t>(m.u16());
    if ((w & 1u) == 0) {  // a delta from 0
      int32_t v = static_cast<int32_t>(w >> 1);
      if (v >= 1 << 14) v -= 1 << 15;
      return v;
    }
    int32_t v = static_cast<int32_t>((w | static_cast<uint32_t>(m.u8()) << 16) >> 1);
    if (v >= 1 << 22) v -= 1 << 23;
    return v;
  }
  void pos(DemoMsg& m, Out& o) {
    for (int i = 0; i < 3; i++) o.i16(short_of(source_coord(m)));
  }

  // A settings string's slot in protocol 34's lists, or -1.
  static int config_slot(int i) {
    struct Range {
      int from, to, at34, size34;
    };
    static constexpr Range kRanges[] = {
        {0, 5, 0, 5},               // the name, the music, the sky and its turn
        {5, 59, 5, 24},             // the status bar's program
        {59, 62, 29, 3},            // the air acceleration, the most clients, the map's checksum
        {62, 8254, 32, 256},        // models
        {8254, 10302, 288, 256},    // sounds
        {10302, 12350, 544, 256},   // images
        {12350, 12606, 800, 256},   // light styles
        {12606, 12862, 1056, 256},  // items
        {12862, 13118, 1312, 256},  // player skins
        {13118, 13630, 1568, 512},  // general
    };
    for (const Range& r : kRanges) {
      if (i < r.from || i >= r.to) continue;
      const int k = i - r.from;
      return k < r.size34 ? r.at34 + k : -1;
    }
    return -1;
  }

  // An entity's header and fields, `origin` (eighths) the value the reader resolved for it.
  bool entity(DemoMsg& m, uint64_t bits, int n, const DemoEntity* resolved, Out& o) {
    const bool keep = n < kMostEntities34;
    if (!keep) notes.dropped["an entity numbered past 1023"]++;
    uint64_t ob = bits & ~(kModel16 | kMoreFxLo | kMoreFxHi | kAlpha | kScale | kMore4 | kMore1 |
                           kMore2 | kMore3);
    Out f;  // the fields, then the header in front
    const bool wide = (bits & kModel16) != 0;
    const auto model = [&](uint64_t bit) {
      if ((bits & bit) == 0) return;
      const int v = wide ? m.u16() : m.u8();
      if (v > 255) notes.rewritten["a model past 255 as none"]++;
      f.u8(v > 255 ? 0 : v);
    };
    const auto copy = [&](size_t n_bytes) {
      for (size_t i = 0; i < n_bytes; i++) f.u8(m.u8());
    };
    const auto sized = [&](uint64_t lo, uint64_t hi) {
      if ((bits & lo) != 0 && (bits & hi) != 0)
        copy(4);
      else if ((bits & lo) != 0)
        copy(1);
      else if ((bits & hi) != 0)
        copy(2);
    };
    if ((bits & kRemove) == 0) {
      model(kModel);
      model(kModel2);
      model(kModel3);
      model(kModel4);
      if ((bits & kF8) != 0) copy(1);
      if ((bits & kF16) != 0) copy(2);
      sized(kSkinLo, kSkinHi);
      sized(kEffLo, kEffHi);
      sized(kFxLo, kFxHi);
      const uint64_t axis[3] = {kOx, kOy, kOz};
      for (int i = 0; i < 3; i++) {
        if ((bits & axis[i]) == 0) continue;
        if (m.int23) {
          source_coord(m);  // a delta from its base: the reader resolved it
          const float v = resolved == nullptr ? 0.0f
                          : i == 0            ? resolved->origin.x
                          : i == 1            ? resolved->origin.y
                                              : resolved->origin.z;
          f.i16(short_of(static_cast<int32_t>(v * 8.0f)));
        } else {
          f.i16(m.i16());
        }
      }
      if ((bits & kA1) != 0) copy(1);
      if ((bits & kA2) != 0) copy(1);
      if ((bits & kA3) != 0) copy(1);
      if ((bits & kOld) != 0) pos(m, f);
      if ((bits & kSoundBit) != 0) {
        const int w = m.u16();
        if ((w & 0x4000) != 0) m.u8();
        if ((w & 0x8000) != 0) m.u8();
        const int index = w & 0x3FFF;
        if (index > 255) {
          notes.dropped["an entity's sound past the 255th"]++;
          ob &= ~kSoundBit;
        } else {
          f.u8(index);
        }
        if ((w & 0xC000) != 0) notes.dropped["an entity's loop volume and attenuation"]++;
      }
      if ((bits & kEvent) != 0) copy(1);
      if ((bits & kSolid) != 0) {
        // The long's bytes (half width, half depth, the depth below, the height above plus 32) as
        // protocol 34's short: the half width, the depth below and the height above plus 32, in
        // eights of 5, 5 and 6 bits; a brush model's 31 stays 31.
        const uint32_t v = static_cast<uint32_t>(m.i32());
        if (v == 31) {
          f.i16(31);
        } else {
          const int x = std::min(static_cast<int>(v & 0xFF) / 8, 31);
          const int zd = std::min(static_cast<int>((v >> 16) & 0xFF) / 8, 31);
          const int zu = std::min(static_cast<int>((v >> 24) & 0xFF) / 8, 63);
          f.i16(x | zd << 5 | zu << 10);
        }
      }
      if ((bits & (kMoreFxLo | kMoreFxHi)) != 0) {
        if ((bits & kMoreFxLo) != 0 && (bits & kMoreFxHi) != 0)
          m.skip(4);
        else
          m.skip((bits & kMoreFxLo) != 0 ? 1 : 2);
        notes.dropped["an entity's extra effects"]++;
      }
      if ((bits & kAlpha) != 0) {
        m.u8();
        notes.dropped["an entity's alpha"]++;
      }
      if ((bits & kScale) != 0) {
        m.u8();
        notes.dropped["an entity's scale"]++;
      }
    }
    if (!keep) return !m.bad;
    if (n > 255) ob |= kNum16;
    if ((ob & 0xFF000000u) != 0) ob |= kMore3;
    if ((ob & 0xFFFF0000u) != 0) ob |= kMore2;
    if ((ob & 0xFFFFFF00u) != 0) ob |= kMore1;
    o.u8(static_cast<int>(ob & 0xFF));
    if ((ob & kMore1) != 0) o.u8(static_cast<int>(ob >> 8 & 0xFF));
    if ((ob & kMore2) != 0) o.u8(static_cast<int>(ob >> 16 & 0xFF));
    if ((ob & kMore3) != 0) o.u8(static_cast<int>(ob >> 24 & 0xFF));
    if ((ob & kNum16) != 0)
      o.i16(n);
    else
      o.u8(n);
    o.b.insert(o.b.end(), f.b.begin(), f.b.end());
    return !m.bad;
  }

  uint64_t header(DemoMsg& m, int& n) {
    uint64_t bits = static_cast<uint32_t>(m.u8());
    if ((bits & kMore1) != 0) bits |= static_cast<uint64_t>(m.u8()) << 8;
    if ((bits & kMore2) != 0) bits |= static_cast<uint64_t>(m.u8()) << 16;
    if ((bits & kMore3) != 0) bits |= static_cast<uint64_t>(m.u8()) << 24;
    if ((bits & kMore4) != 0) bits |= static_cast<uint64_t>(m.u8()) << 32;
    n = (bits & kNum16) != 0 ? m.u16() : m.u8();
    return bits;
  }

  // The player's state as protocol 34 has it: the values the reader resolved where the source's
  // are deltas or wider.
  void player(DemoMsg& m, const SequelView& v, Out& o) {
    int bits = m.u16();
    if (ext >= 3 && (bits & 0x8000) != 0) bits |= m.u8() << 16;
    const int ob = bits & 0x7FFF;
    o.i16(ob);
    const auto skip_point = [&] {
      if (ext >= 2)
        source_coord(m);
      else
        m.i16();
    };
    if ((bits & 1) != 0) o.u8(m.u8());  // the movement type
    if ((bits & 2) != 0) {
      for (const int32_t c : v.origin) {
        skip_point();
        o.i16(short_of(c));
      }
    }
    if ((bits & 4) != 0) {
      for (const int32_t c : v.velocity) {
        skip_point();
        o.i16(short_of(c));
      }
    }
    const auto narrow = [&](const char* what) {
      const int w = ext >= 2 ? m.u16() : m.u8();
      if (w > 255) notes.rewritten[what]++;
      o.u8(w & 0xFF);
    };
    if ((bits & 8) != 0) narrow("a movement time past a byte, its low byte");
    if ((bits & 16) != 0) narrow("movement flags past a byte, their low byte");
    if ((bits & 32) != 0) o.i16(m.i16());  // gravity
    if ((bits & 64) != 0)
      for (int i = 0; i < 3; i++) o.i16(m.i16());
    if ((bits & 128) != 0)
      for (int i = 0; i < 3; i++) o.u8(m.u8());
    if ((bits & 256) != 0)
      for (int i = 0; i < 3; i++) o.i16(m.i16());
    if ((bits & 512) != 0)
      for (int i = 0; i < 3; i++) o.u8(m.u8());
    if ((bits & 4096) != 0) {
      const int g = ext >= 1 ? m.u16() : m.u8();
      if (g > 255) notes.rewritten["a view model past 255 as none"]++;
      o.u8(g > 255 ? 0 : g);
    }
    if ((bits & 8192) != 0)
      for (int i = 0; i < 7; i++) o.u8(m.u8());
    if ((bits & 1024) != 0) {
      if (ext >= 2) {
        const int which = m.u8();
        for (int i = 0; i < 8; i++)
          if ((which >> i & 1) != 0) m.u8();
        if ((which & 0xF0) != 0) notes.dropped["the damage blend"]++;
      } else {
        m.skip(4);
      }
      for (const uint8_t c : v.blend) o.u8(c);
    }
    if ((bits & (1 << 16)) != 0) {
      const int fl = m.u8();
      static constexpr int kSize[6] = {3, 4, 2, 2, 3, 3};
      for (int i = 0; i < 6; i++)
        if ((fl >> i & 1) != 0) m.skip(static_cast<size_t>(kSize[i]));
      if ((fl & 64) != 0) source_coord(m);
      if ((fl & 128) != 0) source_coord(m);
      notes.dropped["the fog"]++;
    }
    if ((bits & 2048) != 0) o.u8(m.u8());   // the field of view
    if ((bits & 16384) != 0) o.u8(m.u8());  // the render flags
    uint64_t mask = 0;
    if (ext >= 2) {
      for (int shift = 0; shift < 64 && !m.bad; shift += 7) {
        const int b = m.u8();
        mask |= static_cast<uint64_t>(b & 127) << shift;
        if ((b & 128) == 0) break;
      }
    } else {
      mask = static_cast<uint32_t>(m.i32());
    }
    o.i32(static_cast<int32_t>(mask & 0xFFFFFFFFu));
    for (int i = 0; i < 64; i++) {
      if ((mask >> i & 1u) == 0) continue;
      const int s = m.i16();
      if (i < 32)
        o.i16(s);
      else
        notes.dropped["a stat past the 32nd"]++;
    }
  }

  // A sound: false when it is dropped.
  bool sound(DemoMsg& m, Out& o) {
    const int flags = m.u8();
    const int index = (flags & 32) != 0 ? m.u16() : m.u8();
    Out f;
    f.u8(flags & ~32);
    f.u8(index & 0xFF);
    if ((flags & 1) != 0) f.u8(m.u8());
    if ((flags & 2) != 0) f.u8(m.u8());
    if ((flags & 16) != 0) f.u8(m.u8());
    int entity = 0;
    if ((flags & 8) != 0) {
      const int w = m.u16();
      entity = w >> 3;
      f.i16(w);
    }
    if ((flags & 4) != 0) pos(m, f);
    if (index > 255) {
      notes.dropped["a sound past the 255th"]++;
      return false;
    }
    if (entity >= kMostEntities34) {
      notes.dropped["a sound on an entity past 1023"]++;
      return false;
    }
    o.u8(kSound);
    o.b.insert(o.b.end(), f.b.begin(), f.b.end());
    return true;
  }

  // A temporary entity by its kind's payload: p a point, d a direction byte, b a byte, s a short,
  // l a long. False when protocol 34 has no kind for it (dropped).
  bool temp(DemoMsg& m, Out& o, std::string& error) {
    const int kind = m.u8();
    const char* shape = nullptr;
    int as = kind;
    switch (kind) {
      case 0:
      case 1:
      case 2:
      case 4:
      case 9:
      case 12:
      case 13:
      case 14:
      case 26:
      case 30:
      case 42:
      case 43:
      case 44:
      case 46:
      case 55:
        shape = "pd";
        break;
      case 5:
      case 6:
      case 7:
      case 8:
      case 17:
      case 18:
      case 20:
      case 21:
      case 22:
      case 28:
      case 35:
      case 45:
      case 47:
      case 48:
      case 49:
      case 51:
      case 52:
      case 53:
      case 54:
        shape = "p";
        break;
      case 3:
      case 11:
      case 23:
      case 27:
      case 34:
      case 41:
        shape = "pp";
        break;
      case 10:
      case 15:
      case 25:
      case 29:
        shape = "bpdb";
        break;
      case 16:
      case 19:
      case 38:
      case 39:
        shape = "spp";
        break;
      case 24:
        shape = "sppp";
        break;
      case 33:
        shape = "sspp";
        break;
      case 36:
        shape = "ps";
        break;
      case 37:
        shape = "ppb";
        break;
      case 50:
        shape = "sp";
        break;
      case 40:
        shape = "steam";
        break;
      case 31:  // the lighter rail's line: the rail's
        shape = "pp";
        as = 3;
        break;
      case 62:  // the explosions without light: the plain ones
        shape = "p";
        as = 5;
        break;
      case 63:
        shape = "p";
        as = 6;
        break;
      case 56:
      case 58:
        shape = "-pd";
        break;
      case 57:
        shape = "-pp";
        break;
      case 59:
      case 61:
        shape = "-spp";
        break;
      case 60:
        shape = "-sb";
        break;
      case 128:
        shape = "-s";
        break;
      default:
        error = "a temporary entity of kind " + std::to_string(kind) + " at byte " +
                std::to_string(m.at) + " that the recording's game does not have";
        return false;
    }
    if (as != kind) notes.rewritten["a newer temporary entity as protocol 34's own kind"]++;
    Out f;
    f.u8(kTemp);
    f.u8(as);
    std::string s = shape;
    const bool drop = !s.empty() && s[0] == '-';
    if (drop) s.erase(0, 1);
    if (s == "steam") {
      const int id = m.i16();
      f.i16(id);
      f.u8(m.u8());
      pos(m, f);
      f.u8(m.u8());
      f.u8(m.u8());
      f.i16(m.i16());
      if (id != -1) f.i32(m.i32());
    } else {
      for (const char c : s) {
        if (c == 'p') pos(m, f);
        if (c == 'd' || c == 'b') f.u8(m.u8());
        if (c == 's') f.i16(m.i16());
      }
    }
    if (drop) {
      notes.dropped["a newer temporary entity protocol 34 has no kind for"]++;
      return true;
    }
    o.b.insert(o.b.end(), f.b.begin(), f.b.end());
    return true;
  }
};

const DemoEntity* find(const std::vector<std::pair<int, DemoEntity>>& list, int n) {
  const auto it =
      std::lower_bound(list.begin(), list.end(), n,
                       [](const std::pair<int, DemoEntity>& p, int k) { return p.first < k; });
  return it != list.end() && it->first == n ? &it->second : nullptr;
}

}  // namespace

bool extended_block_to_34(const Demo& d, std::span<const uint8_t> body,
                          const std::vector<DemoSpan>& spans, ConvertNotes& notes,
                          std::vector<std::vector<uint8_t>>& blocks, std::string& error) {
  const int protocol = d.state.protocol;
  Ext x{d, notes, protocol >= 3434 && protocol <= 3436 ? protocol - 3433 : 0};
  Out block;
  const auto put = [&](Out& msg) {
    if (!block.b.empty() && block.b.size() + msg.b.size() > kMostBlock34) {
      blocks.push_back(std::move(block.b));
      block = {};
      notes.rewritten["a block over 1400 bytes split between its messages"]++;
    }
    if (msg.b.size() > kMostBlock34) notes.rewritten["a message over 1400 bytes kept whole"]++;
    block.b.insert(block.b.end(), msg.b.begin(), msg.b.end());
  };
  for (const DemoSpan& sp : spans) {
    DemoMsg m{body.subspan(sp.at, sp.end - sp.at)};
    m.int23 = x.ext >= 2;
    m.u8();  // the id
    Out o;
    switch (sp.id) {
      case kServerData:
        o.u8(kServerData);
        m.i32();
        o.i32(34);
        notes.rewritten["the server data's protocol as 34"]++;
        while (!m.done()) o.u8(m.u8());
        break;
      case kConfig: {
        const int index = m.i16();
        const std::string text = m.str();
        const int slot = Ext::config_slot(index);
        if (slot < 0) {
          notes.dropped["a settings string with no slot in protocol 34's lists"]++;
          continue;
        }
        o.u8(kConfig);
        o.i16(slot);
        o.str(text);
        break;
      }
      case kBaseline: {
        int n = 0;
        const uint64_t bits = x.header(m, n);
        Out e;
        const DemoEntity* r = static_cast<size_t>(n) < d.baselines.size()
                                  ? &d.baselines[static_cast<size_t>(n)]
                                  : nullptr;
        if (!x.entity(m, bits, n, r, e)) break;
        if (e.b.empty()) continue;
        o.u8(kBaseline);
        o.b.insert(o.b.end(), e.b.begin(), e.b.end());
        break;
      }
      case kFrame: {
        o.u8(kFrame);
        const int32_t number = m.i32();
        o.i32(number);
        o.i32(m.i32());
        o.u8(m.u8());  // the packets held back
        const int areas = m.u8();
        o.u8(areas);
        for (int i = 0; i < areas; i++) o.u8(m.u8());
        o.u8(m.u8());  // the player's state's id
        const size_t slot = static_cast<size_t>(number) & 15;
        x.player(m, d.views[slot], o);
        o.u8(m.u8());  // the entity list's id
        const auto& list = d.ring[slot];
        for (;;) {
          int n = 0;
          const uint64_t bits = x.header(m, n);
          if (m.bad) break;
          if (n == 0) {
            o.u8(0);
            o.u8(0);
            break;
          }
          if (!x.entity(m, bits, n, find(list, n), o)) break;
        }
        notes.frames++;
        break;
      }
      case kSound:
        if (!x.sound(m, o)) continue;
        break;
      case kTemp:
        if (!x.temp(m, o, error)) return false;
        if (o.b.empty()) continue;
        break;
      case kMuzzle:
      case kMuzzle2: {
        const int w = m.u16();
        int entity = w, flash = m.u8();
        if (sp.id == kMuzzle2) {
          entity = w & 0x1FFF;
          flash |= (w >> 13) << 8;
        }
        if (entity >= kMostEntities34 || flash > 255) {
          notes.dropped["a muzzle flash past protocol 34's entities or kinds"]++;
          continue;
        }
        o.u8(sp.id);
        o.i16(entity);
        o.u8(flash);
        break;
      }
      default:  // the rest keeps protocol 34's layout
        o.b.assign(body.begin() + sp.at, body.begin() + sp.end);
        break;
    }
    if (m.bad) {
      error = "the message " + std::to_string(sp.id) + " at byte " + std::to_string(sp.at) +
              " could not be rewritten: it runs past its end";
      return false;
    }
    put(o);
  }
  if (!block.b.empty()) blocks.push_back(std::move(block.b));
  return true;
}

}  // namespace shooters_edge::client
