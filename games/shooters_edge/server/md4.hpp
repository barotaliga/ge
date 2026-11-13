// MD4 (RFC 1320): the digest the classic game's map checks and the sequel's map checksum are
// made of. Three rounds of 16 steps over 64-byte blocks, the message padded with 0x80, zeros, and
// its length in bits as a little-endian 64-bit number.
//
// ge:summary MD4 and its four words XORed, for the games' map checksums
// ge:keywords md4 md4_digest md4_word map-checksum rfc1320
// ge:skill shooter
// ge:test tests/shooters_edge_classic_mapsum_test.cpp
#pragma once

#include <cstdint>
#include <span>

namespace shooters_edge::server {

// The MD4 digest of `bytes` (16 bytes, RFC 1320).
void md4_digest(std::span<const uint8_t> bytes, uint8_t out[16]);
// The digest's four little-endian words XORed.
uint32_t md4_word(std::span<const uint8_t> bytes);

}  // namespace shooters_edge::server
