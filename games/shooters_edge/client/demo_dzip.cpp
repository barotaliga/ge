// demo_dzip.hpp's reader. The archive is read whole and checked before anything is unpacked; a
// demo's rewritten stream is written back block by block with a model of every entity (its
// baseline, the frame last written and the frame being built), the client data last written and
// the running time and view angles, as the stream's form defines them. Each rebuilt message is the
// classic one a recording holds, so the checksum over the result holds only when every byte is
// right.
//
// ge:summary The Dzip archive and its demo stream written back as a recording, byte for byte
// ge:keywords dzip dz entity-group client-data delta force-bits baseline raw-tail time-delta
// ge:skill shooter vfs
// ge:test tests/shooters_edge_demo_dzip_test.cpp
#include "shooters_edge/client/demo_dzip.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <set>

#include "core/inflate.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kEntities = 32768;

struct In {
  std::span<const uint8_t> b;
  size_t at = 0;
  bool bad = false;
  bool more() const { return at < b.size(); }
  uint8_t peek() const { return at < b.size() ? b[at] : 0; }
  uint8_t u8() {
    if (at >= b.size()) {
      bad = true;
      return 0;
    }
    return b[at++];
  }
  uint16_t u16() {
    const uint16_t lo = u8();
    return static_cast<uint16_t>(lo | (u8() << 8));
  }
  uint32_t u32() {
    const uint32_t lo = u16();
    return lo | (static_cast<uint32_t>(u16()) << 16);
  }
  int8_t c8() { return static_cast<int8_t>(u8()); }
  // A NUL-ended string's bytes, the NUL with them.
  void str(std::vector<uint8_t>& out) {
    for (;;) {
      const uint8_t c = u8();
      out.push_back(c);
      if (c == 0 || bad) return;
    }
  }
  void copy(std::vector<uint8_t>& out, size_t n) {
    if (at + n > b.size()) {
      bad = true;
      return;
    }
    out.insert(out.end(), b.begin() + static_cast<std::ptrdiff_t>(at),
               b.begin() + static_cast<std::ptrdiff_t>(at + n));
    at += n;
  }
};

void put8(std::vector<uint8_t>& o, uint32_t v) { o.push_back(static_cast<uint8_t>(v)); }
void put16(std::vector<uint8_t>& o, uint32_t v) {
  put8(o, v);
  put8(o, v >> 8);
}
void put32(std::vector<uint8_t>& o, uint32_t v) {
  put16(o, v & 0xFFFF);
  put16(o, v >> 16);
}
void put_float(std::vector<uint8_t>& o, float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, 4);
  put32(o, u);
}

// A 16-bit field moved by a signed byte: the whole sum once its high byte is in use, else a byte.
uint16_t add16(uint16_t prev, int8_t d) {
  const auto sum = static_cast<uint16_t>(prev + d);
  return (prev & 0xFF00) != 0 ? sum : static_cast<uint16_t>(sum & 0xFF);
}

struct Ent {
  uint16_t model = 0, frame = 0;
  uint8_t colormap = 0, skin = 0, effects = 0;
  std::array<uint8_t, 3> angle{};
  std::array<int16_t, 3> origin{}, vel{};
  bool nolerp = false;
  uint8_t alpha = 0, scale = 0, lerp = 0;
  uint32_t trans_alpha = 0;  // protocol 15's transparency: the f32's bits
  uint8_t fullbright = 0;
  uint64_t force = 0;
  bool active = false;
};

// Entity force bits.
enum : uint8_t {
  kFOrigin1 = 10,
  kFAngle0 = 11,
  kFAngle1 = 12,
  kFAngle2 = 13,
  kFFrame = 14,
  kFOrigin0 = 16,
  kFOrigin2 = 17,
  kFModel = 18,
  kFColormap = 19,
  kFSkin = 20,
  kFEffects = 21,
  kFLong = 22,
  kFTrans = 23,
  kFAlpha = 24,
  kFScale = 25,
  kFLerp = 26,
  kFModelHigh = 27,
  kFFrameHigh = 28,
};

struct Client {
  std::array<uint8_t, 3> vel{}, punch{};
  uint16_t weapon_frame = 0, ammo = 0, armour = 0, shells = 0, nails = 0, rockets = 0, model = 0,
           cells = 0;
  int16_t health = 0;
  uint32_t items = 0x4001;
  bool on_ground = false, in_water = false, items_absent = false;
  uint8_t ideal_pitch = 0, active_weapon = 0, view_height = 22, weapon_alpha = 0;
  uint32_t force = 0;
  uint8_t alpha_written = 0;
};

