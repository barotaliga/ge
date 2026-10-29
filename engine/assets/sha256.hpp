// SHA-256 (FIPS 180-4): the digest a file is recognised by when its identity matters, such as a
// content archive a tool will only read when it is a known, unchanged release. A whole buffer at
// once or streamed in pieces; the answer as 32 bytes or as lower-case hex.
//
// ge:summary SHA-256 of a buffer or a stream of pieces, as bytes or hex
// ge:keywords sha256 sha-256 hash digest checksum identity verify file
// ge:skill assets
// ge:test tests/assets_sha256_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace ge::assets {

struct Sha256 {
  std::array<uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::array<uint8_t, 64> block{};
  size_t fill = 0;
  uint64_t total = 0;

  void update(std::span<const uint8_t> bytes);
  std::array<uint8_t, 32> finish();
};

std::array<uint8_t, 32> sha256(std::span<const uint8_t> bytes);
std::string to_hex(std::span<const uint8_t> bytes);
// The digest of `bytes` as 64 lower-case hex digits.
std::string sha256_hex(std::span<const uint8_t> bytes);

}  // namespace ge::assets
