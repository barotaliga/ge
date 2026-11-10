// The map's checksums and MD4 (RFC 1320): three rounds of 16 steps over 64-byte blocks, the
// message padded with 0x80, zeros, and its length in bits as a little-endian 64-bit number.
//
// ge:summary MD4 and the per-lump map checksums, defined
// ge:keywords md4 map-checksum lump-digest
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_mapsum_test.cpp
#include "shooters_edge/net/classic_mapsum.hpp"

#include <cstring>

namespace shooters_edge::net {
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

MapChecksums map_checksums(std::span<const uint8_t> bsp) {
  MapChecksums out;
  constexpr size_t kLumps = 15;
  if (bsp.size() < 4 + kLumps * 8) return out;
  for (size_t i = 1; i < kLumps; i++) {
    const uint32_t ofs = le32(bsp.data() + 4 + 8 * i);
    const uint32_t len = le32(bsp.data() + 8 + 8 * i);
    if (ofs > bsp.size() || len > bsp.size() - ofs) return out;
    const uint32_t w = md4_word(bsp.subspan(ofs, len));
    out.all ^= w;
    if (i != 4 && i != 5 && i != 10) out.played ^= w;
  }
  out.ok = true;
  return out;
}

struct ReleasedMap {
  const char* path;
  uint32_t rebuilt;  // the rebuilt file's own second checksum
  uint32_t first;    // the first release's, which the wire carries
};
// Measured: each rebuilt file served by a live server, its reported second checksum read back.
constexpr ReleasedMap kReleasedMaps[] = {
    {"maps/dm2.bsp", 2066969664u, 1710634548u},
    {"maps/dm3.bsp", static_cast<uint32_t>(-1859681874), 367136248u},
    {"maps/dm4.bsp", static_cast<uint32_t>(-1015750775), static_cast<uint32_t>(-1670388545)},
    {"maps/dm5.bsp", 2009758949u, static_cast<uint32_t>(-1339209475)},
    {"maps/dm6.bsp", 537693021u, 1376311851u},
};

uint32_t sent_checksum(std::string_view path, const MapChecksums& sums) {
  for (const ReleasedMap& m : kReleasedMaps)
    if (path == m.path && sums.played == m.rebuilt) return m.first;
  return sums.played;
}

}  // namespace shooters_edge::net