struct Decoder {
  In in;
  std::vector<uint8_t> out;
  int protocol = 15;
  uint32_t time = 0;
  std::array<uint32_t, 3> angles{};
  int prev_baseline = 0;
  std::vector<Ent> base, old, now;
  std::set<int> frame;  // the entities in the frame
  Client cl;
  std::string why;

  Decoder() : base(kEntities), old(kEntities), now(kEntities) {}

  bool fail(const std::string& w) {
    if (why.empty()) why = w;
    return false;
  }

  // One message of a fixed total length, the id already read.
  void fixed(std::vector<uint8_t>& m, uint8_t id, size_t total) {
    m.push_back(id);
    in.copy(m, total - 1);
  }

  void baseline_from(int e, const Ent& b, bool reset_rest) {
    if (reset_rest) base[size_t(e)] = Ent{};
    Ent& t = base[size_t(e)];
    t.model = b.model;
    t.frame = b.frame;
    t.colormap = b.colormap;
    t.skin = b.skin;
    t.angle = b.angle;
    t.origin = b.origin;
    t.alpha = b.alpha;
  }

  void body15(std::vector<uint8_t>& m, const Ent& e) {
    put8(m, e.model);
    put8(m, e.frame);
    put8(m, e.colormap);
    put8(m, e.skin);
    for (int i = 0; i < 3; i++) {
      put16(m, static_cast<uint16_t>(e.origin[size_t(i)]));
      put8(m, e.angle[size_t(i)]);
    }
  }

  // Encoded 22: the step from the last baseline's number, then the present fields.
  void baseline22(std::vector<uint8_t>& m) {
    const uint16_t x = in.u16();
    const int e = (prev_baseline + (x & 1023)) % 1024;
    prev_baseline = e;
    Ent b;
    b.model = in.u8();
    if ((x & (1 << 10)) != 0) b.frame = in.u8();
    if ((x & (1 << 11)) != 0) b.colormap = in.u8();
    if ((x & (1 << 12)) != 0) b.skin = in.u8();
    if ((x & (1 << 13)) != 0)
      for (int i = 0; i < 3; i++) b.origin[size_t(i)] = static_cast<int16_t>(in.u16());
    if ((x & (1 << 14)) != 0) b.angle[1] = in.u8();
    if ((x & (1 << 15)) != 0) {
      b.angle[0] = in.u8();
      b.angle[2] = in.u8();
    }
    put8(m, 22);
    put16(m, uint32_t(e));
    body15(m, b);
    baseline_from(e, b, true);
  }

  // Encoded 42: an absolute number, flags, the fields at their widths.
  void baseline42(std::vector<uint8_t>& m) {
    const int e = in.u16() & 0x7FFF;
    uint32_t f = in.u8();
    if ((f & 1) != 0) f |= uint32_t(in.u8()) << 8;
    Ent b;
    b.model = (f & (1 << 9)) != 0 ? in.u16() : in.u8();
    if ((f & (1 << 2)) != 0) b.frame = (f & (1 << 10)) != 0 ? in.u16() : in.u8();
    if ((f & (1 << 3)) != 0) b.colormap = in.u8();
    if ((f & (1 << 4)) != 0) b.skin = in.u8();
    if ((f & (1 << 5)) != 0)
      for (int i = 0; i < 3; i++) b.origin[size_t(i)] = static_cast<int16_t>(in.u16());
    if ((f & (1 << 6)) != 0) b.angle[1] = in.u8();
    if ((f & (1 << 7)) != 0) {
      b.angle[0] = in.u8();
      b.angle[2] = in.u8();
    }
    if ((f & (1 << 11)) != 0) b.alpha = in.u8();
    if ((f & (1 << 8)) != 0) {
      put8(m, 22);
      put16(m, uint32_t(e));
      body15(m, b);
    } else {
      const bool big_model = (f & (1 << 9)) != 0, big_frame = (f & (1 << 10)) != 0,
                 alpha = (f & (1 << 11)) != 0;
      put8(m, 42);
      put16(m, uint32_t(e));
      put8(m, (big_model ? 1 : 0) | (big_frame ? 2 : 0) | (alpha ? 4 : 0));
      big_model ? put16(m, b.model) : put8(m, b.model);
      big_frame ? put16(m, b.frame) : put8(m, b.frame);
      put8(m, b.colormap);
      put8(m, b.skin);
      for (int i = 0; i < 3; i++) {
        put16(m, static_cast<uint16_t>(b.origin[size_t(i)]));
        put8(m, b.angle[size_t(i)]);
      }
      if (alpha) put8(m, b.alpha);
    }
    baseline_from(e, b, true);
  }

