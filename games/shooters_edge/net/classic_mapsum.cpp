// The map's checksums: each lump's MD4 word (server/md4.hpp), XORed.
//
// ge:summary The per-lump map checksums, defined
// ge:keywords md4 map-checksum lump-digest
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_mapsum_test.cpp
#include "shooters_edge/net/classic_mapsum.hpp"

#include <cstring>

namespace shooters_edge::net {
namespace {

uint32_t le32(const uint8_t* p) {
  return uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24;
}

}  // namespace

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
