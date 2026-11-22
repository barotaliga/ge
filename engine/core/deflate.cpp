// The definitions behind deflate.hpp: hash chains over the input, one fixed-code block.
//
// ge:summary The deflater: matches by hash chains, written with deflate's fixed codes
// ge:keywords deflate lz77 hash-chain fixed-huffman length-code distance-code
// ge:skill determinism net
// ge:test tests/core_deflate_test.cpp
#include "core/deflate.hpp"

#include "core/inflate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace ge::core {

namespace {

constexpr int kWindow = 32768;
constexpr int kMinMatch = 3, kMaxMatch = 258;
constexpr int kTries = 32;  // places with the same hash tried per position
constexpr int kHashBits = 13;

// The bits of the stream, least significant first.
struct Out {
  std::vector<uint8_t> bytes;
  uint32_t acc = 0;
  int have = 0;

  void put(uint32_t v, int n) {
    acc |= v << have;
    have += n;
    while (have >= 8) {
      bytes.push_back(static_cast<uint8_t>(acc & 0xFF));
      acc >>= 8;
      have -= 8;
    }
  }
  // A Huffman code of `n` bits, its most significant bit first.
  void code(uint32_t c, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) r |= ((c >> i) & 1u) << (n - 1 - i);
    put(r, n);
  }
  void flush() {
    if (have > 0) bytes.push_back(static_cast<uint8_t>(acc & 0xFF));
    acc = 0;
    have = 0;
  }
};

// A literal or length symbol (0..287) in the fixed code.
void symbol(Out& o, int s) {
  if (s < 144)
    o.code(0x30u + static_cast<uint32_t>(s), 8);
  else if (s < 256)
    o.code(0x190u + static_cast<uint32_t>(s - 144), 9);
  else if (s < 280)
    o.code(static_cast<uint32_t>(s - 256), 7);
  else
    o.code(0xC0u + static_cast<uint32_t>(s - 280), 8);
}

constexpr std::array<uint16_t, 29> kLengthBase = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                                  15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                                  67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> kLengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> kDistBase = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> kDistExtra = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                                4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                                9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void match(Out& o, int length, int distance) {
  int l = 28;
  while (kLengthBase[size_t(l)] > length) l--;
  symbol(o, 257 + l);
  if (kLengthExtra[size_t(l)] > 0)
    o.put(static_cast<uint32_t>(length - kLengthBase[size_t(l)]), kLengthExtra[size_t(l)]);
  int d = 29;
  while (kDistBase[size_t(d)] > distance) d--;
  o.code(static_cast<uint32_t>(d), 5);
  if (kDistExtra[size_t(d)] > 0)
    o.put(static_cast<uint32_t>(distance - kDistBase[size_t(d)]), kDistExtra[size_t(d)]);
}

uint32_t hash3(const uint8_t* p) {
  const uint32_t v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
  return (v * 2654435761u) >> (32 - kHashBits);
}

// One block's codes for `in` into `o` (`final` its first bit).
void block(Out& o, std::span<const uint8_t> in, bool final) {
  o.put(final ? 1u : 0u, 1);
  o.put(1, 2);  // fixed codes
  const int n = static_cast<int>(in.size());
  std::vector<int32_t> head(size_t(1) << kHashBits, -1), prev(in.size(), -1);
  const auto insert = [&](int at) {
    if (at + kMinMatch > n) return;
    const uint32_t h = hash3(in.data() + at);
    prev[size_t(at)] = head[h];
    head[h] = at;
  };
  int at = 0;
  while (at < n) {
    int best = 0, best_at = 0;
    if (at + kMinMatch <= n) {
      int cand = head[hash3(in.data() + at)];
      const int most = std::min(kMaxMatch, n - at);
      for (int tries = 0; cand >= 0 && at - cand <= kWindow && tries < kTries; tries++) {
        int l = 0;
        while (l < most && in[size_t(cand) + size_t(l)] == in[size_t(at) + size_t(l)]) l++;
        if (l > best) {
          best = l;
          best_at = cand;
          if (l == most) break;
        }
        cand = prev[size_t(cand)];
      }
    }
    if (best >= kMinMatch) {
      match(o, best, at - best_at);
      for (int k = 0; k < best; k++) insert(at + k);
      at += best;
    } else {
      symbol(o, in[size_t(at)]);
      insert(at);
      at++;
    }
  }
  symbol(o, 256);
}

void start(DeflateStream& s, std::vector<uint8_t>& out) {
  if (s.started) return;
  s.started = true;
  if (s.wrap == DeflateWrap::zlib) {
    out.push_back(0x78);
    out.push_back(0x01);
  } else if (s.wrap == DeflateWrap::gzip) {
    const uint8_t header[10] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0xFF};  // no time, OS unknown
    out.insert(out.end(), header, header + 10);
  }
}

}  // namespace

std::vector<uint8_t> deflate_raw(std::span<const uint8_t> in) {
  Out o;
  o.bytes.reserve(in.size() / 2 + 16);
  block(o, in, true);
  o.flush();
  return o.bytes;
}

uint32_t adler32(const uint8_t* bytes, size_t n, uint32_t a) {
  uint32_t lo = a & 0xFFFFu, hi = a >> 16;
  for (size_t i = 0; i < n; i++) {
    lo = (lo + bytes[i]) % 65521u;
    hi = (hi + lo) % 65521u;
  }
  return hi << 16 | lo;
}

void deflate_sync(DeflateStream& s, std::span<const uint8_t> bytes, std::vector<uint8_t>& out) {
  if (s.finished) return;
  start(s, out);
  Out o;
  if (!bytes.empty()) block(o, bytes, false);
  // The sync flush: an empty stored block (not final), its lengths after the byte boundary.
  o.put(0, 3);
  o.flush();
  out.insert(out.end(), o.bytes.begin(), o.bytes.end());
  const uint8_t lengths[4] = {0x00, 0x00, 0xFF, 0xFF};
  out.insert(out.end(), lengths, lengths + 4);
  s.adler = adler32(bytes.data(), bytes.size(), s.adler);
  s.crc = crc32(bytes.data(), bytes.size(), s.crc);
  s.length += bytes.size();
}

void deflate_finish(DeflateStream& s, std::vector<uint8_t>& out) {
  if (s.finished) return;
  start(s, out);
  s.finished = true;
  Out o;
  o.put(1, 1);
  o.put(1, 2);
  symbol(o, 256);
  o.flush();
  out.insert(out.end(), o.bytes.begin(), o.bytes.end());
  if (s.wrap == DeflateWrap::zlib) {
    for (int k = 3; k >= 0; k--) out.push_back(static_cast<uint8_t>((s.adler >> (8 * k)) & 0xFF));
  } else if (s.wrap == DeflateWrap::gzip) {
    for (int k = 0; k < 4; k++) out.push_back(static_cast<uint8_t>((s.crc >> (8 * k)) & 0xFF));
    const auto size = static_cast<uint32_t>(s.length & 0xFFFFFFFFu);
    for (int k = 0; k < 4; k++) out.push_back(static_cast<uint8_t>((size >> (8 * k)) & 0xFF));
  }
}

}  // namespace ge::core