  // Encoded 0x40..0x4F: the client data's changes, then a force record, then the classic message.
  void client_data(std::vector<uint8_t>& m, uint8_t first) {
    uint64_t k = first & 0x0F;
    if ((k & (1u << 3)) != 0) k |= uint64_t(in.u8()) << 8;
    if ((k & (1u << 15)) != 0) k |= uint64_t(in.u8()) << 16;
    if ((k & (1u << 23)) != 0) k |= uint64_t(in.u8()) << 24;
    if ((k & (1ull << 31)) != 0) k |= uint64_t(in.u8()) << 32;
    const auto has = [&](int bit) { return (k >> bit & 1) != 0; };
    const auto byte_delta = [&](int bit, uint8_t& v) {
      if (has(bit)) v = static_cast<uint8_t>(v + in.c8());
    };
    const auto wide = [&](int bit, int abs_bit, uint16_t& v) {
      if (has(abs_bit))
        v = in.u16();
      else if (has(bit))
        v = add16(v, in.c8());
    };
    byte_delta(0, cl.vel[2]);
    byte_delta(1, cl.vel[0]);
    byte_delta(2, cl.vel[1]);
    wide(8, 29, cl.weapon_frame);
    if (has(9)) cl.on_ground = !cl.on_ground;
    byte_delta(10, cl.punch[0]);
    wide(11, 30, cl.ammo);
    if (has(12)) cl.health = static_cast<int16_t>(cl.health + static_cast<int16_t>(in.u16()));
    if (has(13)) cl.items ^= in.u32();
    wide(14, 32, cl.armour);
    byte_delta(16, cl.ideal_pitch);
    wide(17, 33, cl.shells);
    wide(18, 34, cl.nails);
    wide(19, 35, cl.rockets);
    wide(20, 36, cl.model);
    byte_delta(21, cl.active_weapon);
    if (has(22)) cl.in_water = !cl.in_water;
    byte_delta(24, cl.view_height);
    wide(25, 37, cl.cells);
    byte_delta(26, cl.punch[1]);
    byte_delta(27, cl.punch[2]);
    if (has(28)) cl.items_absent = !cl.items_absent;
    byte_delta(38, cl.weapon_alpha);
    if ((in.peek() & 0xF0) == 0x50) {
      uint32_t f = in.u8();
      if ((f & 8) != 0) f |= uint32_t(in.u8()) << 8;
      cl.force ^= f & 0xFF07;
    }
    // The classic message from the current values.
    uint32_t bits = 0;
    std::vector<uint8_t> body;
    const auto forced = [&](int bit) { return (cl.force >> bit & 1) != 0; };
    const auto opt = [&](uint32_t su, uint8_t v, uint8_t def, int force_bit) {
      if (v != def || forced(force_bit)) {
        bits |= su;
        put8(body, v);
      }
    };
    if (!cl.items_absent) bits |= 1u << 9;
    opt(1u << 0, cl.view_height, 22, 11);
    opt(1u << 1, cl.ideal_pitch, 0, 12);
    for (int i = 0; i < 3; i++) {
      opt(1u << (2 + i), cl.punch[size_t(i)], 0, 8 + i);
      opt(1u << (5 + i), cl.vel[size_t(i)], 0, i);
    }
    put32(body, cl.items);
    if (cl.on_ground) bits |= 1u << 10;
    if (cl.in_water) bits |= 1u << 11;
    opt(1u << 12, static_cast<uint8_t>(cl.weapon_frame), 0, 13);
    opt(1u << 13, static_cast<uint8_t>(cl.armour), 0, 14);
    opt(1u << 14, static_cast<uint8_t>(cl.model), 0, 15);
    put16(body, static_cast<uint16_t>(cl.health));
    for (const uint16_t v : {cl.ammo, cl.shells, cl.nails, cl.rockets, cl.cells})
      put8(body, v & 0xFF);
    put8(body, cl.active_weapon);
    const auto high = [&](uint32_t bit, uint16_t v) {
      if ((v >> 8) != 0) {
        bits |= 1u << bit;
        put8(body, v >> 8);
      }
    };
    high(16, cl.model);
    high(17, cl.armour);
    high(18, cl.ammo);
    high(19, cl.shells);
    high(20, cl.nails);
    high(21, cl.rockets);
    high(22, cl.cells);
    high(24, cl.weapon_frame);
    if (cl.weapon_alpha != cl.alpha_written) {
      bits |= 1u << 25;
      put8(body, cl.weapon_alpha);
      cl.alpha_written = cl.weapon_alpha;
    }
    if ((bits & 0xFF000000u) != 0) bits |= 1u << 23;
    if ((bits & 0x00FF0000u) != 0) bits |= 1u << 15;
    put8(m, 15);
    put16(m, bits & 0xFFFF);
    if ((bits & (1u << 15)) != 0) put8(m, bits >> 16);
    if ((bits & (1u << 23)) != 0) put8(m, bits >> 24);
    m.insert(m.end(), body.begin(), body.end());
  }

