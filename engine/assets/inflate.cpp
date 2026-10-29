// The definitions behind inflate.hpp: a canonical-Huffman decoder read a bit at a time.
//
// ge:summary The inflater: deflate's stored, fixed and dynamic blocks decoded
// ge:keywords inflate deflate huffman canonical-code length-distance
// ge:skill assets labs
// ge:test tests/lab_raster_test.cpp
#include "assets/inflate.hpp"

#include <array>

namespace ge::assets {

namespace {

// The bits of a deflate stream, least significant first.
struct Bits {
  std::span<const uint8_t> in;
  size_t at = 0;
  uint32_t acc = 0;
  int have = 0;
  bool bad = false;

  uint32_t take(int n) {
    while (have < n) {
      if (at >= in.size()) {
        bad = true;
        return 0;
      }
      acc |= uint32_t(in[at++]) << have;
      have += 8;
    }
    const uint32_t v = acc & ((1u << n) - 1);
    acc >>= n;
    have -= n;
    return v;
  }
  void align() {
    acc = 0;
    have = 0;
  }
};

// A canonical Huffman code: how many codes of each length, and the symbols in code order.
struct Code {
  std::array<uint16_t, 16> count{};
  std::vector<uint16_t> symbol;
};

bool build(Code& code, std::span<const uint8_t> lengths) {
  code.count.fill(0);
  for (const uint8_t l : lengths) code.count[l]++;
  code.count[0] = 0;
  std::array<uint16_t, 16> offs{};
  int left = 1;
  for (size_t l = 1; l < 16; l++) {
    left = left * 2 - code.count[l];
    if (left < 0) return false;
    offs[l] = static_cast<uint16_t>(offs[l - 1] + code.count[l - 1]);
  }
  code.symbol.assign(lengths.size(), 0);
  for (size_t s = 0; s < lengths.size(); s++) {
    if (lengths[s] != 0) code.symbol[offs[lengths[s]]++] = static_cast<uint16_t>(s);
  }
  return true;
}

int decode(Bits& bits, const Code& code) {
  int first = 0;
  int index = 0;
  int value = 0;
  for (size_t l = 1; l < 16; l++) {
    value |= int(bits.take(1));
    if (bits.bad) return -1;
    const int n = code.count[l];
    if (value - n < first) return code.symbol[size_t(index + value - first)];
    index += n;
    first = (first + n) << 1;
    value <<= 1;
  }
  return -1;
}

constexpr std::array<uint16_t, 29> kLenBase{3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                            15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                            67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> kLenExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                            2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> kDistBase{
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> kDistExtra{0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                             6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(Bits& bits, std::vector<uint8_t>& out, const Code& lit, const Code& dist) {
  for (;;) {
    const int s = decode(bits, lit);
    if (s < 0) return false;
    if (s < 256) {
      out.push_back(static_cast<uint8_t>(s));
      continue;
    }
    if (s == 256) return true;
    const size_t li = size_t(s - 257);
    if (li >= kLenBase.size()) return false;
    const size_t len = kLenBase[li] + bits.take(kLenExtra[li]);
    const int d = decode(bits, dist);
    if (d < 0 || size_t(d) >= kDistBase.size()) return false;
    const size_t back = kDistBase[size_t(d)] + bits.take(kDistExtra[size_t(d)]);
    if (bits.bad || back > out.size()) return false;
    const size_t from = out.size() - back;
    for (size_t k = 0; k < len; k++) out.push_back(out[from + k]);
  }
}

bool dynamic(Bits& bits, Code& lit, Code& dist) {
  const size_t nlen = bits.take(5) + 257;
  const size_t ndist = bits.take(5) + 1;
  const size_t ncode = bits.take(4) + 4;
  constexpr std::array<uint8_t, 19> kOrder{16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                           11, 4,  12, 3, 13, 2, 14, 1, 15};
  std::array<uint8_t, 19> cl{};
  for (size_t i = 0; i < ncode; i++) cl[kOrder[i]] = static_cast<uint8_t>(bits.take(3));
  Code clc;
  if (bits.bad || !build(clc, cl)) return false;
  std::vector<uint8_t> lengths;
  lengths.reserve(nlen + ndist);
  while (lengths.size() < nlen + ndist) {
    const int s = decode(bits, clc);
    if (s < 0) return false;
    if (s < 16) {
      lengths.push_back(static_cast<uint8_t>(s));
      continue;
    }
    uint8_t v = 0;
    size_t rep = 0;
    if (s == 16) {
      if (lengths.empty()) return false;
      v = lengths.back();
      rep = 3 + bits.take(2);
    } else if (s == 17) {
      rep = 3 + bits.take(3);
    } else {
      rep = 11 + bits.take(7);
    }
    if (lengths.size() + rep > nlen + ndist) return false;
    lengths.insert(lengths.end(), rep, v);
  }
  return !bits.bad && build(lit, std::span(lengths).first(nlen)) &&
         build(dist, std::span(lengths).subspan(nlen));
}

// The deflate blocks in `bits` appended to `out`, to the last block; false when malformed.
bool inflate_blocks(Bits& bits, std::vector<uint8_t>& out) {
  Code fixed_lit;
  Code fixed_dist;
  {
    std::array<uint8_t, 288> l{};
    for (size_t i = 0; i < 288; i++) l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    std::array<uint8_t, 30> d{};
    d.fill(5);
    build(fixed_lit, l);
    build(fixed_dist, d);
  }
  for (bool last = false; !last;) {
    last = bits.take(1) != 0;
    const uint32_t type = bits.take(2);
    if (bits.bad) return false;
    if (type == 0) {
      bits.align();
      if (bits.at + 4 > bits.in.size()) return false;
      const size_t len = bits.in[bits.at] | (size_t(bits.in[bits.at + 1]) << 8);
      const size_t nlen = bits.in[bits.at + 2] | (size_t(bits.in[bits.at + 3]) << 8);
      bits.at += 4;
      if ((len ^ 0xffff) != nlen || bits.at + len > bits.in.size()) return false;
      out.insert(out.end(), bits.in.begin() + long(bits.at), bits.in.begin() + long(bits.at + len));
      bits.at += len;
    } else if (type == 1) {
      if (!codes(bits, out, fixed_lit, fixed_dist)) return false;
    } else if (type == 2) {
      Code lit;
      Code dist;
      if (!dynamic(bits, lit, dist) || !codes(bits, out, lit, dist)) return false;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<std::vector<uint8_t>> inflate_zlib(std::span<const uint8_t> stream) {
  if (stream.size() < 6 || (stream[0] & 0x0f) != 8 || ((stream[0] << 8) | stream[1]) % 31 != 0 ||
      (stream[1] & 0x20) != 0) {
    return std::nullopt;
  }
  Bits bits{stream.subspan(2, stream.size() - 6)};
  std::vector<uint8_t> out;
  if (!inflate_blocks(bits, out)) return std::nullopt;
  uint32_t a = 1;
  uint32_t b = 0;
  for (const uint8_t v : out) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  const size_t t = stream.size() - 4;
  const uint32_t want = (uint32_t(stream[t]) << 24) | (uint32_t(stream[t + 1]) << 16) |
                        (uint32_t(stream[t + 2]) << 8) | uint32_t(stream[t + 3]);
  if (((b << 16) | a) != want) return std::nullopt;
  return out;
}

std::optional<std::vector<uint8_t>> inflate_raw(std::span<const uint8_t> stream,
                                                size_t expected_size) {
  Bits bits{stream};
  std::vector<uint8_t> out;
  out.reserve(expected_size);
  if (!inflate_blocks(bits, out) || out.size() != expected_size) return std::nullopt;
  return out;
}

}  // namespace ge::assets
