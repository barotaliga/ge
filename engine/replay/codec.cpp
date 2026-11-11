// The range coder and its models (codec.hpp): the coder's carry and its normalisation, and the
// integer and byte trees.
//
// ge:summary The field codec's coder: carry, normalisation, the integer and byte bit trees
// ge:keywords field-codec range-coder carry normalize bit-tree int byte
// ge:skill replay
// ge:test tests/replay_codec_test.cpp
#include "replay/codec.hpp"

#include <array>

namespace ge::replay {

namespace {

constexpr uint32_t kTop = 1u << 24;
constexpr int kProbBits = 11;
constexpr uint32_t kProbMask = (1u << kProbBits) - 1u;
// How far a probability moves toward the bit it saw, as a shift, by how many bits it has seen
// (its top 5 bits count them, up to 31): a new one learns fast, a seasoned one settles.
constexpr std::array<uint8_t, 32> kShift = {1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5,
                                            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

void adapt(Prob& p, int bit) {
  const uint32_t seen = p >> kProbBits;
  uint32_t q = p & kProbMask;
  const int shift = kShift[seen];
  if (bit == 0)
    q += ((1u << kProbBits) - q) >> shift;
  else
    q -= q >> shift;
  q = q < 1 ? 1 : q > kProbMask ? kProbMask : q;
  p = static_cast<Prob>((seen < 31 ? seen + 1 : 31) << kProbBits | q);
}

void shift_low(RangeWriter& w) {
  if (static_cast<uint32_t>(w.low) < 0xFF000000u || (w.low >> 32) != 0) {
    const auto carry = static_cast<uint8_t>(w.low >> 32);
    uint8_t temp = w.cache;
    do {
      w.out->push_back(static_cast<uint8_t>(temp + carry));
      temp = 0xFF;
    } while (--w.cache_size != 0);
    w.cache = static_cast<uint8_t>(w.low >> 24);
  }
  w.cache_size++;
  w.low = (w.low & 0x00FFFFFFu) << 8;
}

uint8_t next_byte(RangeReader& r) {
  if (r.at >= r.end) {
    r.bad = true;
    return 0;
  }
  return *r.at++;
}

void put_tree(RangeWriter& w, Prob* tree, int levels, uint32_t v) {
  uint32_t node = 1;
  for (int i = levels - 1; i >= 0; i--) {
    const int bit = static_cast<int>((v >> i) & 1u);
    put_bit(w, tree[node], bit);
    node = (node << 1) | static_cast<uint32_t>(bit);
  }
}

uint32_t get_tree(RangeReader& r, Prob* tree, int levels) {
  uint32_t node = 1;
  for (int i = 0; i < levels; i++)
    node = (node << 1) | static_cast<uint32_t>(get_bit(r, tree[node]));
  return node - (1u << levels);
}

}  // namespace

void rc_begin(RangeWriter& w, std::vector<uint8_t>& out) {
  w = RangeWriter{};
  w.out = &out;
}

void rc_end(RangeWriter& w) {
  for (int i = 0; i < 5; i++) shift_low(w);
}

void rc_begin(RangeReader& r, const uint8_t* data, size_t size) {
  r = RangeReader{};
  r.at = data;
  r.end = data + size;
  for (int i = 0; i < 5; i++) r.code = (r.code << 8) | next_byte(r);
}

void put_bit(RangeWriter& w, Prob& p, int bit) {
  const uint32_t bound = (w.range >> kProbBits) * (p & kProbMask);
  if (bit == 0) {
    w.range = bound;
  } else {
    w.low += bound;
    w.range -= bound;
  }
  adapt(p, bit);
  while (w.range < kTop) {
    w.range <<= 8;
    shift_low(w);
  }
}

int get_bit(RangeReader& r, Prob& p) {
  const uint32_t bound = (r.range >> kProbBits) * (p & kProbMask);
  int bit = 0;
  if (r.code < bound) {
    r.range = bound;
  } else {
    r.code -= bound;
    r.range -= bound;
    bit = 1;
  }
  adapt(p, bit);
  while (r.range < kTop) {
    r.range <<= 8;
    r.code = (r.code << 8) | next_byte(r);
  }
  return bit;
}

void put_raw(RangeWriter& w, uint32_t value, int count) {
  for (int i = count - 1; i >= 0; i--) {
    w.range >>= 1;
    if (((value >> i) & 1u) != 0) w.low += w.range;
    while (w.range < kTop) {
      w.range <<= 8;
      shift_low(w);
    }
  }
}

uint32_t get_raw(RangeReader& r, int count) {
  uint32_t v = 0;
  for (int i = 0; i < count; i++) {
    r.range >>= 1;
    uint32_t bit = 0;
    if (r.code >= r.range) {
      r.code -= r.range;
      bit = 1;
    }
    v = (v << 1) | bit;
    while (r.range < kTop) {
      r.range <<= 8;
      r.code = (r.code << 8) | next_byte(r);
    }
  }
  return v;
}

IntModel::IntModel() {
  length.fill(kProbHalf);
  sign.fill(kProbHalf);
  under.fill(kProbHalf);
}

int int_length(int32_t v) {
  uint32_t a = v < 0 ? 0u - static_cast<uint32_t>(v) : static_cast<uint32_t>(v);
  int n = 0;
  while (a != 0) {
    n++;
    a >>= 1;
  }
  return n;
}

void put_int(RangeWriter& w, IntModel& m, int32_t v) {
  const int n = int_length(v);
  put_tree(w, m.length.data(), 6, static_cast<uint32_t>(n));
  if (n == 0) return;
  put_bit(w, m.sign[static_cast<size_t>(n)], v < 0 ? 1 : 0);
  const uint32_t a = v < 0 ? 0u - static_cast<uint32_t>(v) : static_cast<uint32_t>(v);
  const int rest = n - 1;  // the bits under the top bit
  const int modelled = rest < 3 ? rest : 3;
  if (modelled > 0) {
    const uint32_t top = (a >> (rest - modelled)) & ((1u << modelled) - 1u);
    put_tree(w, &m.under[static_cast<size_t>(n) * 8], modelled, top);
  }
  if (rest > modelled) put_raw(w, a & ((1u << (rest - modelled)) - 1u), rest - modelled);
}

int32_t get_int(RangeReader& r, IntModel& m) {
  const int n = static_cast<int>(get_tree(r, m.length.data(), 6));
  if (n == 0) return 0;
  if (n > 32) {
    r.bad = true;
    return 0;
  }
  const int negative = get_bit(r, m.sign[static_cast<size_t>(n)]);
  const int rest = n - 1;
  const int modelled = rest < 3 ? rest : 3;
  uint32_t a = 1;
  if (modelled > 0)
    a = (a << modelled) | get_tree(r, &m.under[static_cast<size_t>(n) * 8], modelled);
  if (rest > modelled) a = (a << (rest - modelled)) | get_raw(r, rest - modelled);
  return negative != 0 ? static_cast<int32_t>(0u - a) : static_cast<int32_t>(a);
}

ByteModel::ByteModel() : tree(size_t{256} * 256, kProbHalf) {}

void put_byte(RangeWriter& w, ByteModel& m, uint8_t context, uint8_t b) {
  put_tree(w, &m.tree[size_t{context} * 256], 8, b);
}

uint8_t get_byte(RangeReader& r, ByteModel& m, uint8_t context) {
  return static_cast<uint8_t>(get_tree(r, &m.tree[size_t{context} * 256], 8));
}

}  // namespace ge::replay