  // A sound: `mask` the real mask; the channel and the origin restored.
  void sound(std::vector<uint8_t>& m, uint32_t mask) {
    put8(m, 6);
    put8(m, mask);
    if ((mask & 1) != 0) put8(m, in.u8());
    if ((mask & 2) != 0) put8(m, in.u8());
    int ent = 0;
    if ((mask & 8) != 0) {
      ent = in.u16();
      put16(m, uint32_t(ent));
      put8(m, in.u8());
    } else {
      const uint16_t v = in.u16();
      ent = v >> 3;
      const uint32_t channel = (2u - (v & 7u)) & 7u;
      put16(m, (uint32_t(ent) << 3) | channel);
    }
    if ((mask & 16) != 0)
      put16(m, in.u16());
    else
      put8(m, in.u8());
    for (int i = 0; i < 3; i++) {
      auto o = static_cast<uint16_t>(in.u16());
      if (ent < kEntities) o = static_cast<uint16_t>(o + now[size_t(ent)].origin[size_t(i)]);
      put16(m, o);
    }
  }

  // A classic entity update copied as it is: its length from its own bits.
  bool verbatim(std::vector<uint8_t>& m, uint8_t first) {
    uint32_t bits = first;
    std::vector<uint8_t> u = {first};
    if ((bits & 1) != 0) {
      const uint8_t b = in.u8();
      u.push_back(b);
      bits |= uint32_t(b) << 8;
    }
    if (protocol == 15 && (bits & (1u << 15)) != 0)
      return fail("an entity update with the older transparency the stream does not keep");
    if (protocol != 15 && (bits & (1u << 15)) != 0) {
      const uint8_t b = in.u8();
      u.push_back(b);
      bits |= uint32_t(b) << 16;
    }
    if (protocol != 15 && (bits & (1u << 23)) != 0) {
      const uint8_t b = in.u8();
      u.push_back(b);
      bits |= uint32_t(b) << 24;
    }
    size_t n = (bits & (1u << 14)) != 0 ? 2 : 1;
    for (const int b : {10, 6, 11, 12, 13, 8, 4, 9})
      if ((bits >> b & 1) != 0) n += 1;
    for (const int b : {1, 2, 3})
      if ((bits >> b & 1) != 0) n += 2;
    if (protocol != 15)
      for (const int b : {16, 17, 18, 19, 20})
        if ((bits >> b & 1) != 0) n += 1;
    in.copy(u, n);
    m.insert(m.end(), u.begin(), u.end());
    return true;
  }

