// The classic recordings' face on the replay file version 2 (demo_v2.hpp): the record and message
// walk, written once over three IOs (the probe that checks a body walks whole, the packer that
// codes it, the unpacker that writes it back), the predictions and the models.
//
// ge:summary Shooter's Edge's replay v2 face: the multiview walk over probe, packer and unpacker
// ge:keywords replay-v2 face field-codec multiview walk probe residual predict entity player
// ge:skill replay shooter
// ge:test tests/shooters_edge_demo_v2_test.cpp
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/client/sequel_tv.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <unordered_map>

#include "core/libm/libm.hpp"
#include "replay/codec.hpp"
#include "replay/stream.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_caption.hpp"

namespace shooters_edge::client {

namespace {

namespace gr = ge::replay;

constexpr int kSet = 2, kMultiple = 3, kAll = 6;
constexpr uint32_t kMostBody = 1u << 20;  // a record's body past this is damage, bytes
constexpr uint32_t kFamilyFte = 0x58455446, kFamilyFte2 = 0x32455446, kFamilyMvd = 0x3144564D;
constexpr uint32_t kFteFloatCoords = 0x8000, kFteChunkedDownloads = 0x20000000;
constexpr uint32_t kMvdFloatCoords = 0x1, kMvdHighLagTeleport = 0x2;
constexpr uint32_t kMultiviewTrack = 1, kBytesTrack = 2;  // kSequelTrack: sequel_tv.hpp
constexpr size_t kBytesBlock = size_t{256} << 10;
constexpr int64_t kBlockMs = 1000;
constexpr const char* kGame = "shooters_edge";

// An entity delta's flags (demo_client.cpp's), as they sit in the word and the bytes after it.
constexpr uint32_t kOrigin1 = 1u << 9, kOrigin2 = 1u << 10, kOrigin3 = 1u << 11, kAngle2 = 1u << 12;
constexpr uint32_t kFrame = 1u << 13, kRemove = 1u << 14, kMoreBits = 1u << 15;
constexpr uint32_t kAngle1 = 1u << 0, kAngle3 = 1u << 1, kModel = 1u << 2, kColormap = 1u << 3;
constexpr uint32_t kSkin = 1u << 4, kEffects = 1u << 5, kEvenMore = 1u << 7, kTrans = 1u << 17;
constexpr uint32_t kModelDbl = 1u << 19, kPlus512 = 1u << 21, kPlus1024 = 1u << 22;
constexpr uint32_t kYetMore = 1u << 23, kColourMod = 1u << 26;

// The extension bits a body's layout hangs on, set by the server data.
struct Shape {
  uint32_t fte = 0, mvd = 0;
  bool float_coords() const { return (fte & kFteFloatCoords) != 0; }
  bool float_origins() const { return (mvd & kMvdFloatCoords) != 0 || float_coords(); }
};

uint8_t size_ctx(int32_t residual) {
  const int n = gr::int_length(residual);
  return static_cast<uint8_t>(n > 15 ? 15 : n);
}

// `x` as a signed number of `n` bytes.
int32_t wrap(uint32_t x, int n) {
  if (n == 4) return static_cast<int32_t>(x);
  const int bits = 8 * n;
  x &= (1u << bits) - 1u;
  return x >= (1u << (bits - 1)) ? static_cast<int32_t>(x) - (1 << bits) : static_cast<int32_t>(x);
}
uint32_t mask_of(int n) { return n == 4 ? 0xFFFFFFFFu : (1u << (8 * n)) - 1u; }

// A float coordinate's eighths, the value its prediction runs on (0 for one out of range).
int32_t eighths(uint32_t bits) {
  const auto f = std::bit_cast<float>(bits);
  if (!(f < 1048576.0f && f > -1048576.0f)) return 0;
  return static_cast<int32_t>(f * 8.0f);
}
// Whether `bits` is exactly `k` eighths, so it can be written as `k`.
bool exact_eighths(uint32_t bits, int32_t& k) {
  k = eighths(bits);
  return std::bit_cast<uint32_t>(static_cast<float>(k) * 0.125f) == bits;
}

// The line through the last two values (written at `t` and `pt`) carried on to `now`, at most twice
// the last step's time ahead: a player is written when he changes, not every frame.
int64_t ahead(int64_t d, int64_t t, int64_t pt, int64_t now) {
  const int64_t step = t - pt, dt = now - t;
  if (step <= 0 || dt <= 0) return d;
  return d * std::min(dt, 2 * step) / step;
}

int32_t linear(int32_t o, int32_t po) {
  return static_cast<int32_t>(2u * static_cast<uint32_t>(o) - static_cast<uint32_t>(po));
}

// What the walk remembers to predict from; all of it starts over at a keyframe.
struct Ent {
  std::array<int32_t, 3> o{}, po{};
  std::array<uint32_t, 3> a{};
  uint32_t model = 0, frame = 0;
  uint8_t hi = 0, more = 0, skin = 0, effects = 0, colormap = 0;
  std::array<uint8_t, 3> oc{};
};
struct Pl {
  std::array<int32_t, 3> o{}, po{};
  std::array<int64_t, 3> ot{}, pot{}, at{}, pat{};  // when each was written, ms
  std::array<uint32_t, 3> a{}, pa{};
  uint32_t frame = 0, ping = 0, frags = 0, wframe = 0;
  uint8_t lo = 0, hi = 0, model = 0, skin = 0, effects = 0;
  std::array<uint8_t, 3> oc{}, ac{};
};

template <size_t N>
using Ints = std::array<gr::IntModel, N>;

struct Models {
  // Bytes under a context: the record's head, the message ids, text, what has no model.
  gr::ByteModel ms, type, id, first_id, text, misc, raw, slot, stat_key;
  gr::ByteModel ent_hi, ent_more, ent_skin, ent_fx, ent_byte, pl_lo, pl_hi, pl_byte, sound, temp;
  gr::IntModel len, ent_n, ent_model, ent_frame, ent_angle, pl_frame, pl_wframe, stat, ping, frags;
  gr::IntModel number, set0, set1, tpl, tpl_count, seq;
  Ints<16> tnum;
  std::array<Ints<16>, 3> ent_o, pl_o, pl_a;  // by the axis's last residual's size
  Ints<3> point;
  gr::Prob more = gr::kProbHalf, tail = gr::kProbHalf, same_mask = gr::kProbHalf;
  gr::Prob float_raw = gr::kProbHalf;
  std::array<gr::Prob, 8> walked{};
  std::array<gr::Prob, 4> word_same{};
  std::array<gr::Prob, 256> end{}, end_first{}, type_hit{};
  Models() {
    walked.fill(gr::kProbHalf);
    word_same.fill(gr::kProbHalf);
    end.fill(gr::kProbHalf);
    end_first.fill(gr::kProbHalf);
    type_hit.fill(gr::kProbHalf);
  }
};

// TEXT is written as a template (its text with the whole numbers taken out) and its numbers: a
// template seen lately is its place in a list of the last 256, most recent first, and each number
// is the residual from the same template's last numbers (those of the last string that began with
// the same first number, when there was one: a player's slot, say). A team's info line repeated
// every few frames costs a few bits for what changed.
struct Template {
  std::string lits;  // count + 1 literals, each ended by a 0
  size_t count = 0;
  std::vector<int32_t> last;
  std::unordered_map<int32_t, std::vector<int32_t>> by_first;
};
constexpr size_t kTemplates = 256, kMostNumbers = 64;

struct Texts {
  std::vector<Template> mru;
  void to_front(size_t pos) {
    std::rotate(mru.begin(), mru.begin() + static_cast<std::ptrdiff_t>(pos),
                mru.begin() + static_cast<std::ptrdiff_t>(pos) + 1);
  }
  void add(Template t) {
    mru.insert(mru.begin(), std::move(t));
    if (mru.size() > kTemplates) mru.pop_back();
  }
  static int32_t predict(const Template& t, size_t j, int32_t first) {
    if (j > 0) {
      const auto it = t.by_first.find(first);
      if (it != t.by_first.end()) return it->second[j];
    }
    return t.last.empty() ? 0 : t.last[j];
  }
  static void remember(Template& t, const std::vector<int32_t>& nums) {
    t.last = nums;
    if (!nums.empty()) t.by_first[nums[0]] = nums;
  }
};

bool is_digit(uint8_t c) { return c >= '0' && c <= '9'; }

// `str` as its literals (each ended by a 0) and its whole numbers: a run of up to 9 digits, a
// minus before it, written as the number prints (no leading zero, no "-0"), so the string is the
// literals and the numbers in turn.
void split(std::span<const uint8_t> str, std::string& lits, std::vector<int32_t>& nums) {
  lits.clear();
  nums.clear();
  std::string lit;
  const size_t n = str.size();
  for (size_t i = 0; i < n;) {
    const size_t j = i + (str[i] == '-' ? 1 : 0);
    if (j < n && is_digit(str[j])) {
      size_t k = j;
      while (k < n && is_digit(str[k])) k++;
      const size_t len = k - j;
      const bool canonical = len <= 9 && (len == 1 ? !(j > i && str[j] == '0') : str[j] != '0');
      if (canonical) {
        int32_t v = 0;
        for (size_t d = j; d < k; d++) v = v * 10 + (str[d] - '0');
        nums.push_back(j > i ? -v : v);
        lits += lit;
        lits += '\0';
        lit.clear();
      } else {
        lit.append(reinterpret_cast<const char*>(str.data() + i), k - i);
      }
      i = k;
      continue;
    }
    lit += static_cast<char>(str[i]);
    i++;
  }
  lits += lit;
  lits += '\0';
  if (nums.size() > kMostNumbers) {  // too many to model: the whole string as one literal
    lits.assign(reinterpret_cast<const char*>(str.data()), n);
    lits += '\0';
    nums.clear();
  }
}

// An entity list's word and the number it names, kept to guess the next list's words.
struct ListWord {
  uint32_t word = 0, n = 0;
};
constexpr uint32_t kMissed = 0x10000;  // what a guess returns when the word is not the guess

struct Face {
  Shape shape;
  std::vector<ListWord> list;
  Texts texts;
  std::vector<Ent> ents = std::vector<Ent>(2048);
  std::array<Pl, 32> pl{};
  std::array<std::array<uint32_t, 256>, 32> stats{};
  std::array<int32_t, 3> point{};
  std::array<uint32_t, 2> set{};
  uint32_t mask = 0;
  uint8_t ms = 0, type = 0, slot = 0, stat_key = 0;
  int64_t clock = 0;                  // ms since the keyframe, the records' times summed
  uint32_t seq = 0;                   // the last delta list's sequence byte
  std::array<uint8_t, 256> follow{};  // the kind that last came after each kind
  std::unique_ptr<Models> m = std::make_unique<Models>();

