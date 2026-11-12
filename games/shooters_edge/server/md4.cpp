// MD4 (md4.hpp).
//
// ge:summary MD4 (RFC 1320), defined
// ge:keywords md4 rfc1320
// ge:skill shooter
// ge:test tests/shooters_edge_classic_mapsum_test.cpp
#include "shooters_edge/server/md4.hpp"

#include <cstddef>
#include <cstring>

namespace shooters_edge::server {
namespace {

uint32_t rol(uint32_t x, int s) { return (x << s) | (x >> (32 - s)); }
uint32_t le32(const uint8_t* p) {
  return uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24;
}

void block(uint32_t h[4], const uint8_t* p) {
  uint32_t x[16];
  for (size_t i = 0; i < 16; i++) x[i] = le32(p + 4 * i);
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
  const auto f = [](uint32_t u, uint32_t v, uint32_t w) { return (u & v) | (~u & w); };
  const auto g = [](uint32_t u, uint32_t v, uint32_t w) { return (u & v) | (u & w) | (v & w); };
  const auto k = [](uint32_t u, uint32_t v, uint32_t w) { return u ^ v ^ w; };
  static const int s1[4] = {3, 7, 11, 19}, s2[4] = {3, 5, 9, 13}, s3[4] = {3, 9, 11, 15};
  for (int i = 0; i < 16; i++) {
    const uint32_t t = rol(a + f(b, c, d) + x[i], s1[i % 4]);
    a = d, d = c, c = b, b = t;
  }
  for (int i = 0; i < 16; i++) {
    const int j = (i % 4) * 4 + i / 4;
    const uint32_t t = rol(a + g(b, c, d) + x[j] + 0x5a827999u, s2[i % 4]);
    a = d, d = c, c = b, b = t;
  }
  static const int order3[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
  for (int i = 0; i < 16; i++) {
    const uint32_t t = rol(a + k(b, c, d) + x[order3[i]] + 0x6ed9eba1u, s3[i % 4]);
    a = d, d = c, c = b, b = t;
  }
  h[0] += a, h[1] += b, h[2] += c, h[3] += d;
}

}  // namespace

void md4_digest(std::span<const uint8_t> bytes, uint8_t out[16]) {
  uint32_t h[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
  size_t at = 0;
  for (; at + 64 <= bytes.size(); at += 64) block(h, bytes.data() + at);
  uint8_t tail[128] = {};
  const size_t rest = bytes.size() - at;
  if (rest > 0) std::memcpy(tail, bytes.data() + at, rest);
  tail[rest] = 0x80;
  const size_t n = rest < 56 ? 64 : 128;
  const uint64_t bits = static_cast<uint64_t>(bytes.size()) * 8;
  for (int i = 0; i < 8; i++) tail[n - 8 + i] = static_cast<uint8_t>(bits >> (8 * i));
  block(h, tail);
  if (n == 128) block(h, tail + 64);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) out[4 * i + j] = static_cast<uint8_t>(h[i] >> (8 * j));
}

uint32_t md4_word(std::span<const uint8_t> bytes) {
  uint8_t d[16];
  md4_digest(bytes, d);
  return le32(d) ^ le32(d + 4) ^ le32(d + 8) ^ le32(d + 12);
}

}  // namespace shooters_edge::server