  // Encoded 0x30: the frame's entities.
  bool group(std::vector<uint8_t>& m) {
    int run = 0;
    for (;;) {
      const uint8_t v = in.u8();
      if (in.bad) return fail("the stream ends inside an entity group");
      if (v == 0) break;
      if (v == 0xFF) {
        run += 254;
        continue;
      }
      const int e = run + v;
      if (e >= kEntities) return fail("an entity group names an entity past the limit");
      now[size_t(e)].active = true;
      frame.insert(e);
    }
    for (auto it = frame.begin(); it != frame.end();) {
      const int e = *it;
      Ent& n = now[size_t(e)];
      const Ent& o = old[size_t(e)];
      for (int i = 0; i < 3; i++)
        n.origin[size_t(i)] = static_cast<int16_t>(n.origin[size_t(i)] + n.vel[size_t(i)]);
      if (!n.active) {
        ++it;
        continue;
      }
      uint32_t r = in.u8();
      if (r == 0x80) {
        now[size_t(e)] = base[size_t(e)];
        old[size_t(e)] = base[size_t(e)];
        it = frame.erase(it);
        continue;
      }
      if (r == 0) {
        n.active = false;
        ++it;
        continue;
      }
      if ((r & 0x80) != 0) r |= uint32_t(in.u8()) << 8;
      if ((r & 0x8000) != 0) r |= uint32_t(in.u8()) << 16;
      const auto has = [&](int bit) { return (r >> bit & 1) != 0; };
      // Origins: 2, 1, 0, each small (a change in velocity) or absolute.
      for (const auto& [small, absolute, axis] :
           {std::array<int, 3>{0, 11, 2}, std::array<int, 3>{1, 10, 1},
            std::array<int, 3>{2, 9, 0}}) {
        const auto a = size_t(axis);
        if (has(small)) {
          n.vel[a] = static_cast<int16_t>(o.vel[a] + in.c8());
          n.origin[a] = static_cast<int16_t>(o.origin[a] + n.vel[a]);
        }
        if (has(absolute)) {
          n.origin[a] = static_cast<int16_t>(in.u16());
          n.vel[a] = static_cast<int16_t>(n.origin[a] - o.origin[a]);
        }
      }
      for (int i = 0; i < 3; i++)
        if (has(3 + i)) n.angle[size_t(i)] = static_cast<uint8_t>(o.angle[size_t(i)] + in.c8());
      if (has(6)) n.frame = static_cast<uint16_t>(o.frame + 1);
      if (has(8)) n.frame = add16(o.frame, in.c8());
      if (has(22)) n.frame = in.u16();
      if (has(12)) n.effects = in.u8();
      if (has(13)) n.model = static_cast<uint16_t>((n.model & 0xFF00) | in.u8());
      if (has(21)) n.model = static_cast<uint16_t>((n.model & 0x00FF) | (in.u8() << 8));
      if (has(14)) n.nolerp = !n.nolerp;
      if (has(16)) n.colormap = in.u8();
      if (has(17)) n.skin = in.u8();
      if (has(18)) {
        if (protocol == 15)
          n.trans_alpha = in.u32();
        else
          n.alpha = in.u8();
      }
      if (has(19)) {
        if (protocol == 15)
          n.fullbright = in.u8();
        else
          n.scale = in.u8();
      }
      if (has(20)) n.lerp = in.u8();
      ++it;
    }
    if (in.peek() == 0x31) {
      in.u8();
      for (;;) {
        uint32_t v = in.u16();
        if (v == 0 || in.bad) break;
        if ((v & 0x8000) != 0) v |= uint32_t(in.u8()) << 16;
        if (protocol != 15 && (v & (1u << 23)) != 0) v |= uint32_t(in.u8()) << 24;
        now[v & 1023].force ^= v & ~1023u;
      }
    }
    if (in.peek() == 0x32) {
      in.u8();
      for (;;) {
        uint64_t v = in.u16();
        if (v == 0 || in.bad) break;
        v |= uint64_t(in.u8()) << 16;
        if ((v & (1u << 23)) != 0) v |= uint64_t(in.u8()) << 24;
        if (protocol != 15 && (v & (1ull << 31)) != 0) v |= uint64_t(in.u8()) << 32;
        now[v & 0x7FFF].force ^= (v & ~uint64_t{0x7FFF}) >> 8;
      }
    }
    for (const int e : frame) write_update(m, e);
    return !in.bad || fail("the stream ends inside an entity group");
  }

  void write_update(std::vector<uint8_t>& m, int e) {
    const Ent& n = now[size_t(e)];
    const Ent& b = base[size_t(e)];
    const auto forced = [&](uint32_t bit) { return (n.force >> bit & 1) != 0; };
    uint32_t bits = 1u << 7;
    std::vector<uint8_t> body;
    const bool long_number = e > 255 || forced(kFLong);
    if (long_number) bits |= 1u << 14;
    const auto field = [&](uint32_t bit, bool differs, uint32_t force_bit, uint32_t v) {
      if (differs || forced(force_bit)) {
        bits |= 1u << bit;
        put8(body, v);
      }
    };
    field(10, n.model != b.model, kFModel, n.model & 0xFF);
    field(6, n.frame != b.frame, kFFrame, n.frame & 0xFF);
    field(11, n.colormap != b.colormap, kFColormap, n.colormap);
    field(12, n.skin != b.skin, kFSkin, n.skin);
    field(13, n.effects != b.effects, kFEffects, n.effects);
    static constexpr std::array<uint32_t, 3> kOriginBit = {1, 2, 3}, kAngleBit = {8, 4, 9},
                                             kOriginForce = {kFOrigin0, kFOrigin1, kFOrigin2},
                                             kAngleForce = {kFAngle0, kFAngle1, kFAngle2};
    for (size_t i = 0; i < 3; i++) {
      if (n.origin[i] != b.origin[i] || forced(kOriginForce[i])) {
        bits |= 1u << kOriginBit[i];
        put16(body, static_cast<uint16_t>(n.origin[i]));
      }
      field(kAngleBit[i], n.angle[i] != b.angle[i], kAngleForce[i], n.angle[i]);
    }
    if (protocol == 15 && forced(kFTrans)) {
      bits |= 1u << 15;
      const float mode = n.fullbright != 0 ? 2.0f : 1.0f;
      put_float(body, mode);
      put32(body, n.trans_alpha);
      if (n.fullbright != 0) put_float(body, static_cast<float>(n.fullbright) - 1.0f);
    }
    if (protocol != 15) {
      field(16, n.alpha != b.alpha, kFAlpha, n.alpha);
      field(20, n.scale != b.scale, kFScale, n.scale);
      field(17, (n.frame >> 8) != (b.frame >> 8) && (n.frame >> 8) != 0, kFFrameHigh, n.frame >> 8);
      field(18, (n.model >> 8) != (b.model >> 8) && (n.model >> 8) != 0, kFModelHigh, n.model >> 8);
      field(19, n.lerp != b.lerp, kFLerp, n.lerp);
    }
    if (n.nolerp) bits |= 1u << 5;
    if ((bits & 0x00FF0000u) != 0) bits |= 1u << 15;
    if ((bits & 0x00FFFF00u) != 0) bits |= 1u;
    put8(m, bits);
    if ((bits & 1u) != 0) put8(m, bits >> 8);
    if ((bits & 0x00FF0000u) != 0) put8(m, bits >> 16);
    long_number ? put16(m, uint32_t(e)) : put8(m, uint32_t(e));
    m.insert(m.end(), body.begin(), body.end());
    old[size_t(e)] = n;
  }