  // Everything but the shape starts over (a keyframe).
  void restart() {
    std::fill(ents.begin(), ents.end(), Ent{});
    pl.fill(Pl{});
    for (auto& s : stats) s.fill(0);
    point = {};
    set = {};
    mask = 0;
    ms = type = slot = stat_key = 0;
    clock = 0;
    seq = 0;
    follow.fill(0);
    list.clear();
    texts = {};
    m = std::make_unique<Models>();
  }
};

// THE IOS. Each gives the walk a field's value; the packer codes what it reads, the unpacker
// writes what it decodes, the probe only reads. `kLive` says the walk may update its memory.

struct Src {
  std::span<const uint8_t> b;
  size_t at = 0, end = 0;
  bool bad = false;
  uint32_t take(int n) {
    const auto un = static_cast<size_t>(n);
    if (at > end || end - at < un) {
      bad = true;
      at = end;
      return 0;
    }
    uint32_t v = 0;
    for (size_t i = 0; i < un; i++) v |= static_cast<uint32_t>(b[at + i]) << (8 * i);
    at += un;
    return v;
  }
};

struct Probe {
  static constexpr bool kLive = false;
  Src s;
  Shape own;
  double mark() const { return 0; }
  void spend(uint32_t, double) {}
  void part(DemoPart, double) {}
  Shape& shape(Face&) { return own; }
  bool ok() const { return !s.bad; }
  bool body_end(gr::Prob&) { return s.at >= s.end; }
  uint32_t byte(gr::ByteModel&, uint8_t) { return s.take(1); }
  uint32_t num(gr::IntModel&, int n, uint32_t) { return s.take(n); }
  uint32_t raw(int n) { return s.take(n); }
  int32_t fcoord(gr::IntModel&, gr::Prob&, int32_t) { return eighths(s.take(4)); }
  uint32_t guess16(gr::Prob&, uint32_t guess) {
    if (s.at + 2 <= s.end && (s.b[s.at] | s.b[s.at + 1] << 8) == static_cast<int>(guess)) {
      s.at += 2;
      return guess;
    }
    return kMissed;
  }
  size_t string(Face&) {
    const size_t from = s.at;
    while (s.at < s.end && s.b[s.at] != 0) s.at++;
    if (s.at >= s.end) {
      s.bad = true;
      return 0;
    }
    s.at++;
    return s.at - from - 1;
  }
  template <class Ctx>
  uint32_t pair16(gr::IntModel&, int32_t, gr::ByteModel&, Ctx) {
    return s.take(2);
  }
};

struct Pack {
  static constexpr bool kLive = true;
  Src s;
  gr::RangeWriter w;
  DemoPackReport* rep = nullptr;  // where the bits each message and part took are added up
  // The bits written so far, to a fraction of a bit: the bytes out and held, less what the range
  // has left.
  double mark() const {
    if (rep == nullptr) return 0;
    return 8.0 * static_cast<double>(w.out->size() + w.cache_size) -
           ge::core::libm::log2(static_cast<double>(w.range));
  }
  void spend(uint32_t id, double from) {
    if (rep != nullptr) rep->message_bytes[id & 255u] += (mark() - from) / 8;
  }
  void part(DemoPart k, double from) {
    if (rep != nullptr) rep->part_bytes[static_cast<size_t>(k)] += (mark() - from) / 8;
  }
  Shape& shape(Face& f) { return f.shape; }
  bool ok() const { return !s.bad; }
  bool body_end(gr::Prob& p) {
    const int bit = s.at >= s.end ? 1 : 0;
    gr::put_bit(w, p, bit);
    return bit != 0;
  }
  uint32_t byte(gr::ByteModel& m, uint8_t ctx) {
    const uint32_t v = s.take(1);
    gr::put_byte(w, m, ctx, static_cast<uint8_t>(v));
    return v;
  }
  uint32_t num(gr::IntModel& m, int n, uint32_t pred) {
    const uint32_t v = s.take(n);
    gr::put_int(w, m, wrap(v - pred, n));
    return v;
  }
  uint32_t raw(int n) {
    const uint32_t v = s.take(n);
    gr::put_raw(w, v, 8 * n);
    return v;
  }
  int32_t fcoord(gr::IntModel& m, gr::Prob& p, int32_t pred) {
    const uint32_t v = s.take(4);
    int32_t k = 0;
    if (exact_eighths(v, k)) {
      gr::put_bit(w, p, 0);
      gr::put_int(w, m,
                  static_cast<int32_t>(static_cast<uint32_t>(k) - static_cast<uint32_t>(pred)));
    } else {
      gr::put_bit(w, p, 1);
      gr::put_raw(w, v, 32);
    }
    return k;
  }
  // A 16-bit word guessed: one bit when it is the guess, else `kMissed` and nothing read.
  uint32_t guess16(gr::Prob& p, uint32_t guess) {
    const bool hit =
        s.at + 2 <= s.end && (s.b[s.at] | s.b[s.at + 1] << 8) == static_cast<int>(guess);
    gr::put_bit(w, p, hit ? 0 : 1);
    if (!hit) return kMissed;
    s.at += 2;
    return guess;
  }
  size_t string(Face& f) {
    Models& m = *f.m;
    const size_t from = s.at;
    while (s.at < s.end && s.b[s.at] != 0) s.at++;
    const auto str = s.b.subspan(from, s.at - from);
    s.at++;  // the 0
    std::string lits;
    std::vector<int32_t> nums;
    split(str, lits, nums);
    Texts& tx = f.texts;
    size_t pos = 0;
    while (pos < tx.mru.size() && tx.mru[pos].lits != lits) pos++;
    if (pos < tx.mru.size()) {
      gr::put_int(w, m.tpl, static_cast<int32_t>(pos + 1));
      tx.to_front(pos);
    } else {
      gr::put_int(w, m.tpl, 0);
      gr::put_int(w, m.tpl_count, static_cast<int32_t>(nums.size()));
      uint8_t prev = 0;
      for (const char c : lits) {
        gr::put_byte(w, m.text, prev, static_cast<uint8_t>(c));
        prev = static_cast<uint8_t>(c);
      }
      Template t;
      t.lits = std::move(lits);
      t.count = nums.size();
      tx.add(std::move(t));
    }
    Template& t = tx.mru[0];
    for (size_t j = 0; j < nums.size(); j++) {
      const int32_t pred = Texts::predict(t, j, nums[0]);
      gr::put_int(
          w, m.tnum[std::min<size_t>(j, 15)],
          static_cast<int32_t>(static_cast<uint32_t>(nums[j]) - static_cast<uint32_t>(pred)));
    }
    Texts::remember(t, nums);
    return str.size();
  }
  // A 16-bit word as its low 9 bits (a number, against `pred`) and its top 7 (a byte under the
  // context `ctx` gives for the number).
  template <class Ctx>
  uint32_t pair16(gr::IntModel& nm, int32_t pred, gr::ByteModel& hm, Ctx ctx) {
    const uint32_t v = s.take(2);
    const uint32_t n = v & 511u;
    gr::put_int(w, nm, static_cast<int32_t>(n) - pred);
    gr::put_byte(w, hm, ctx(n), static_cast<uint8_t>(v >> 9));
    return v;
  }
};

struct Unpack {
  static constexpr bool kLive = true;
  gr::RangeReader r;
  std::vector<uint8_t>* out = nullptr;
  size_t body = 0;  // where the body being written starts
  bool fault = false;
  double mark() const { return 0; }
  void spend(uint32_t, double) {}
  void part(DemoPart, double) {}
  Shape& shape(Face& f) { return f.shape; }
  bool ok() const { return !r.bad && !fault && out->size() - body <= kMostBody; }
  bool body_end(gr::Prob& p) { return gr::get_bit(r, p) != 0 || !ok(); }
  void put(uint32_t v, int n) {
    for (int i = 0; i < n; i++) out->push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  uint32_t byte(gr::ByteModel& m, uint8_t ctx) {
    const uint32_t v = gr::get_byte(r, m, ctx);
    put(v, 1);
    return v;
  }
  uint32_t num(gr::IntModel& m, int n, uint32_t pred) {
    const uint32_t v = (pred + static_cast<uint32_t>(gr::get_int(r, m))) & mask_of(n);
    put(v, n);
    return v;
  }
  uint32_t raw(int n) {
    const uint32_t v = gr::get_raw(r, 8 * n);
    put(v, n);
    return v;
  }
  int32_t fcoord(gr::IntModel& m, gr::Prob& p, int32_t pred) {
    if (gr::get_bit(r, p) == 0) {
      const auto k = static_cast<int32_t>(static_cast<uint32_t>(pred) +
                                          static_cast<uint32_t>(gr::get_int(r, m)));
      put(std::bit_cast<uint32_t>(static_cast<float>(k) * 0.125f), 4);
      return k;
    }
    const uint32_t v = gr::get_raw(r, 32);
    put(v, 4);
    return eighths(v);
  }
  uint32_t guess16(gr::Prob& p, uint32_t guess) {
    if (gr::get_bit(r, p) != 0) return kMissed;
    put(guess, 2);
    return guess;
  }
  size_t string(Face& f) {
    Models& m = *f.m;
    Texts& tx = f.texts;
    const int32_t pos = gr::get_int(r, m.tpl);
    if (pos < 0 || static_cast<size_t>(pos) > tx.mru.size()) {
      fault = true;
      return 0;
    }
    if (pos > 0) {
      tx.to_front(static_cast<size_t>(pos) - 1);
    } else {
      const int32_t count = gr::get_int(r, m.tpl_count);
      if (count < 0 || static_cast<size_t>(count) > kMostNumbers) {
        fault = true;
        return 0;
      }
      Template t;
      t.count = static_cast<size_t>(count);
      uint8_t prev = 0;
      for (size_t ends = 0; ends <= t.count && ok();) {
        const uint8_t c = gr::get_byte(r, m.text, prev);
        t.lits += static_cast<char>(c);
        prev = c;
        if (c == 0) ends++;
        if (t.lits.size() > kMostBody) fault = true;
      }
      if (!ok()) return 0;
      tx.add(std::move(t));
    }
    Template& t = tx.mru[0];
    std::vector<int32_t> nums(t.count);
    for (size_t j = 0; j < t.count; j++) {
      const int32_t pred = Texts::predict(t, j, j == 0 ? 0 : nums[0]);
      nums[j] = static_cast<int32_t>(
          static_cast<uint32_t>(pred) +
          static_cast<uint32_t>(gr::get_int(r, m.tnum[std::min<size_t>(j, 15)])));
    }
    const size_t from = out->size();
    size_t j = 0;
    for (const char c : t.lits) {
      if (c != 0) {
        out->push_back(static_cast<uint8_t>(c));
      } else if (j < nums.size()) {
        const std::string d = std::to_string(nums[j++]);
        out->insert(out->end(), d.begin(), d.end());
      }
    }
    const size_t len = out->size() - from;
    out->push_back(0);
    Texts::remember(t, nums);
    return len;
  }
  template <class Ctx>
  uint32_t pair16(gr::IntModel& nm, int32_t pred, gr::ByteModel& hm, Ctx ctx) {
    const int32_t n = pred + gr::get_int(r, nm);
    if (n < 0 || n > 511) {
      fault = true;
      return 0;
    }
    const uint32_t hi = gr::get_byte(r, hm, ctx(static_cast<uint32_t>(n)));
    if (hi > 127) fault = true;
    const uint32_t v = static_cast<uint32_t>(n) | (hi & 127u) << 9;
    put(v, 2);
    return v;
  }
};

// THE WALK.

template <class IO>
size_t text(IO& io, Face& f) {
  const double t0 = io.mark();
  const size_t n = io.string(f);
  io.part(DemoPart::text, t0);
  return n;
}

template <class IO>
void bytes(IO& io, gr::ByteModel& m, size_t n) {
  uint32_t prev = 0;
  for (size_t i = 0; i < n && io.ok(); i++) prev = io.byte(m, static_cast<uint8_t>(prev));
}

// A coordinate (a point's, or an origin when `origin`): eighths of a unit either way.
template <class IO>
int32_t coord(IO& io, Face& f, const Shape& sh, gr::IntModel& m, int32_t pred, bool origin) {
  if (origin ? sh.float_origins() : sh.float_coords()) return io.fcoord(m, f.m->float_raw, pred);
  return wrap(io.num(m, 2, static_cast<uint32_t>(pred)), 2);
}

template <class IO>
uint32_t angle(IO& io, const Shape& sh, gr::IntModel& m, uint32_t pred) {
  return io.num(m, sh.float_coords() ? 2 : 1, pred);
}

template <class IO>
void point(IO& io, Face& f, const Shape& sh, const std::array<int32_t, 3>& pred) {
  for (size_t i = 0; i < 3; i++) {
    const int32_t v = coord(io, f, sh, f.m->point[i], pred[i], false);
    if constexpr (IO::kLive) f.point[i] = v;
  }
}

// An entity delta's flags after its word, the number with its high bits.
template <class IO>
uint32_t delta_flags(IO& io, Face& f, const Shape& sh, uint32_t word, uint32_t& n) {
  Models& m = *f.m;
  const uint32_t n9 = word & 511u;
  uint32_t bits = word & ~511u;
  Ent& memo = f.ents[n9];
  if ((bits & kMoreBits) != 0) {
    bits |= io.byte(m.ent_more, memo.more);
    if constexpr (IO::kLive) memo.more = static_cast<uint8_t>(bits);
  }
  if (sh.fte != 0 && (bits & kEvenMore) != 0) bits |= io.byte(m.ent_byte, 0) << 16;
  if (sh.fte != 0 && (bits & kYetMore) != 0) bits |= io.byte(m.ent_byte, 1) << 24;
  if constexpr (IO::kLive) memo.hi = static_cast<uint8_t>(word >> 9);
  n = n9 + ((bits & kPlus512) != 0 ? 512u : 0u) + ((bits & kPlus1024) != 0 ? 1024u : 0u);
  return bits;
}

template <class IO>
void delta_fields(IO& io, Face& f, const Shape& sh, uint32_t bits, Ent& e) {
  Models& m = *f.m;
  double t0 = io.mark();
  if ((bits & kModel) != 0) {
    const uint32_t v = io.num(m.ent_model, (bits & kModelDbl) != 0 ? 2 : 1, e.model);
    if constexpr (IO::kLive) e.model = v;
  } else if ((bits & kModelDbl) != 0) {
    io.byte(m.ent_byte, 2);
  }
  if ((bits & kFrame) != 0) {
    const uint32_t v = io.num(m.ent_frame, 1, e.frame);
    if constexpr (IO::kLive) e.frame = v;
  }
  if ((bits & kColormap) != 0) {
    const uint32_t v = io.byte(m.ent_byte, e.colormap);
    if constexpr (IO::kLive) e.colormap = static_cast<uint8_t>(v);
  }
  if ((bits & kSkin) != 0) {
    const uint32_t v = io.byte(m.ent_skin, e.skin);
    if constexpr (IO::kLive) e.skin = static_cast<uint8_t>(v);
  }
  if ((bits & kEffects) != 0) {
    const uint32_t v = io.byte(m.ent_fx, e.effects);
    if constexpr (IO::kLive) e.effects = static_cast<uint8_t>(v);
  }
  constexpr std::array<uint32_t, 3> kOrigins = {kOrigin1, kOrigin2, kOrigin3};
  constexpr std::array<uint32_t, 3> kAngles = {kAngle1, kAngle2, kAngle3};
  io.part(DemoPart::entity_other, t0);
  for (size_t i = 0; i < 3; i++) {
    if ((bits & kOrigins[i]) != 0) {
      t0 = io.mark();
      const int32_t pred = linear(e.o[i], e.po[i]);
      const int32_t v = coord(io, f, sh, m.ent_o[i][e.oc[i]], pred, true);
      if constexpr (IO::kLive) {
        e.oc[i] =
            size_ctx(static_cast<int32_t>(static_cast<uint32_t>(v) - static_cast<uint32_t>(pred)));
        e.po[i] = e.o[i];
        e.o[i] = v;
      }
      io.part(DemoPart::entity_origin, t0);
    }
    if ((bits & kAngles[i]) != 0) {
      t0 = io.mark();
      const uint32_t v = angle(io, sh, m.ent_angle, e.a[i]);
      if constexpr (IO::kLive) e.a[i] = v;
      io.part(DemoPart::entity_angles, t0);
    }
  }
  if ((bits & kTrans) != 0) io.byte(m.ent_byte, 3);
  if ((bits & kColourMod) != 0) bytes(io, m.misc, 3);
}

template <class IO>
void packet_entities(IO& io, Face& f, const Shape& sh, bool delta) {
  Models& m = *f.m;
  if (delta) {  // the sequence: one on from the last list's
    const uint32_t seq = io.num(m.seq, 1, f.seq + 1);
    if constexpr (IO::kLive) f.seq = seq;
  }
  // Each word is first guessed as the last list's next one (the same entities change the same
  // fields frame after frame), and written in full only when the guess misses.
  int32_t prev = -1;
  size_t at = 0, run = 0;
  std::vector<ListWord> list;
  while (io.ok()) {
    const double t0 = io.mark();
    const uint32_t guess = at < f.list.size() ? f.list[at].word : 0;
    uint32_t word = io.guess16(m.word_same[std::min<size_t>(run, 3)], guess);
    if (word == guess) {
      run++;
    } else {
      run = 0;
      word = io.pair16(m.ent_n, prev + 1, m.ent_hi, [&](uint32_t n9) { return f.ents[n9].hi; });
    }
    if (word == 0) break;
    prev = static_cast<int32_t>(word & 511u);
    uint32_t n = 0;
    const uint32_t bits = delta_flags(io, f, sh, word, n);
    if constexpr (IO::kLive) list.push_back({word, n});
    while (at < f.list.size() && f.list[at].n <= n) at++;
    io.part(DemoPart::entity_word, t0);
    if ((bits & kRemove) != 0) continue;
    delta_fields(io, f, sh, bits, f.ents[n & 2047u]);
  }
  if constexpr (IO::kLive) f.list = std::move(list);
}

template <class IO>
void spawn(IO& io, Face& f, const Shape& sh) {
  Models& m = *f.m;
  for (uint8_t i = 0; i < 4; i++) io.byte(m.ent_byte, static_cast<uint8_t>(8 + i));
  for (size_t i = 0; i < 3; i++) {
    coord(io, f, sh, m.ent_o[i][0], f.point[i], true);
    angle(io, sh, m.ent_angle, 0);
  }
}

template <class IO>
void multiview_player(IO& io, Face& f, const Shape& sh) {
  Models& m = *f.m;
  double t0 = io.mark();
  const uint32_t slot = io.byte(m.slot, f.slot);
  Pl& p = f.pl[slot & 31u];
  const uint32_t lo = io.byte(m.pl_lo, p.lo);
  const uint32_t hi = io.byte(m.pl_hi, p.hi);
  const uint32_t bits = lo | hi << 8;
  io.part(DemoPart::player_head, t0);
  t0 = io.mark();
  const uint32_t frame = io.num(m.pl_frame, 1, p.frame);
  io.part(DemoPart::player_frame, t0);
  t0 = io.mark();
  if constexpr (IO::kLive) {
    f.slot = static_cast<uint8_t>(slot);
    p.lo = static_cast<uint8_t>(lo);
    p.hi = static_cast<uint8_t>(hi);
    p.frame = frame;
  }
  for (size_t i = 0; i < 3; i++) {
    if ((bits & (1u << i)) == 0) continue;
    const auto pred =
        static_cast<int32_t>(p.o[i] + ahead(int64_t{p.o[i]} - p.po[i], p.ot[i], p.pot[i], f.clock));
    const int32_t v = coord(io, f, sh, m.pl_o[i][p.oc[i]], pred, true);
    if constexpr (IO::kLive) {
      p.oc[i] =
          size_ctx(static_cast<int32_t>(static_cast<uint32_t>(v) - static_cast<uint32_t>(pred)));
      p.po[i] = p.o[i];
      p.o[i] = v;
      p.pot[i] = p.ot[i];
      p.ot[i] = f.clock;
    }
  }
  io.part(DemoPart::player_origin, t0);
  t0 = io.mark();
  for (size_t i = 0; i < 3; i++) {
    if ((bits & (8u << i)) == 0) continue;
    const auto pred =
        static_cast<uint32_t>((p.a[i] + static_cast<uint32_t>(ahead(wrap(p.a[i] - p.pa[i], 2),
                                                                    p.at[i], p.pat[i], f.clock))) &
                              0xFFFFu);
    const uint32_t v = io.num(m.pl_a[i][p.ac[i]], 2, pred);
    if constexpr (IO::kLive) {
      p.ac[i] = size_ctx(wrap(v - pred, 2));
      p.pa[i] = p.a[i];
      p.a[i] = v;
      p.pat[i] = p.at[i];
      p.at[i] = f.clock;
    }
  }
  io.part(DemoPart::player_angles, t0);
  t0 = io.mark();
  if ((bits & 2048u) != 0) {
    const uint32_t v = io.byte(m.pl_byte, p.model);
    if constexpr (IO::kLive) p.model = static_cast<uint8_t>(v);
  }
  if ((bits & 128u) != 0) {
    const uint32_t v = io.byte(m.pl_byte, static_cast<uint8_t>(p.skin ^ 0x55u));
    if constexpr (IO::kLive) p.skin = static_cast<uint8_t>(v);
  }
  if ((bits & 64u) != 0) {
    const uint32_t v = io.byte(m.ent_fx, p.effects);
    if constexpr (IO::kLive) p.effects = static_cast<uint8_t>(v);
  }
  if ((bits & 1024u) != 0) {
    const uint32_t v = io.num(m.pl_wframe, 1, p.wframe);
    if constexpr (IO::kLive) p.wframe = v;
  }
  io.part(DemoPart::player_other, t0);
}

// Where a sound's entity stands: a player's origin for 1..32, else the entity's.
std::array<int32_t, 3> origin_of(const Face& f, uint32_t ent) {
  if (ent >= 1 && ent <= 32) return f.pl[ent - 1].o;
  return f.ents[ent & 2047u].o;
}

template <class IO>
void sound(IO& io, Face& f, const Shape& sh) {
  Models& m = *f.m;
  const uint32_t lo = io.byte(m.sound, 0);
  const uint32_t hi = io.byte(m.sound, static_cast<uint8_t>(1 + (lo & 7u)));
  if ((hi & 0x80u) != 0) io.byte(m.sound, 10);
  if ((hi & 0x40u) != 0) io.byte(m.sound, 11);
  io.byte(m.sound, 12);
  point(io, f, sh, origin_of(f, ((lo | hi << 8) >> 3) & 1023u));
}

template <class IO>
void sound_extended(IO& io, Face& f, const Shape& sh) {
  Models& m = *f.m;
  const uint32_t flags = io.byte(m.sound, 20);
  if ((flags & 1u) != 0) io.byte(m.sound, 10);
  if ((flags & 2u) != 0) io.byte(m.sound, 11);
  uint32_t ent = 0;
  if ((flags & 8u) != 0) {
    ent = io.num(m.number, 2, 0);
    io.byte(m.sound, 21);
  } else {
    ent = io.num(m.number, 2, 0) >> 3;
  }
  if ((flags & 0x10u) != 0)
    io.num(m.number, 2, 0);
  else
    io.byte(m.sound, 12);
  point(io, f, sh, origin_of(f, ent));
}

template <class IO>
bool temp_entity(IO& io, Face& f, const Shape& sh) {
  Models& m = *f.m;
  const uint32_t code = io.byte(m.temp, 0);
  bool beam = false;
  switch (code) {
    case 2:
    case 12:
      io.byte(m.temp, 1);
      break;
    case 0:
    case 1:
    case 3:
    case 4:
    case 7:
    case 8:
    case 10:
    case 11:
    case 13:
      break;
    case 5:
    case 6:
    case 9:
      io.num(m.number, 2, 0);
      beam = true;
      break;
    default:
      return false;
  }
  point(io, f, sh, f.point);
  if (beam) point(io, f, sh, f.point);
  return true;
}

template <class IO>
void name_list(IO& io, Face& f, bool wide) {
  Models& m = *f.m;
  if (wide)
    io.num(m.number, 2, 0);
  else
    io.byte(m.misc, 45);
  while (io.ok() && text(io, f) != 0) {
  }
  io.byte(m.misc, 46);
}

template <class IO>
bool message_fields(IO& io, Face& f, Shape& sh, uint32_t id, uint32_t target);

// A message's fields, counted by part where the part is the message's.
template <class IO>
bool message(IO& io, Face& f, Shape& sh, uint32_t id, uint32_t target) {
  const double t0 = io.mark();
  const bool ok = message_fields(io, f, sh, id, target);
  if (id == 6 || id == 55) io.part(DemoPart::sound, t0);
  if (id == 3 || id == 38 || id == 14 || id == 36) io.part(DemoPart::stat, t0);
  return ok;
}

template <class IO>
bool message_fields(IO& io, Face& f, Shape& sh, uint32_t id, uint32_t target) {
  Models& m = *f.m;
  auto& stats = f.stats[target & 31u];
  switch (id) {
    case 1:
    case 2:
    case 27:
    case 28:
    case 33:
    case 34:
    case 35:
      return true;
    case 3:
    case 38: {  // a stat: the key, the value against the last one written for it
      const uint32_t k = io.byte(m.stat_key, f.stat_key);
      const uint32_t v = io.num(m.stat, id == 3 ? 1 : 4, stats[k & 255u]);
      if constexpr (IO::kLive) {
        f.stat_key = static_cast<uint8_t>(k);
        stats[k & 255u] = v;
      }
      return true;
    }
    case 6:
      sound(io, f, sh);
      return true;
    case 55:
      sound_extended(io, f, sh);
      return true;
    case 8:
      io.byte(m.misc, 8);
      text(io, f);
      return true;
    case 9:
    case 26:
    case 31:
      text(io, f);
      return true;
    case 10:
      io.byte(m.slot, 255);
      if ((sh.mvd & kMvdHighLagTeleport) != 0) io.byte(m.misc, 10);
      for (int i = 0; i < 3; i++) angle(io, sh, m.ent_angle, 0);
      return true;
    case 11: {  // the server data: the extension families, then the protocol and the rest
      sh.fte = sh.mvd = 0;
      uint32_t v = io.raw(4);
      for (int i = 0; i < 8 && io.ok(); i++, v = io.raw(4)) {
        if (v == kFamilyFte)
          sh.fte = io.raw(4);
        else if (v == kFamilyMvd)
          sh.mvd = io.raw(4);
        else if (v == kFamilyFte2)
          io.raw(4);
        else
          break;
      }
      io.raw(4);  // the spawn count
      text(io, f);
      io.raw(4);  // the server's time
      text(io, f);
      for (int i = 0; i < 10; i++) io.raw(4);
      return true;
    }
    case 12:
      io.byte(m.misc, 12);
      text(io, f);
      return true;
    case 14: {
      const uint32_t slot = io.byte(m.slot, 254);
      Pl& p = f.pl[slot & 31u];
      const uint32_t v = io.num(m.frags, 2, p.frags);
      if constexpr (IO::kLive) p.frags = v;
      return true;
    }
    case 16:
      io.num(m.number, 2, 0);
      return true;
    case 19:
      bytes(io, m.misc, 2);
      point(io, f, sh, f.point);
      return true;
    case 20:
      spawn(io, f, sh);
      return true;
    case 21:
    case 66: {
      Ent scratch;
      const uint32_t word = io.num(m.number, 2, 0);
      uint32_t n = 0;
      const uint32_t bits = delta_flags(io, f, sh, word, n);
      delta_fields(io, f, sh, bits, scratch);
      return true;
    }
    case 22:
      io.num(m.number, 2, 0);
      spawn(io, f, sh);
      return true;
    case 23:
      return temp_entity(io, f, sh);
    case 24:
    case 32:
    case 44:
      io.byte(m.misc, static_cast<uint8_t>(id));
      return true;
    case 29:
      point(io, f, sh, f.point);
      bytes(io, m.misc, 3);
      return true;
    case 30:
      point(io, f, sh, f.point);
      for (int i = 0; i < 3; i++) angle(io, sh, m.ent_angle, 0);
      return true;
    case 36: {
      const uint32_t slot = io.byte(m.slot, 253);
      Pl& p = f.pl[slot & 31u];
      const uint32_t v = io.num(m.ping, 2, p.ping);
      if constexpr (IO::kLive) p.ping = v;
      return true;
    }
    case 37:
      io.byte(m.slot, 252);
      io.raw(4);
      return true;
    case 39:
      io.num(m.number, 2, 0);
      return true;
    case 40:
      io.byte(m.slot, 251);
      io.raw(4);
      text(io, f);
      return true;
    case 41:
      if ((sh.fte & kFteChunkedDownloads) != 0) {
        if (static_cast<int32_t>(io.raw(4)) < 0) {
          io.raw(4);
          text(io, f);
        } else {
          bytes(io, m.raw, 1024);
        }
      } else {
        const int32_t size = wrap(io.num(m.number, 2, 0), 2);
        io.byte(m.misc, 41);
        if (size > 0) bytes(io, m.raw, static_cast<size_t>(size));
      }
      return true;
    case 42:
      multiview_player(io, f, sh);
      return true;
    case 43:
      bytes(io, m.misc, size_t{io.byte(m.misc, 43)} * 6);
      return true;
    case 54:
      bytes(io, m.misc, size_t{io.byte(m.misc, 54)} * 7);
      return true;
    case 45:
    case 46:
      name_list(io, f, false);
      return true;
    case 56:
    case 60:
      name_list(io, f, true);
      return true;
    case 47:
    case 48:
      packet_entities(io, f, sh, id == 48);
      return true;
    case 49:
    case 50:
      io.raw(4);
      return true;
    case 51:
      io.byte(m.slot, 250);
      text(io, f);
      text(io, f);
      return true;
    case 52:
      text(io, f);
      text(io, f);
      return true;
    case 53:
      io.byte(m.slot, 249);
      io.byte(m.misc, 53);
      return true;
    case 84:
      bytes(io, m.misc, 3);
      bytes(io, m.raw, io.num(m.number, 2, 0));
      return true;
    case 78:
      io.byte(m.stat_key, 0);
      text(io, f);
      return true;
    case 79:
      io.byte(m.stat_key, 0);
      io.raw(4);
      return true;
    default:
      return false;
  }
}

template <class IO>
bool walk_body(IO& io, Face& f, uint32_t type) {
  Shape& sh = io.shape(f);
  const uint32_t target = type >> 3;
  Models& m = *f.m;
  uint32_t prev = 0;
  // The first message under the record's kind and player (what a record holds is mostly its
  // kind's), the others under the message before.
  for (bool first = true;; first = false) {
    double from = io.mark();
    const bool end = io.body_end(first ? m.end_first[type & 255u] : m.end[prev]);
    io.part(DemoPart::message_id, from);
    if (end) break;
    from = io.mark();
    const uint32_t id = first ? io.byte(m.first_id, static_cast<uint8_t>(type))
                              : io.byte(m.id, static_cast<uint8_t>(prev));
    io.part(DemoPart::message_id, from);
    if (!message(io, f, sh, id, target) || !io.ok()) return false;
    io.spend(id, from);
    prev = id;
  }
  return io.ok();
}

// A record: its kind, player and where its body is (demo_multiview.cpp's walk). False when the
// bytes end inside it, its kind is not one, or its length is not one.
struct Record {
  uint32_t ms = 0, type = 0, mask = 0;
  size_t body = 0, size = 0, next = 0;
  int kind() const { return static_cast<int>(type & 7u); }
};

uint32_t u32_at(std::span<const uint8_t> b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}

bool record_at(std::span<const uint8_t> b, size_t at, Record& r) {
  if (at + 2 > b.size()) return false;
  r.ms = b[at];
  r.type = b[at + 1];
  at += 2;
  if (r.kind() == kSet) {
    if (at + 8 > b.size()) return false;
    r.body = at;
    r.size = 8;
    r.next = at + 8;
    return true;
  }
  if (r.kind() == 0 || r.kind() > kAll) return false;
  if (r.kind() == kMultiple) {
    if (at + 4 > b.size()) return false;
    r.mask = u32_at(b, at);
    at += 4;
  }
  if (at + 4 > b.size()) return false;
  const uint32_t len = u32_at(b, at);
  if (len > kMostBody || at + 4 + len > b.size()) return false;
  r.body = at + 4;
  r.size = len;
  r.next = r.body + len;
  return true;
}

// One block of records packed: the records from `at` while the block's second lasts.
void pack_block(std::span<const uint8_t> b, Face& f, bool keyframe, size_t& at, int64_t& time,
                std::vector<uint8_t>& chunk, DemoPackReport& rep) {
  Pack io;
  io.s.b = b;
  io.rep = &rep;
  gr::rc_begin(io.w, chunk);
  if (keyframe) {
    f.restart();
    gr::put_raw(io.w, f.shape.fte, 32);
    gr::put_raw(io.w, f.shape.mvd, 32);
  }
  Models& m = *f.m;
  const int64_t start = time;
  bool first = true;
  while (at < b.size() && (first || time - start < kBlockMs)) {
    first = false;
    gr::put_bit(io.w, m.more, 1);
    Record r;
    if (!record_at(b, at, r)) {  // the bytes after the last whole record
      gr::put_bit(io.w, m.tail, 1);
      rep.tail = b.size() - at;
      gr::put_int(io.w, m.len, static_cast<int32_t>(rep.tail));
      io.s.at = at;
      io.s.end = b.size();
      bytes(io, m.raw, rep.tail);
      at = b.size();
      break;
    }
    gr::put_bit(io.w, m.tail, 0);
    rep.records++;
    io.s.at = at;
    io.s.end = r.next;
    const double head = io.mark();
    const uint8_t prev_type = f.type;
    // The kind, then the time under the last record's time.
    f.type = static_cast<uint8_t>(r.type);
    // Guessed as the kind that came after the last one last time; written when the guess misses.
    const bool hit = f.type == f.follow[prev_type];
    gr::put_bit(io.w, m.type_hit[prev_type], hit ? 0 : 1);
    if (!hit) gr::put_byte(io.w, m.type, prev_type, f.type);
    f.follow[prev_type] = f.type;
    gr::put_byte(io.w, m.ms, f.ms, static_cast<uint8_t>(r.ms));
    f.ms = static_cast<uint8_t>(r.ms);
    io.s.at += 2;
    f.clock += f.ms;
    io.part(DemoPart::record_head, head);
    time += r.ms;
    if (r.kind() == kSet) {
      f.set[0] = io.num(m.set0, 4, f.set[0]);
      f.set[1] = io.num(m.set1, 4, f.set[1]);
      rep.walked++;
      at = r.next;
      continue;
    }
    if (r.kind() == kMultiple) {
      const bool same = r.mask == f.mask;
      gr::put_bit(io.w, m.same_mask, same ? 0 : 1);
      if (same)
        io.s.at += 4;
      else
        f.mask = io.raw(4);
    }
    io.s.at += 4;  // the length: the unpacker counts the body it writes
    Probe probe{{b, r.body, r.next}, f.shape};
    const bool whole = walk_body(probe, f, r.type) && probe.s.at == r.next;
    gr::put_bit(io.w, m.walked[static_cast<size_t>(r.kind())], whole ? 0 : 1);
    if (whole) {
      walk_body(io, f, r.type);
      rep.walked++;
    } else {
      const double t0 = io.mark();
      gr::put_int(io.w, m.len, static_cast<int32_t>(r.size));
      bytes(io, m.raw, r.size);
      io.part(DemoPart::kept, t0);
      rep.kept++;
    }
    at = r.next;
  }
  gr::put_bit(io.w, m.more, 0);
  gr::rc_end(io.w);
}

const char* const kPartNames[] = {"records' heads and framing",
                                  "kept as bytes",
                                  "player: slot and bits",
                                  "player: frame",
                                  "player: origin",
                                  "player: angles",
                                  "player: model, skin, effects",
                                  "entity: number and flags",
                                  "entity: origin",
                                  "entity: angles",
                                  "entity: model, frame, skin",
                                  "text",
                                  "message ids and ends",
                                  "sounds",
                                  "stats, frags, pings"};

bool unpack_block(std::span<const uint8_t> chunk, Face& f, bool keyframe, std::vector<uint8_t>& out,
                  std::string& error) {
  Unpack io;
  io.out = &out;
  io.body = out.size();
  gr::rc_begin(io.r, chunk.data(), chunk.size());
  if (keyframe) {
    f.restart();
    f.shape.fte = gr::get_raw(io.r, 32);
    f.shape.mvd = gr::get_raw(io.r, 32);
  }
  Models& m = *f.m;
  while (gr::get_bit(io.r, m.more) != 0) {
    if (io.r.bad) break;
    io.body = out.size();
    if (gr::get_bit(io.r, m.tail) != 0) {
      const int32_t n = gr::get_int(io.r, m.len);
      if (n < 0 || static_cast<uint32_t>(n) > kMostBody) {
        error = "a block's tail is " + std::to_string(n) + " bytes; the file is damaged";
        return false;
      }
      bytes(io, m.raw, static_cast<size_t>(n));
      break;
    }
    const uint8_t prev_type = f.type;
    f.type = gr::get_bit(io.r, m.type_hit[prev_type]) == 0 ? f.follow[prev_type]
                                                           : gr::get_byte(io.r, m.type, prev_type);
    f.follow[prev_type] = f.type;
    f.ms = gr::get_byte(io.r, m.ms, f.ms);
    io.put(f.ms, 1);
    io.put(f.type, 1);
    f.clock += f.ms;
    const int kind = f.type & 7;
    if (kind == kSet) {
      f.set[0] = io.num(m.set0, 4, f.set[0]);
      f.set[1] = io.num(m.set1, 4, f.set[1]);
      continue;
    }
    if (kind == 0 || kind > kAll) {
      error = "a record of kind " + std::to_string(kind) + "; the file is damaged";
      return false;
    }
    if (kind == kMultiple) {
      if (gr::get_bit(io.r, m.same_mask) == 0)
        io.put(f.mask, 4);
      else
        f.mask = io.raw(4);
    }
    const size_t len_at = out.size();
    io.put(0, 4);
    io.body = out.size();
    if (gr::get_bit(io.r, m.walked[static_cast<size_t>(kind)]) == 0) {
      if (!walk_body(io, f, f.type)) {
        error =
            "a record at byte " + std::to_string(len_at) + " does not decode; the file is damaged";
        return false;
      }
    } else {
      const int32_t n = gr::get_int(io.r, m.len);
      if (n < 0 || static_cast<uint32_t>(n) > kMostBody) {
        error = "a record's body is " + std::to_string(n) + " bytes; the file is damaged";
        return false;
      }
      bytes(io, m.raw, static_cast<size_t>(n));
    }
    const auto len = static_cast<uint32_t>(out.size() - io.body);
    for (size_t i = 0; i < 4; i++) out[len_at + i] = static_cast<uint8_t>(len >> (8 * i));
  }
  if (io.r.bad || io.fault) {
    error = "a block's field-coded bytes end early; the file is damaged";
    return false;
  }
  return true;
}

}  // namespace

const char* demo_part_name(DemoPart part) {
  const auto i = static_cast<size_t>(part);
  return i < std::size(kPartNames) ? kPartNames[i] : "?";
}

void demo_hub_meta(std::span<const uint8_t> recording, gr::Meta& header, gr::Meta& final_meta) {
  Demo d;
  std::string error;
  if (!open_demo(std::vector<uint8_t>(recording.begin(), recording.end()), d, error)) return;
  struct Seen {
    std::string name, team;
    int32_t frags = 0;
  };
  std::vector<Seen> seen;  // every player, in the order first seen
  std::string map;
  const auto look = [&] {
    const DemoState& s = d.state;
    if (map.empty()) map = s.map;
    for (size_t slot = 0; slot < s.names.size(); slot++) {
      if (s.names[slot].empty() || s.spectator[slot]) continue;
      auto it = std::find_if(seen.begin(), seen.end(),
                             [&](const Seen& p) { return p.name == s.names[slot]; });
      if (it == seen.end()) it = seen.insert(seen.end(), {s.names[slot], s.teams[slot], 0});
      it->team = s.teams[slot];
      it->frags = s.frags[slot];
    }
  };
  look();
  while (next_frame(d, error) == DemoRead::frame) look();
  const DemoState& s = d.state;
  const auto info = [&](const char* key) {
    for (const auto& [k, v] : s.serverinfo)
      if (k == key) return plain_text(v);  // a page shows them: the font's bytes as ASCII
    return std::string();
  };
  for (Seen& p : seen) {
    p.name = plain_text(p.name);
    p.team = plain_text(p.team);
  }
  const auto on = [&](const char* key) { return std::atoi(info(key).c_str()) != 0; };
  const size_t players = seen.size();
  std::string mode;
  if (s.format == DemoFormat::single)
    mode = "single";
  else if (on("coop"))
    mode = "coop";
  else if (on("teamplay"))
    mode = players <= 4 ? "2on2" : "4on4";
  else
    mode = players <= 2 ? "1on1" : "ffa";
  if (!map.empty()) header.emplace_back("map", map);
  header.emplace_back("mode", mode);
  if (!info("hostname").empty()) header.emplace_back("server", info("hostname"));
  std::vector<std::pair<std::string, int64_t>> teams;  // in the order first seen
  for (const Seen& p : seen) {
    header.emplace_back("player", p.team.empty() ? p.name : p.name + "\t" + p.team);
    if (p.team.empty()) continue;
    auto t =
        std::find_if(teams.begin(), teams.end(), [&](const auto& x) { return x.first == p.team; });
    if (t == teams.end()) t = teams.insert(teams.end(), {p.team, 0});
    t->second += p.frags;
  }
  for (const auto& t : teams) header.emplace_back("team", t.first);
  final_meta.emplace_back("duration_ms", std::to_string(std::llround(s.time * 1000.0)));
  if (s.format == DemoFormat::single) return;
  for (const Seen& p : seen) {
    std::string v = p.name;
    v += '\t';
    v += std::to_string(p.frags);
    final_meta.emplace_back("score", std::move(v));
  }
  for (const auto& [team, frags] : teams) {
    std::string v = team;
    v += '\t';
    v += std::to_string(frags);
    final_meta.emplace_back("team_score", std::move(v));
  }
}

bool pack_demo(std::span<const uint8_t> recording, std::vector<uint8_t>& out, std::string& error,
               const DemoPackOptions& options, DemoPackReport* report) {
  DemoPackReport rep;
  gr::StreamHeader h;
  h.game = kGame;
  const bool multiview = is_multiview(recording);
  const bool sequel = !multiview && is_sequel_recording(recording);
  rep.format = multiview ? "multiview" : sequel ? "sequel" : "bytes";
  h.meta = {{"format", rep.format},
            {"bytes", std::to_string(recording.size())},
            {"keyframe_seconds", std::to_string(options.keyframe_seconds)}};
  gr::Meta final_meta;
  if (!sequel) demo_hub_meta(recording, h.meta, final_meta);  // the sequel's: its own face's
  h.meta.insert(h.meta.end(), options.meta.begin(), options.meta.end());
  if (multiview)
    h.tracks = {{kMultiviewTrack, gr::TrackKind::messages, gr::TrackCodec::field, "multiview"}};
  else if (sequel)
    h.tracks = {{kSequelTrack, gr::TrackKind::messages, gr::TrackCodec::field, "sequel"}};
  else
    h.tracks = {{kBytesTrack, gr::TrackKind::messages, gr::TrackCodec::zstd, "recording"}};
  out.clear();
  gr::StreamWriter w;
  gr::stream_begin(w, h, out);
  const int every = std::max(1, options.keyframe_seconds);
  if (multiview) {
    Face f;
    size_t at = 0;
    int64_t time = 0;
    for (int64_t block = 0; at < recording.size(); block++) {
      const bool keyframe = block % every == 0;
      const int64_t start = time;
      std::vector<uint8_t> chunk;
      pack_block(recording, f, keyframe, at, time, chunk, rep);
      gr::stream_add(w, kMultiviewTrack, start, chunk);
      gr::stream_end_block(w, keyframe);
      rep.blocks++;
      if (keyframe) rep.keyframes++;
    }
    // The records' own clock is the duration (the reader's may stop at a disconnect).
    std::erase_if(final_meta, [](const auto& kv) { return kv.first == "duration_ms"; });
    final_meta.insert(final_meta.begin(), {"duration_ms", std::to_string(time)});
    final_meta.emplace_back("records", std::to_string(rep.records));
    gr::stream_finish(w, final_meta);
  } else if (sequel) {
    // The broadcast's writer (sequel_tv.hpp), fed the finished recording: the same bytes.
    SequelBroadcast b;
    sequel_broadcast_begin(b, every, recording.size());
    for (const std::span<const uint8_t> block : sequel_blocks(recording))
      sequel_broadcast_block(b, block);
    sequel_broadcast_end(b);
    out = std::move(b.out);
    rep.records = b.report.blocks;
    rep.walked = b.report.walked;
    rep.kept = b.report.kept;
    rep.blocks = b.report.chunks;
    rep.keyframes = b.report.keyframes;
    error.clear();
    if (report != nullptr) *report = rep;
    return true;
  } else {
    for (size_t at = 0; at < recording.size(); at += kBytesBlock) {
      const size_t n = std::min(kBytesBlock, recording.size() - at);
      gr::stream_add(w, kBytesTrack, static_cast<int64_t>(rep.blocks), recording.subspan(at, n));
      gr::stream_end_block(w, true);
      rep.blocks++;
      rep.keyframes++;
    }
    gr::stream_finish(w, final_meta);
  }
  error.clear();
  if (report != nullptr) *report = rep;
  return true;
}

struct DemoUnpacker::State {
  gr::StreamReader r;
  Face face;
  SequelCoder sequel;
  bool opened = false, started = false, sequel_seen = false;
};

DemoUnpacker::DemoUnpacker() : state(std::make_unique<State>()) {}
DemoUnpacker::~DemoUnpacker() = default;
DemoUnpacker::DemoUnpacker(DemoUnpacker&&) noexcept = default;
DemoUnpacker& DemoUnpacker::operator=(DemoUnpacker&&) noexcept = default;

bool unpack_more(DemoUnpacker& u, std::span<const uint8_t> file, std::vector<uint8_t>& out,
                 std::string& error) {
  DemoUnpacker::State& st = *u.state;
  error.clear();
  if (!st.opened) {
    if (!gr::read_stream_header(file, st.r, error)) return false;
    if (st.r.header.game != kGame) {
      error = "a replay file of the game \"" + st.r.header.game + "\", not this game's";
      return false;
    }
    st.opened = true;
  }
  st.r.bytes = file;  // the same file, grown: the reader goes on from its last whole block
  gr::Block b;
  for (;;) {
    const size_t at = st.r.at;
    if (!gr::next_block(st.r, b, error)) {
      st.r.at = at;  // a block not whole yet is read again when more has come
      break;
    }
    u.blocks++;
    for (const gr::Chunk& c : b.chunks) {
      if (c.track == kSequelTrack) {
        if (!st.started && !b.keyframe) continue;
        st.started = st.sequel_seen = true;
        if (!unpack_sequel_chunk(st.sequel, c.bytes, b.keyframe, out, error)) {
          error += " (the block at " + std::to_string(b.time_ms) + " ms)";
          return false;
        }
      } else if (c.track == kBytesTrack) {
        out.insert(out.end(), c.bytes.begin(), c.bytes.end());
      } else if (c.track == kMultiviewTrack) {
        if (!st.started && !b.keyframe) continue;  // a stream joined late starts at a keyframe
        st.started = true;
        if (!unpack_block(c.bytes, st.face, b.keyframe, out, error)) {
          error += " (the block at " + std::to_string(b.time_ms) + " ms)";
          return false;
        }
      }
    }
  }
  return true;
}

bool unpack_demo(std::span<const uint8_t> file, std::vector<uint8_t>& out, std::string& error) {
  out.clear();
  DemoUnpacker u;
  if (!unpack_more(u, file, out, error)) return false;
  // A sequel recording ends with a length of -1; a cut file stops at its last whole block.
  if (u.state->sequel_seen && error.empty()) out.insert(out.end(), 4, 0xFF);
  return true;  // a cut file: what its whole blocks held, `error` naming the cut
}

}  // namespace shooters_edge::client
