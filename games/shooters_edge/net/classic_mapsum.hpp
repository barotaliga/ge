// THE MAP'S CHECKSUMS: what a classic client sends in its first `prespawn` and a server compares
// (`sv_mapcheck`) to know both hold the same map. A BSP file's header is a 4-byte version (29, or
// the magic "BSP2") and 15 lumps of (offset, length); each lump but the entities (0) is hashed with
// MD4 (RFC 1320) and its digest's four little-endian 32-bit words XORed into one word. The first
// checksum XORs every such word; the second leaves out the visibility (4), node (5) and leaf (10)
// lumps, which a recompile changes without changing the map's play, and is the one sent.
//
// THE RELEASED MAPS: five deathmatch maps were later released again rebuilt from their sources, so
// their bytes differ from the first release's. A server holding the rebuilt file reports (and
// expects) the first release's second checksum for it, so players of either file meet:
// `sent_checksum` answers that value when the map's name and its own second checksum are the
// rebuilt file's (`kReleasedMaps`, measured from a live server), else the map's own. Held by
// the RFC's MD4 vectors and a live server's answer for a known map (the `se-interop` lab), with a
// fixture BSP's values golden in tests/shooters_edge_classic_mapsum_test.cpp.
//
// ge:summary The map's first and second checksums (MD4 per lump, XORed) and MD4 itself
// ge:keywords map-checksum checksum2 mapcheck sv_mapcheck prespawn md4 lump-digest bsp-header
// ge:keywords map_checksums md4_digest md4_word sent_checksum released-maps rebuilt-maps
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_mapsum_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "shooters_edge/server/md4.hpp"

namespace shooters_edge::net {

// MD4 and one lump's word (server/md4.hpp).
using server::md4_digest;
using server::md4_word;

struct MapChecksums {
  uint32_t all = 0;     // every lump but the entities
  uint32_t played = 0;  // the second: without visibility, nodes and leaves (sent in prespawn)
  bool ok = false;      // false: not a BSP the header reads (a lump outside the file)
};
MapChecksums map_checksums(std::span<const uint8_t> bsp);

// The second checksum as the wire carries it for the map at `path` ("maps/dm4.bsp"): the first
// release's for a rebuilt released map, else `sums.played`.
uint32_t sent_checksum(std::string_view path, const MapChecksums& sums);

}  // namespace shooters_edge::net