  // The server info copied; its protocol noted.
  bool server_info(std::vector<uint8_t>& m) {
    put8(m, 11);
    const uint32_t p = in.u32();
    put32(m, p);
    if (p != 15 && p != 666)
      return fail("the demo's protocol " + std::to_string(p) +
                  " is not one the archive's stream holds (15 or 666)");
    protocol = int(p);
    prev_baseline = 0;
    put8(m, in.u8());
    put8(m, in.u8());
    in.str(m);
    for (int list = 0; list < 2; list++) {
      for (;;) {
        const size_t before = m.size();
        in.str(m);
        if (in.bad || m.size() == before + 1) break;
      }
    }
    return true;
  }

  bool temp_entity(std::vector<uint8_t>& m) {
    put8(m, 23);
    const uint8_t type = in.u8();
    put8(m, type);
    size_t total = 0;
    switch (type) {
      case 5:
      case 6:
      case 9:
      case 13:
        total = 16;
        break;
      case 12:
        total = 10;
        break;
      case 16:
        total = 14;
        break;
      case 17:  // a string, then 14 bytes: the string's length and 17 in all
        in.str(m);
        in.copy(m, 14);
        return true;
      default:
        if (type > 17) return fail("a temporary effect of a kind the stream does not keep");
        total = 8;
    }
    in.copy(m, total - 2);
    return true;
  }

  // One encoded message by its first byte, appended to the block.
  bool message(std::vector<uint8_t>& m, uint8_t b, bool& had_baseline) {
    static constexpr std::array<uint8_t, 45> kFixed = [] {
      std::array<uint8_t, 45> t{};
      const std::pair<int, int> rows[] = {{1, 1},  {2, 1},  {3, 6},  {4, 5},   {5, 3},  {10, 4},
                                          {14, 4}, {16, 3}, {17, 3}, {18, 12}, {19, 9}, {20, 14},
                                          {21, 1}, {24, 2}, {25, 2}, {27, 1},  {28, 1}, {29, 10},
                                          {30, 1}, {32, 3}, {33, 1}, {41, 6},  {44, 11}};
      for (const auto& [id, n] : rows) t[size_t(id)] = uint8_t(n);
      return t;
    }();
    if ((b & 0xF8) == 0x30) return group(m);
    if (b >= 1 && b <= 44) {
      if (kFixed[b] != 0) {
        fixed(m, b, kFixed[b]);
        return true;
      }
      switch (b) {
        case 8:
        case 9:
        case 26:
        case 31:
        case 34:
        case 36:
        case 37:
          put8(m, b);
          in.str(m);
          return true;
        case 12:
        case 13:
          put8(m, b);
          put8(m, in.u8());
          in.str(m);
          return true;
        case 11:
          return server_info(m);
        case 43: {
          put8(m, 43);
          const uint8_t f = in.u8();
          put8(m, f);
          in.copy(m, 13 + ((f & 1) != 0) + ((f & 2) != 0) + ((f & 4) != 0));
          return true;
        }
        case 23:
          return temp_entity(m);
        case 38:
          put8(m, 35);
          in.str(m);
          in.str(m);
          in.copy(m, 2);
          return true;
        case 7:
          time += in.u16();
          put8(m, 7);
          put32(m, time);
          return true;
        case 35:
          time += in.u32();
          put8(m, 7);
          put32(m, time);
          return true;
        case 22:
          had_baseline = true;
          baseline22(m);
          return true;
        case 42:
          had_baseline = true;
          baseline42(m);
          return true;
        case 6:
          sound(m, in.u8());
          return true;
        default:
          return fail("message " + std::to_string(b) + ", which the stream never holds");
      }
    }
    if ((b & 0xF0) == 0x40) {
      client_data(m, b);
      return true;
    }
    if ((b & 0xF8) == 0x38 || (b & 0xE0) == 0x60) {
      uint32_t mask = b & 7u;
      if ((b & 8) == 0) mask |= 8;
      if ((b & 16) == 0) mask |= 16;
      sound(m, mask);
      return true;
    }
    if ((b & 0x80) != 0) return verbatim(m, b);
    return fail("byte " + std::to_string(b) + " where a message starts");
  }

