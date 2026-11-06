// Tests: the map's checksums (games/shooters_edge/net/classic_mapsum.hpp): MD4 against the RFC 1320
// vectors (empty, one byte, a block and a half, the 80-digit line), one lump's word, a built BSP's
// first and second checksums held golden with the entity lump left out of both and the visibility,
// node and leaf lumps left out of the second, a lump past the file's end refused, and the released
// maps' wire value given only for the rebuilt file's own checksum.
//
// ge:summary Tests: MD4 vectors, the map's two checksums per lump, the released maps' wire value
// ge:keywords test classic-protocol map-checksum md4 checksum2 sent_checksum released-maps
// ge:skill net shooter testing
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/net/classic_mapsum.hpp"

namespace cn = shooters_edge::net;

namespace {

std::string md4_hex(const std::string& text) {
  uint8_t d[16];
  cn::md4_digest({reinterpret_cast<const uint8_t*>(text.data()), text.size()}, d);
  std::string out;
  char b[3];
  for (uint8_t v : d) {
    std::snprintf(b, sizeof b, "%02x", v);
    out += b;
  }
  return out;
}

void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
  for (int i = 0; i < 4; i++) v[at + i] = static_cast<uint8_t>(x >> (8 * i));
}

// A BSP of 15 lumps, lump i holding 10 + 7 i bytes of (i * 31 + k) & 255.
std::vector<uint8_t> built_bsp() {
  std::vector<uint8_t> v(4 + 15 * 8, 0);
  put32(v, 0, 29);
  for (uint32_t i = 0; i < 15; i++) {
    const auto ofs = static_cast<uint32_t>(v.size());
    const uint32_t len = 10 + 7 * i;
    for (uint32_t k = 0; k < len; k++) v.push_back(static_cast<uint8_t>((i * 31 + k) & 255));
    put32(v, 4 + 8 * i, ofs);
    put32(v, 8 + 8 * i, len);
  }
  return v;
}

uint32_t lump_of(const std::vector<uint8_t>& v, int i) {
  return static_cast<uint32_t>(v[4 + 8 * static_cast<size_t>(i)]) |
         static_cast<uint32_t>(v[5 + 8 * static_cast<size_t>(i)]) << 8;
}

}  // namespace

TEST(md4_holds_the_rfc_vectors) {
  CHECK(md4_hex("") == "31d6cfe0d16ae931b73c59d7e0c089c0");
  CHECK(md4_hex("a") == "bde52cb31de33e46245e05fbdbd6fb24");
  CHECK(md4_hex("abc") == "a448017aaf21d8525fc10ae87aa6729d");
  CHECK(md4_hex("message digest") == "d9130a8164549fe818874806e1c7014b");
  CHECK(md4_hex("abcdefghijklmnopqrstuvwxyz") == "d79e1c308aa5bbcdeea8ed63df412da9");
  CHECK(md4_hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") ==
        "043f8582f241db351ce627e153e7f0e4");
  CHECK(md4_hex("1234567890123456789012345678901234567890123456789012345678901234567890123456"
                "7890") == "e33b4ddc9c38f2199c3e7b164fcc0536");
}

TEST(a_lumps_word_is_its_digests_words_xored) {
  // MD4("abc") = a448017a af21d852 5fc10ae8 7aa6729d, each read little-endian.
  const uint32_t want = 0x7a0148a4u ^ 0x52d821afu ^ 0xe80ac15fu ^ 0x9d72a67au;
  CHECK(cn::md4_word({reinterpret_cast<const uint8_t*>("abc"), 3}) == want);
}

TEST(a_built_maps_checksums_are_held_and_skip_their_lumps) {
  std::vector<uint8_t> bsp = built_bsp();
  const cn::MapChecksums base = cn::map_checksums(bsp);
  CHECK(base.ok);
  uint32_t all = 0, played = 0;
  for (int i = 1; i < 15; i++) {
    const uint32_t ofs = lump_of(bsp, i);
    const uint32_t w = cn::md4_word({bsp.data() + ofs, static_cast<size_t>(10 + 7 * i)});
    all ^= w;
    if (i != 4 && i != 5 && i != 10) played ^= w;
  }
  CHECK(base.all == all);
  CHECK(base.played == played);
  CHECK(base.all == 0xe7e0b37bu);     // golden: the built map
  CHECK(base.played == 0x61706320u);  // golden: the built map
  for (int i = 0; i < 15; i++) {
    std::vector<uint8_t> v = bsp;
    v[lump_of(v, i)] ^= 1;
    const cn::MapChecksums s = cn::map_checksums(v);
    const bool in_all = i != 0, in_played = i != 0 && i != 4 && i != 5 && i != 10;
    CHECK_MSG((s.all != base.all) == in_all, "lump " + std::to_string(i) + " in the first");
    CHECK_MSG((s.played != base.played) == in_played,
              "lump " + std::to_string(i) + " in the second");
  }
}

TEST(a_lump_past_the_end_is_refused) {
  std::vector<uint8_t> bsp = built_bsp();
  put32(bsp, 8 + 8 * 3, 100000);
  CHECK(!cn::map_checksums(bsp).ok);
  CHECK(!cn::map_checksums(std::vector<uint8_t>(20, 0)).ok);
}

TEST(a_released_map_sends_the_first_releases_checksum) {
  cn::MapChecksums rebuilt;
  rebuilt.played = static_cast<uint32_t>(-1015750775);
  rebuilt.ok = true;
  CHECK(cn::sent_checksum("maps/dm4.bsp", rebuilt) == static_cast<uint32_t>(-1670388545));
  CHECK(cn::sent_checksum("maps/dm5.bsp", rebuilt) == rebuilt.played);  // another map's value
  cn::MapChecksums other = rebuilt;
  other.played ^= 1;  // the same name, other bytes: the map's own
  CHECK(cn::sent_checksum("maps/dm4.bsp", other) == other.played);
}
