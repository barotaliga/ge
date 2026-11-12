// The sequel's client recordings on the replay file version 2 (sequel_demo.hpp).
//
// ge:summary The sequel's .dm2 face: the walk, the predictions, the fixture
// ge:keywords sequel dm2 walk predict residual entity-delta playerinfo fixture
// ge:skill replay shooter
// ge:test tests/shooters_edge_sequel_demo_test.cpp
#include "shooters_edge/client/sequel_demo.hpp"

#include <algorithm>
#include <array>

#include "core/prng.hpp"
#include "replay/codec.hpp"

namespace shooters_edge::client {

namespace gr = ge::replay;

namespace {

constexpr int kMostBlock = 1400;  // the largest block a player of the sequel reads
constexpr int kEntities = 1024;

// Server message ids.
constexpr int kMuzzle = 1, kMuzzle2 = 2, kTemp = 3, kLayout = 4, kInventory = 5, kNop = 6;
constexpr int kDisconnect = 7, kReconnect = 8, kSound = 9, kPrint = 10, kStuff = 11;
constexpr int kServerData = 12, kConfig = 13, kBaseline = 14, kCenter = 15, kPlayer = 17;
constexpr int kEntitiesMsg = 18, kFrame = 20;

// An entity delta's flag bits.
constexpr uint32_t kOx = 1u << 0, kOy = 1u << 1, kA2 = 1u << 2, kA3 = 1u << 3, kFrame8 = 1u << 4;
constexpr uint32_t kEvent = 1u << 5, kRemove = 1u << 6, kMore1 = 1u << 7, kNum16 = 1u << 8;
constexpr uint32_t kOz = 1u << 9, kA1 = 1u << 10, kModel = 1u << 11, kRfx8 = 1u << 12;
constexpr uint32_t kFx8 = 1u << 14, kMore2 = 1u << 15, kSkin8 = 1u << 16, kFrame16 = 1u << 17;
constexpr uint32_t kRfx16 = 1u << 18, kFx16 = 1u << 19, kModel2 = 1u << 20, kModel3 = 1u << 21;
constexpr uint32_t kModel4 = 1u << 22, kMore3 = 1u << 23, kOld = 1u << 24, kSkin16 = 1u << 25;
constexpr uint32_t kSnd = 1u << 26, kSolid = 1u << 27;

// The player state's flag bits.
constexpr uint32_t kPmType = 1, kPOrigin = 2, kPVel = 4, kPmTime = 8, kPmFlags = 16, kGrav = 32;
constexpr uint32_t kDelta = 64, kVOfs = 128, kVAng = 256, kKick = 512, kBlend = 1024, kFov = 2048;
constexpr uint32_t kGun = 4096, kGunFrame = 8192, kRdFlags = 16384;

uint32_t mask_of(int n) { return n == 4 ? 0xFFFFFFFFu : (1u << (8 * n)) - 1u; }

// `x` as a signed number of `n` bytes.
int32_t wrap(uint32_t x, int n) {
  if (n == 4) return static_cast<int32_t>(x);
  const int bits = 8 * n;
  x &= (1u << bits) - 1u;
  return x >= (1u << (bits - 1)) ? static_cast<int32_t>(x) - (1 << bits) : static_cast<int32_t>(x);
}

uint32_t linear(uint32_t o, uint32_t po) { return 2u * o - po; }

template <size_t N>
using Ints = std::array<gr::IntModel, N>;

struct Models {
  gr::ByteModel id, text, raw, misc, flag[4], pflag, temp, dir, tbyte, sflags, sidx, sbyte, area;
  gr::ByteModel pbyte, ebyte, model, skin, end_flags;
  gr::IntModel count, rawlen, number, base_number, frame, sframe, dframe, stat, smask, cs, ent16;
  gr::IntModel tshort, tlong, inv, grav, solid, sound_ent, efx, egeneric, gframe;
  Ints<3> eo, po, pv, pa, pd, tpos, spos, old;
  std::array<gr::Prob, 32> end{};
  gr::Prob raw_block = gr::kProbHalf, list_end = gr::kProbHalf;
  Models() { end.fill(gr::kProbHalf); }
};

struct Ent {
  std::array<uint32_t, 3> o{}, po{};
  std::array<uint8_t, 4> fb{};
  uint32_t frame = 0, skin = 0, fx = 0, rfx = 0, solid = 0;
  std::array<uint8_t, 4> model{};
  std::array<uint8_t, 3> a{};
  uint8_t sound = 0, event = 0;
};

struct Pl {
  std::array<uint32_t, 3> o{}, po{}, v{}, a{}, pa{}, d{};
  std::array<uint8_t, 3> vo{}, ka{}, go{}, ga{};
  std::array<uint8_t, 4> blend{}, fb{};
  std::array<uint32_t, 32> stats{};
  std::array<uint8_t, 4> mask{};
  uint32_t grav = 0;
  uint8_t pm = 0, time = 0, pmf = 0, gun = 0, gframe = 0, fov = 0, rd = 0;
};

struct Mem {
  std::vector<Ent> ents = std::vector<Ent>(kEntities);
  Pl pl;
  std::array<uint32_t, 3> tpos{}, spos{};
  std::array<uint8_t, 32> area{};
  std::array<int16_t, 256> inv{};
  uint32_t sframe = 0, mz = 0, mz2 = 0, sent = 0, cs = 0, base = 0, tshort = 0;
  uint8_t prev_id = 0, temp = 0, sflags = 0, sidx = 0, suppress = 0, arealen = 0;
};

}  // namespace

struct SequelFace {
  Mem mem;
  std::unique_ptr<Models> m;  // made by `restart`
  void restart() {
    mem = Mem{};
    m = std::make_unique<Models>();
  }
};

SequelCoder::SequelCoder() : face(std::make_unique<SequelFace>()) { face->restart(); }
SequelCoder::~SequelCoder() = default;
SequelCoder::SequelCoder(SequelCoder&&) noexcept = default;
SequelCoder& SequelCoder::operator=(SequelCoder&&) noexcept = default;

namespace {

// THE IOS. The walk asks each for a field's value: the packer reads it from the block and codes
// it (or, probing, only reads it), the unpacker decodes it and writes its bytes.

struct Src {
  std::span<const uint8_t> b;
  size_t at = 0;
  bool bad = false;
  uint32_t take(int n) {
    const auto un = static_cast<size_t>(n);
    if (at > b.size() || b.size() - at < un) {
      bad = true;
      at = b.size();
      return 0;
    }
    uint32_t v = 0;
    for (size_t i = 0; i < un; i++) v |= static_cast<uint32_t>(b[at + i]) << (8 * i);
    at += un;
    return v;
  }
};

struct PackIo {
  static constexpr bool kPack = true;
  Src s;
  gr::RangeWriter* w = nullptr;  // null: a probe that only reads
  bool ok() const { return !s.bad; }
  void fail() { s.bad = true; }
  bool at_end() const { return s.at >= s.b.size(); }
  // A field of `n` bytes in the block, coded against `pred`.
  uint32_t u(gr::IntModel& m, int n, uint32_t pred) {
    const uint32_t v = s.take(n);
    if (w != nullptr) gr::put_int(*w, m, wrap(v - pred, n));
    return v;
  }
  uint8_t b(gr::ByteModel& m, uint8_t ctx) {
    const auto v = static_cast<uint8_t>(s.take(1));
    if (w != nullptr) gr::put_byte(*w, m, ctx, v);
    return v;
  }
  // A value the walk already read (`v`), coded against `pred`.
  int32_t val(gr::IntModel& m, int32_t pred, int32_t v) {
    if (w != nullptr)
      gr::put_int(*w, m,
                  static_cast<int32_t>(static_cast<uint32_t>(v) - static_cast<uint32_t>(pred)));
    return v;
  }
  uint8_t bval(gr::ByteModel& m, uint8_t ctx, uint8_t v) {
    if (w != nullptr) gr::put_byte(*w, m, ctx, v);
    return v;
  }
  bool flag(gr::Prob& p, bool v) {
    if (w != nullptr) gr::put_bit(*w, p, v ? 1 : 0);
    return v;
  }
  void emit(int, uint32_t) {}
};

struct UnpackIo {
  static constexpr bool kPack = false;
  gr::RangeReader* r = nullptr;
  std::vector<uint8_t>* out = nullptr;
  bool bad = false;
  bool ok() const { return !bad && !r->bad && out->size() < (size_t{1} << 26); }
  void fail() { bad = true; }
  bool at_end() const { return false; }
  uint32_t take(int) { return 0; }
  void emit(int n, uint32_t v) {
    for (int i = 0; i < n; i++) out->push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  uint32_t u(gr::IntModel& m, int n, uint32_t pred) {
    const uint32_t v = (pred + static_cast<uint32_t>(gr::get_int(*r, m))) & mask_of(n);
    emit(n, v);
    return v;
  }
  uint8_t b(gr::ByteModel& m, uint8_t ctx) {
    const uint8_t v = gr::get_byte(*r, m, ctx);
    out->push_back(v);
    return v;
  }
  int32_t val(gr::IntModel& m, int32_t pred, int32_t) {
    return static_cast<int32_t>(static_cast<uint32_t>(pred) +
                                static_cast<uint32_t>(gr::get_int(*r, m)));
  }
  uint8_t bval(gr::ByteModel& m, uint8_t ctx, uint8_t) { return gr::get_byte(*r, m, ctx); }
  bool flag(gr::Prob& p, bool) { return gr::get_bit(*r, p) != 0; }
};

// A string through the text model, its ending 0 too.
template <class Io>
void str(Io& io, Models& m) {
  uint8_t prev = 0;
  for (int i = 0; i < 4096; i++) {
    const uint8_t c = io.b(m.text, prev);
    if (c == 0 || !io.ok()) return;
    prev = c;
  }
  io.fail();
}

template <class Io>
void pos(Io& io, Ints<3>& m, std::array<uint32_t, 3>& last) {
  for (int k = 0; k < 3; k++) last[k] = io.u(m[k], 2, last[k]);
}

// A temporary entity's payload by its kind (the kinds the sequel's player reads).
template <class Io>
void temp_entity(Io& io, SequelFace& f) {
  Models& m = *f.m;
  Mem& mem = f.mem;
  const uint8_t kind = io.b(m.temp, mem.temp);
  mem.temp = kind;
  auto p = [&] { pos(io, m.tpos, mem.tpos); };
  auto d = [&] { io.b(m.dir, kind); };
  auto by = [&] { io.b(m.tbyte, kind); };
  auto sh = [&] { mem.tshort = io.u(m.tshort, 2, mem.tshort); };
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
      p();
      d();
      break;
    case 3:
    case 11:
    case 23:
    case 27:
    case 34:
    case 41:
      p();
      p();
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
      p();
      break;
    case 10:
    case 15:
    case 25:
    case 29:
      by();
      p();
      d();
      by();
      break;
    case 16:
    case 19:
    case 38:
    case 39:
      sh();
      p();
      p();
      break;
    case 24:
      sh();
      p();
      p();
      p();
      break;
    case 33:
      sh();
      sh();
      p();
      p();
      break;
    case 36:
      p();
      sh();
      break;
    case 37:
      p();
      p();
      by();
      break;
    case 40: {
      const uint32_t id = io.u(m.tshort, 2, 0xFFFFu);
      by();
      p();
      d();
      by();
      sh();
      if (id != 0xFFFFu) io.u(m.tlong, 4, 0);
      break;
    }
    case 50:
      sh();
      p();
      break;
    default:
      io.fail();  // a kind the player refuses (31, 32, past 55)
  }
}

// Bits `lo` and `hi` of `flags` pick a byte, a short or a long.
int size_of(uint32_t flags, uint32_t lo, uint32_t hi) {
  const bool l = (flags & lo) != 0, h = (flags & hi) != 0;
  return l && h ? 4 : h ? 2 : l ? 1 : 0;
}

// One entity delta: a baseline's, or an entry of a frame's list (`list`: the entry may be the
// list's end). Its number is coded before its flag bytes, so they are learned per entity. False at
// the list's end.
template <class Io>
bool entity(Io& io, SequelFace& f, bool list, uint32_t& prev) {
  Models& m = *f.m;
  Mem& mem = f.mem;
  uint32_t flags = 0, number = 0;
  if constexpr (Io::kPack) {
    flags = io.s.take(1);
    if ((flags & kMore1) != 0) flags |= io.s.take(1) << 8;
    if ((flags & kMore2) != 0) flags |= io.s.take(1) << 16;
    if ((flags & kMore3) != 0) flags |= io.s.take(1) << 24;
    number = io.s.take((flags & kNum16) != 0 ? 2 : 1);
  }
  const bool end = list && io.flag(m.list_end, number == 0);
  std::array<uint8_t, 4> fb{};
  auto flag_bytes = [&](const std::array<uint8_t, 4>& ctx, gr::ByteModel* models) {
    for (int k = 0; k < 4; k++) {
      if (k > 0 && (fb[static_cast<size_t>(k) - 1] & 0x80) == 0) break;
      fb[static_cast<size_t>(k)] =
          io.bval(models[k], ctx[static_cast<size_t>(k)], static_cast<uint8_t>(flags >> (8 * k)));
    }
    flags = static_cast<uint32_t>(fb[0]) | static_cast<uint32_t>(fb[1]) << 8 |
            static_cast<uint32_t>(fb[2]) << 16 | static_cast<uint32_t>(fb[3]) << 24;
  };
  auto emit_head = [&] {
    if constexpr (!Io::kPack) {
      io.emit(1, fb[0]);
      for (int k = 1; k < 4 && (fb[static_cast<size_t>(k) - 1] & 0x80) != 0; k++)
        io.emit(1, fb[static_cast<size_t>(k)]);
      io.emit((flags & kNum16) != 0 ? 2 : 1, number);
    }
  };
  if (end) {
    for (int k = 0; k < 4; k++) {
      if (k > 0 && (fb[static_cast<size_t>(k) - 1] & 0x80) == 0) break;
      fb[static_cast<size_t>(k)] =
          io.bval(m.end_flags, static_cast<uint8_t>(k), static_cast<uint8_t>(flags >> (8 * k)));
    }
    flags = static_cast<uint32_t>(fb[0]) | static_cast<uint32_t>(fb[1]) << 8 |
            static_cast<uint32_t>(fb[2]) << 16 | static_cast<uint32_t>(fb[3]) << 24;
    number = 0;
    emit_head();
    return false;
  }
  number =
      static_cast<uint32_t>(io.val(list ? m.number : m.base_number, static_cast<int32_t>(prev + 1),
                                   static_cast<int32_t>(number)));
  if (number == 0 || number >= static_cast<uint32_t>(kEntities)) {
    io.fail();
    return false;
  }
  prev = number;
  Ent& e = mem.ents[number];
  flag_bytes(e.fb, m.flag);
  if ((flags & kNum16) == 0 && number > 255) {
    io.fail();
    return false;
  }
  emit_head();
  e.fb = fb;
  if ((flags & kRemove) != 0) return true;  // nothing follows a removal
  if ((flags & kModel) != 0) e.model[0] = io.b(m.model, e.model[0]);
  if ((flags & kModel2) != 0) e.model[1] = io.b(m.model, e.model[1]);
  if ((flags & kModel3) != 0) e.model[2] = io.b(m.model, e.model[2]);
  if ((flags & kModel4) != 0) e.model[3] = io.b(m.model, e.model[3]);
  if ((flags & kFrame8) != 0) e.frame = io.u(m.frame, 1, e.frame + 1);
  if ((flags & kFrame16) != 0) e.frame = io.u(m.frame, 2, e.frame + 1);
  if (const int n = size_of(flags, kSkin8, kSkin16); n > 0) e.skin = io.u(m.efx, n, e.skin);
  if (const int n = size_of(flags, kFx8, kFx16); n > 0) e.fx = io.u(m.efx, n, e.fx);
  if (const int n = size_of(flags, kRfx8, kRfx16); n > 0) e.rfx = io.u(m.efx, n, e.rfx);
  const uint32_t axes[3] = {kOx, kOy, kOz};
  for (int k = 0; k < 3; k++) {
    if ((flags & axes[k]) == 0) continue;
    const uint32_t v = io.u(m.eo[k], 2, linear(e.o[k], e.po[k]));
    e.po[k] = e.o[k];
    e.o[k] = v;
  }
  const uint32_t angles[3] = {kA1, kA2, kA3};
  for (int k = 0; k < 3; k++)
    if ((flags & angles[k]) != 0) e.a[k] = io.b(m.ebyte, e.a[k]);
  if ((flags & kOld) != 0) {
    for (int k = 0; k < 3; k++) io.u(m.old[k], 2, e.po[k]);
  }
  if ((flags & kSnd) != 0) e.sound = io.b(m.ebyte, e.sound);
  if ((flags & kEvent) != 0) e.event = io.b(m.ebyte, e.event);
  if ((flags & kSolid) != 0) e.solid = io.u(m.solid, 2, e.solid);
  return true;
}

template <class Io>
void player(Io& io, SequelFace& f) {
  Models& m = *f.m;
  Pl& p = f.mem.pl;
  std::array<uint8_t, 2> fb{};
  for (int k = 0; k < 2; k++)
    fb[static_cast<size_t>(k)] = io.b(m.pflag, p.fb[static_cast<size_t>(k)]);
  p.fb[0] = fb[0];
  p.fb[1] = fb[1];
  const uint32_t flags = static_cast<uint32_t>(fb[0]) | static_cast<uint32_t>(fb[1]) << 8;
  if ((flags & kPmType) != 0) p.pm = io.b(m.pbyte, p.pm);
  if ((flags & kPOrigin) != 0)
    for (int k = 0; k < 3; k++) {
      const uint32_t v = io.u(m.po[k], 2, linear(p.o[k], p.po[k]));
      p.po[k] = p.o[k];
      p.o[k] = v;
    }
  if ((flags & kPVel) != 0)
    for (int k = 0; k < 3; k++) p.v[k] = io.u(m.pv[k], 2, p.v[k]);
  if ((flags & kPmTime) != 0) p.time = io.b(m.pbyte, p.time);
  if ((flags & kPmFlags) != 0) p.pmf = io.b(m.pbyte, p.pmf);
  if ((flags & kGrav) != 0) p.grav = io.u(m.grav, 2, p.grav);
  if ((flags & kDelta) != 0)
    for (int k = 0; k < 3; k++) p.d[k] = io.u(m.pd[k], 2, p.d[k]);
  if ((flags & kVOfs) != 0)
    for (auto& v : p.vo) v = io.b(m.pbyte, v);
  if ((flags & kVAng) != 0)
    for (int k = 0; k < 3; k++) {
      const uint32_t v = io.u(m.pa[k], 2, linear(p.a[k], p.pa[k]));
      p.pa[k] = p.a[k];
      p.a[k] = v;
    }
  if ((flags & kKick) != 0)
    for (auto& v : p.ka) v = io.b(m.pbyte, v);
  if ((flags & kGun) != 0) p.gun = io.b(m.pbyte, p.gun);
  if ((flags & kGunFrame) != 0) {
    p.gframe = static_cast<uint8_t>(io.u(m.gframe, 1, p.gframe + 1u));
    for (auto& v : p.go) v = io.b(m.pbyte, v);
    for (auto& v : p.ga) v = io.b(m.pbyte, v);
  }
  if ((flags & kBlend) != 0)
    for (auto& v : p.blend) v = io.b(m.pbyte, v);
  if ((flags & kFov) != 0) p.fov = io.b(m.pbyte, p.fov);
  if ((flags & kRdFlags) != 0) p.rd = io.b(m.pbyte, p.rd);
  uint32_t mask = 0;
  for (int k = 0; k < 4; k++) {
    p.mask[static_cast<size_t>(k)] = io.b(m.misc, p.mask[static_cast<size_t>(k)]);
    mask |= static_cast<uint32_t>(p.mask[static_cast<size_t>(k)]) << (8 * k);
  }
  for (int i = 0; i < 32; i++)
    if ((mask & (1u << i)) != 0)
      p.stats[static_cast<size_t>(i)] = io.u(m.stat, 2, p.stats[static_cast<size_t>(i)]);
}

template <class Io>
void frame(Io& io, SequelFace& f) {
  Models& m = *f.m;
  Mem& mem = f.mem;
  mem.sframe = io.u(m.sframe, 4, mem.sframe + 1);
  io.u(m.dframe, 4, mem.sframe - 1);
  mem.suppress = io.b(m.misc, mem.suppress);
  mem.arealen = io.b(m.misc, mem.arealen);
  for (int i = 0; i < mem.arealen; i++)
    mem.area[static_cast<size_t>(i) & 31] = io.b(m.area, mem.area[static_cast<size_t>(i) & 31]);
}

template <class Io>
void sound(Io& io, SequelFace& f) {
  Models& m = *f.m;
  Mem& mem = f.mem;
  const uint8_t flags = io.b(m.sflags, mem.sflags);
  mem.sflags = flags;
  mem.sidx = io.b(m.sidx, mem.sidx);
  if ((flags & 1) != 0) io.b(m.sbyte, 1);
  if ((flags & 2) != 0) io.b(m.sbyte, 2);
  if ((flags & 16) != 0) io.b(m.sbyte, 16);
  if ((flags & 8) != 0) mem.sent = io.u(m.sound_ent, 2, mem.sent);
  if ((flags & 4) != 0) {
    const uint32_t e = (mem.sent & 0xFFFFu) >> 3;
    std::array<uint32_t, 3> pred = mem.spos;
    if ((flags & 8) != 0 && e < static_cast<uint32_t>(kEntities)) pred = mem.ents[e].o;
    for (int k = 0; k < 3; k++) mem.spos[k] = io.u(m.spos[k], 2, pred[k]);
  }
}

// One block's messages. False when it does not walk whole (the packer keeps such a block as its
// bytes).
template <class Io>
bool walk(Io& io, SequelFace& f, size_t* messages) {
  Models& m = *f.m;
  Mem& mem = f.mem;
  uint8_t prev = 0;
  for (int count = 0; count < 2048; count++) {
    bool done = false;
    if constexpr (Io::kPack) done = io.at_end();
    if (io.flag(m.end[prev & 31u], done)) return io.ok();
    const uint8_t id = io.b(m.id, prev);
    prev = id;
    if (messages != nullptr) (*messages)++;
    switch (id) {
      case kMuzzle:
        mem.mz = io.u(m.ent16, 2, mem.mz);
        io.b(m.misc, 100);
        break;
      case kMuzzle2:
        mem.mz2 = io.u(m.ent16, 2, mem.mz2);
        io.b(m.misc, 101);
        break;
      case kTemp:
        temp_entity(io, f);
        break;
      case kLayout:
      case kStuff:
      case kCenter:
        str(io, m);
        break;
      case kInventory:
        for (auto& v : mem.inv) v = static_cast<int16_t>(io.u(m.inv, 2, static_cast<uint16_t>(v)));
        break;
      case kNop:
      case kDisconnect:
      case kReconnect:
        break;
      case kSound:
        sound(io, f);
        break;
      case kPrint:
        io.b(m.misc, 102);
        str(io, m);
        break;
      case kServerData:
        io.u(m.egeneric, 4, 34);
        io.u(m.egeneric, 4, 0);
        io.b(m.misc, 103);
        str(io, m);
        io.u(m.egeneric, 2, 0);
        str(io, m);
        break;
      case kConfig:
        mem.cs = io.u(m.cs, 2, mem.cs + 1);
        str(io, m);
        break;
      case kBaseline:
        entity(io, f, false, mem.base);
        break;
      case kPlayer:
        player(io, f);
        break;
      case kEntitiesMsg: {
        uint32_t last = 0;
        for (int n = 0; n < 2 * kEntities && io.ok(); n++)
          if (!entity(io, f, true, last)) break;
        break;
      }
      case kFrame:
        frame(io, f);
        break;
      default:
        io.fail();
    }
    if (!io.ok()) return false;
  }
  io.fail();
  return false;
}

}  // namespace

bool is_sequel_recording(std::span<const uint8_t> bytes) {
  size_t at = 0;
  int blocks = 0;
  while (at + 4 <= bytes.size()) {
    const auto len = static_cast<int32_t>(
        static_cast<uint32_t>(bytes[at]) | static_cast<uint32_t>(bytes[at + 1]) << 8 |
        static_cast<uint32_t>(bytes[at + 2]) << 16 | static_cast<uint32_t>(bytes[at + 3]) << 24);
    at += 4;
    if (len == -1) return blocks > 0 && at == bytes.size();
    if (len <= 0 || len > kMostBlock || bytes.size() - at < static_cast<size_t>(len)) return false;
    if (blocks == 0 && bytes[at] != kServerData) return false;
    at += static_cast<size_t>(len);
    blocks++;
  }
  return false;
}

std::vector<std::span<const uint8_t>> sequel_blocks(std::span<const uint8_t> recording) {
  std::vector<std::span<const uint8_t>> out;
  size_t at = 0;
  while (at + 4 <= recording.size()) {
    const auto len = static_cast<int32_t>(static_cast<uint32_t>(recording[at]) |
                                          static_cast<uint32_t>(recording[at + 1]) << 8 |
                                          static_cast<uint32_t>(recording[at + 2]) << 16 |
                                          static_cast<uint32_t>(recording[at + 3]) << 24);
    at += 4;
    if (len <= 0 || recording.size() - at < static_cast<size_t>(len)) break;
    out.push_back(recording.subspan(at, static_cast<size_t>(len)));
    at += static_cast<size_t>(len);
  }
  return out;
}

void pack_sequel_chunk(SequelCoder& c, const std::vector<std::span<const uint8_t>>& blocks,
                       bool keyframe, std::vector<uint8_t>& chunk, SequelPackReport* report) {
  SequelFace& f = *c.face;
  if (keyframe) f.restart();
  Models& m = *f.m;
  chunk.clear();
  gr::RangeWriter w;
  gr::rc_begin(w, chunk);
  gr::put_int(w, m.count, static_cast<int32_t>(blocks.size()));
  for (const std::span<const uint8_t> b : blocks) {
    // A probe first, on a copy of the predictions: does the block walk whole?
    SequelFace probe;
    probe.mem = f.mem;
    probe.m.swap(f.m);
    PackIo pi;
    pi.s.b = b;
    const bool whole = walk(pi, probe, nullptr) && pi.s.at == b.size();
    f.m.swap(probe.m);
    gr::put_bit(w, m.raw_block, whole ? 0 : 1);
    if (whole) {
      PackIo io;
      io.s.b = b;
      io.w = &w;
      walk(io, f, report != nullptr ? &report->messages : nullptr);
    } else {
      gr::put_int(w, m.rawlen, static_cast<int32_t>(b.size()));
      uint8_t prev = 0;
      for (const uint8_t x : b) {
        gr::put_byte(w, m.raw, prev, x);
        prev = x;
      }
    }
    if (report != nullptr) {
      report->blocks++;
      (whole ? report->walked : report->kept)++;
    }
  }
  gr::rc_end(w);
  if (report != nullptr) {
    report->chunks++;
    if (keyframe) report->keyframes++;
  }
}

bool unpack_sequel_chunk(SequelCoder& c, std::span<const uint8_t> chunk, bool keyframe,
                         std::vector<uint8_t>& out, std::string& error) {
  SequelFace& f = *c.face;
  if (keyframe) f.restart();
  Models& m = *f.m;
  gr::RangeReader r;
  gr::rc_begin(r, chunk.data(), chunk.size());
  const int32_t count = gr::get_int(r, m.count);
  if (count < 0 || count > 10000) {
    error = "a sequel chunk says it holds " + std::to_string(count) + " blocks: it is damaged";
    return false;
  }
  std::vector<uint8_t> block;
  for (int32_t i = 0; i < count; i++) {
    block.clear();
    if (gr::get_bit(r, m.raw_block) != 0) {
      const int32_t n = gr::get_int(r, m.rawlen);
      if (n <= 0 || n > 1 << 20) {
        error = "a sequel chunk's kept block says it is " + std::to_string(n) + " bytes: damaged";
        return false;
      }
      uint8_t prev = 0;
      for (int32_t k = 0; k < n; k++) {
        prev = gr::get_byte(r, m.raw, prev);
        block.push_back(prev);
      }
    } else {
      UnpackIo io;
      io.r = &r;
      io.out = &block;
      if (!walk(io, f, nullptr)) {
        error = "a sequel chunk's block " + std::to_string(i) + " did not decode: damaged";
        return false;
      }
    }
    if (r.bad) {
      error = "a sequel chunk ended inside its block " + std::to_string(i) + ": it is cut short";
      return false;
    }
    const auto n = static_cast<uint32_t>(block.size());
    for (int k = 0; k < 4; k++) out.push_back(static_cast<uint8_t>(n >> (8 * k)));
    out.insert(out.end(), block.begin(), block.end());
  }
  return true;
}

// ---- The fixture ----

namespace {

struct Writer {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void s16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void s32(int32_t v) {
    s16(v & 0xFFFF);
    s16((v >> 16) & 0xFFFF);
  }
  void text(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
};

void put_block(std::vector<uint8_t>& file, const std::vector<uint8_t>& block) {
  const auto n = static_cast<uint32_t>(block.size());
  for (int k = 0; k < 4; k++) file.push_back(static_cast<uint8_t>(n >> (8 * k)));
  file.insert(file.end(), block.begin(), block.end());
}

struct Thing {
  int number = 0;
  int x = 0, y = 0, z = 0, vx = 0, vy = 0;
  int frame = 0, yaw = 0, model = 1;
};

void entity_out(Writer& w, int number, uint32_t flags, const Thing& t) {
  if (number > 255) flags |= kNum16;
  if ((flags & 0xFF000000u) != 0) flags |= kMore3;
  if ((flags & 0xFFFF0000u) != 0) flags |= kMore2;
  if ((flags & 0xFFFFFF00u) != 0) flags |= kMore1;
  w.u8(static_cast<int>(flags & 0xFF));
  if ((flags & kMore1) != 0) w.u8(static_cast<int>((flags >> 8) & 0xFF));
  if ((flags & kMore2) != 0) w.u8(static_cast<int>((flags >> 16) & 0xFF));
  if ((flags & kMore3) != 0) w.u8(static_cast<int>((flags >> 24) & 0xFF));
  if ((flags & kNum16) != 0)
    w.s16(number);
  else
    w.u8(number);
  if ((flags & kRemove) != 0) return;
  if ((flags & kModel) != 0) w.u8(t.model);
  if ((flags & kFrame8) != 0) w.u8(t.frame);
  if ((flags & kOx) != 0) w.s16(t.x);
  if ((flags & kOy) != 0) w.s16(t.y);
  if ((flags & kOz) != 0) w.s16(t.z);
  if ((flags & kA2) != 0) w.u8(t.yaw);
  if ((flags & kOld) != 0) {
    w.s16(t.x);
    w.s16(t.y);
    w.s16(t.z);
  }
}

}  // namespace

std::vector<uint8_t> sequel_fixture(int seconds, uint32_t seed, int players, int things) {
  ge::core::Rng rng = ge::core::make_rng(seed);
  auto pick = [&](int lo, int hi) { return ge::core::int_in(rng, lo, hi); };
  std::vector<uint8_t> file;
  std::vector<Thing> all;
  for (int i = 0; i < players + things; i++) {
    Thing t;
    t.number = i < players ? i + 1 : 40 + i * 7;
    t.x = pick(-8000, 8000);
    t.y = pick(-8000, 8000);
    t.z = pick(-800, 800);
    t.vx = i < players ? pick(-40, 40) : (i % 3 == 0 ? pick(-6, 6) : 0);
    t.vy = i < players ? pick(-40, 40) : 0;
    t.model = i < players ? 255 : pick(2, 60);
    t.yaw = pick(0, 255);
    all.push_back(t);
  }
  // The signon: the server data and the settings strings, then the baselines.
  Writer w;
  w.u8(kServerData);
  w.s32(34);
  w.s32(0x10000 + static_cast<int32_t>(seed & 0xFFFF));
  w.u8(1);
  w.text("");
  w.s16(0);
  w.text("The Test Yard");
  for (int i = 0; i < 40; i++) {
    w.u8(kConfig);
    w.s16(32 + i);
    w.text(i == 0 ? "maps/yard.bsp" : "models/things/thing" + std::to_string(i) + "/tris.md2");
  }
  put_block(file, w.b);
  w = {};
  for (const Thing& t : all) {
    w.u8(kBaseline);
    entity_out(w, t.number, kModel | kOx | kOy | kOz | kA2 | kOld, t);
  }
  w.u8(kStuff);
  w.text("precache\n");
  put_block(file, w.b);
  // The frames: ten a second, each a delta from the one before.
  Thing me = all[0];
  int stat_health = 100, ammo = 50;
  for (int f = 0; f < seconds * 10; f++) {
    w = {};
    w.u8(kFrame);
    w.s32(100 + f);
    w.s32(f == 0 ? -1 : 99 + f);
    w.u8(0);
    w.u8(1);
    w.u8(0xFF);
    w.u8(kPlayer);
    me.x += me.vx;
    me.y += me.vy;
    if (pick(0, 20) == 0) me.vx = pick(-40, 40);
    if (pick(0, 20) == 0) me.vy = pick(-40, 40);
    w.s16(static_cast<int>(kPOrigin | kPVel | kVAng | kGun | (f % 3 == 0 ? kGunFrame : 0)));
    w.s16(me.x);
    w.s16(me.y);
    w.s16(me.z);
    w.s16(me.vx * 10);
    w.s16(me.vy * 10);
    w.s16(0);
    w.s16(me.yaw * 256);
    w.s16((me.yaw + f) * 64);
    w.s16(0);
    w.u8(7);
    if (f % 3 == 0) {
      w.u8(f % 12);
      for (int k = 0; k < 6; k++) w.u8(0);
    }
    const bool hurt = pick(0, 15) == 0, shot = pick(0, 4) == 0;
    if (hurt) stat_health = std::max(1, stat_health - pick(1, 30));
    if (shot) ammo = std::max(0, ammo - 1);
    w.s32((hurt ? 1 << 1 : 0) | (shot ? 1 << 3 : 0));
    if (hurt) w.s16(stat_health);
    if (shot) w.s16(ammo);
    w.u8(kEntitiesMsg);
    for (Thing& t : all) {
      if (t.number == 1) continue;
      uint32_t flags = 0;
      if (t.vx != 0 || t.vy != 0) {
        t.x += t.vx;
        t.y += t.vy;
        flags |= kOx | kOy;
        if (pick(0, 25) == 0) t.vx = pick(-40, 40);
      }
      if (t.number <= players) {
        flags |= kOld | kFrame8;
        t.frame = (t.frame + 1) % 40;
        if (pick(0, 3) == 0) {
          t.yaw = (t.yaw + pick(-12, 12)) & 255;
          flags |= kA2;
        }
      }
      if (flags != 0) entity_out(w, t.number, flags, t);
    }
    w.u8(0);
    w.u8(0);
    if (shot) {
      w.u8(kMuzzle);
      w.s16(1);
      w.u8(2);
      w.u8(kSound);
      w.u8(8);
      w.u8(pick(1, 40));
      w.s16(1 * 8 + 1);
      w.u8(kTemp);
      w.u8(0);
      w.s16(me.x + pick(-400, 400));
      w.s16(me.y + pick(-400, 400));
      w.s16(me.z);
      w.u8(pick(0, 161));
    }
    if (f % 97 == 50) {
      w.u8(kPrint);
      w.u8(1);
      w.text("Player" + std::to_string(pick(2, players)) + " died.\n");
    }
    put_block(file, w.b);
  }
  for (int k = 0; k < 4; k++) file.push_back(0xFF);
  return file;
}

}  // namespace shooters_edge::client