  bool run() {
    // The track line, as it was.
    size_t nl = 0;
    while (nl < 12 && nl < in.b.size() && in.b[nl] != '\n') nl++;
    if (nl >= 12 || nl >= in.b.size()) return fail("the demo stream has no track line");
    in.copy(out, nl + 1);
    while (in.more()) {
      if (in.peek() == 0xFF) {  // the raw tail: the rest of the recording as it was
        while (in.more()) {
          if (in.u8() != 0xFF) return fail("the raw tail is damaged");
          const uint32_t n = in.u32();
          in.copy(out, n);
          if (in.bad) return fail("the raw tail runs past the stream");
        }
        break;
      }
      const uint8_t flags = in.u8();
      for (int i = 0; i < 3; i++)
        if ((flags >> i & 1) != 0) angles[size_t(i)] += in.u32();
      std::vector<uint8_t> m;
      bool had_baseline = false;
      for (;;) {
        const uint8_t b = in.u8();
        if (in.bad) return fail("the stream ends inside a block");
        if (b == 0) break;
        if (!message(m, b, had_baseline)) return false;
        if (in.bad) return fail("the stream ends inside a message");
      }
      put32(out, uint32_t(m.size()));
      for (const uint32_t a : angles) put32(out, a);
      out.insert(out.end(), m.begin(), m.end());
      if (had_baseline) {
        old = base;
        now = base;
      }
    }
    return true;
  }
};

uint32_t u32_at(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) |
         (uint32_t(b[at + 3]) << 24);
}

uint32_t adler32(std::span<const uint8_t> b) {
  uint32_t a = 1, s = 0;
  for (const uint8_t v : b) {
    a = (a + v) % 65521;
    s = (s + a) % 65521;
  }
  return (s << 16) | a;
}

}  // namespace

bool is_dzip(std::span<const uint8_t> b) {
  return b.size() >= 12 && b[0] == 'D' && b[1] == 'Z' && b[2] >= 1 && b[2] <= 3;
}

bool dzip_list(std::span<const uint8_t> b, std::vector<DzipEntry>& out, std::string& error) {
  out.clear();
  if (!is_dzip(b)) {
    error = "dzip: not a Dzip archive (no \"DZ\" and a version of 1 to 3)";
    return false;
  }
  if (b[2] == 1) {
    error = "dzip: an archive of the first version (2000), whose layout is not read";
    return false;
  }
  const uint32_t dir = u32_at(b, 4), count = u32_at(b, 8);
  if ((count >> 27) != 0 || dir >= b.size() || uint64_t(count) * 32 > b.size()) {
    error = "dzip: the directory's place or size is outside the file; the archive is damaged";
    return false;
  }
  size_t at = dir;
  for (uint32_t i = 0; i < count; i++) {
    if (at + 32 > b.size()) {
      error = "dzip: the directory runs past the file; the archive is damaged";
      return false;
    }
    DzipEntry e;
    e.data = u32_at(b, at);
    e.packed = u32_at(b, at + 4);
    e.size = u32_at(b, at + 8);
    const size_t name_len = b[at + 12] | (b[at + 13] << 8);
    e.check = u32_at(b, at + 16);
    e.type = u32_at(b, at + 20);
    e.middle = u32_at(b, at + 28);
    if (name_len == 0 || name_len > 257 || at + 32 + name_len > b.size()) {
      error = "dzip: entry " + std::to_string(i) + "'s name is past the file";
      return false;
    }
    e.name.assign(reinterpret_cast<const char*>(b.data() + at + 32), name_len - 1);
    std::replace(e.name.begin(), e.name.end(), '\\', '/');
    if (e.type != 7 && uint64_t(e.data) + e.packed > b.size()) {
      error = "dzip: '" + e.name + "' runs past the file; the archive is damaged";
      return false;
    }
    at += 32 + name_len;
    out.push_back(std::move(e));
  }
  return true;
}

