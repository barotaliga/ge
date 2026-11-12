// SHA-1 of a byte string (sha1.hpp).
//
// ge:summary SHA-1's compression rounds over padded 64-byte blocks
// ge:keywords sha1 digest
// ge:skill net
// ge:test tests/net_sha1_test.cpp
#include "net/sha1.hpp"

#include <vector>

namespace ge::net {

namespace {

uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

}  // namespace

std::array<uint8_t, 20> sha1(std::span<const uint8_t> bytes) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::vector<uint8_t> m(bytes.begin(), bytes.end());
  const uint64_t bits = static_cast<uint64_t>(bytes.size()) * 8;
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  for (int i = 7; i >= 0; i--) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
  for (size_t at = 0; at < m.size(); at += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      const size_t k = at + static_cast<size_t>(i) * 4;
      w[i] = static_cast<uint32_t>(m[k]) << 24 | static_cast<uint32_t>(m[k + 1]) << 16 |
             static_cast<uint32_t>(m[k + 2]) << 8 | m[k + 3];
    }
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      uint32_t f = 0, k = 0;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rol(b, 30);
      b = a;
      a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }
  std::array<uint8_t, 20> out{};
  for (size_t i = 0; i < 5; i++)
    for (size_t j = 0; j < 4; j++) out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - 8 * j));
  return out;
}

std::string sha1_hex(std::string_view text) {
  const auto d =
      sha1(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
  static const char* k = "0123456789ABCDEF";
  std::string out;
  for (uint8_t b : d) {
    out += k[b >> 4];
    out += k[b & 15];
  }
  return out;
}

}  // namespace ge::net