bool dzip_entry(std::span<const uint8_t> b, const DzipEntry& e, std::vector<uint8_t>& out,
                std::string& error) {
  const std::span<const uint8_t> packed = b.subspan(e.data, e.packed);
  if (e.type == 8) {
    out.assign(packed.begin(), packed.end());
  } else if (e.type == 0 || e.type == 5 || e.type == 6) {
    auto middle = ge::core::inflate_zlib(packed);
    if (!middle || middle->size() != e.middle) {
      error = "dzip: '" + e.name + "' does not unpack; the archive is damaged";
      return false;
    }
    if (e.type == 0) {
      out = std::move(*middle);
    } else {
      Decoder d;
      d.in.b = *middle;
      if (!d.run()) {
        error = "dzip: '" + e.name + "' cannot be written back as a recording: " + d.why;
        return false;
      }
      out = std::move(d.out);
    }
  } else {
    error = "dzip: '" + e.name + "' is a " +
            (e.type == 3 ? std::string("pack") : "kind " + std::to_string(e.type)) +
            " entry, which is not read";
    return false;
  }
  if (out.size() != e.size || ~ge::core::crc32(out.data(), out.size()) != e.check) {
    error = "dzip: '" + e.name + "' fails its checksum; the archive is damaged";
    return false;
  }
  return true;
}

bool dzip_unpack(std::span<const uint8_t> b, std::vector<uint8_t>& out, std::string& name,
                 std::string& error, const std::function<bool(std::string_view)>& pick) {
  std::vector<DzipEntry> list;
  if (!dzip_list(b, list, error)) return false;
  std::vector<const DzipEntry*> taken;
  std::string names;
  int files = 0;
  for (const DzipEntry& e : list) {
    if (e.type == 7) continue;
    if (files++ < 8) names += (names.empty() ? "" : ", ") + e.name;
    if (!pick || pick(e.name)) taken.push_back(&e);
  }
  if (taken.size() != 1) {
    error = taken.empty() ? "dzip: the archive holds no recording (" + names + ")"
                          : "dzip: the archive holds " + std::to_string(taken.size()) +
                                " recordings (" + names + "); unpack it and name the one to open";
    return false;
  }
  name = taken[0]->name;
  return dzip_entry(b, *taken[0], out, error);
}

std::vector<uint8_t> dzip_write(const std::vector<DzipFile>& files) {
  std::vector<uint8_t> o = {'D', 'Z', 3, 2, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint32_t> offsets, sizes;
  for (const DzipFile& f : files) {
    offsets.push_back(uint32_t(o.size()));
    const size_t start = o.size();
    o.push_back(0x78);
    o.push_back(0x01);
    size_t at = 0;
    do {  // stored deflate blocks
      const size_t n = std::min<size_t>(65535, f.stored.size() - at);
      o.push_back(at + n == f.stored.size() ? 1 : 0);
      put16(o, uint32_t(n));
      put16(o, uint32_t(~n & 0xFFFF));
      o.insert(o.end(), f.stored.begin() + static_cast<std::ptrdiff_t>(at),
               f.stored.begin() + static_cast<std::ptrdiff_t>(at + n));
      at += n;
    } while (at < f.stored.size());
    const uint32_t a = adler32(f.stored);
    for (int s = 24; s >= 0; s -= 8) o.push_back(static_cast<uint8_t>(a >> s));
    sizes.push_back(uint32_t(o.size() - start));
  }
  const auto dir = uint32_t(o.size());
  for (size_t i = 0; i < files.size(); i++) {
    const DzipFile& f = files[i];
    put32(o, offsets[i]);
    put32(o, sizes[i]);
    put32(o, uint32_t(f.original.size()));
    put16(o, uint32_t(f.name.size() + 1));
    put16(o, 0);
    put32(o, ~ge::core::crc32(f.original.data(), f.original.size()));
    put32(o, f.type);
    put32(o, 0);
    put32(o, uint32_t(f.stored.size()));
    o.insert(o.end(), f.name.begin(), f.name.end());
    o.push_back(0);
  }
  for (int i = 0; i < 4; i++) o[4 + size_t(i)] = static_cast<uint8_t>(dir >> (8 * i));
  for (int i = 0; i < 4; i++) o[8 + size_t(i)] = static_cast<uint8_t>(files.size() >> (8 * i));
  return o;
}

}  // namespace shooters_edge::client
