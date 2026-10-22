// THE INFLATER the labs read compressed files with: a zlib stream (RFC 1950: the two header
// bytes, the deflate blocks of RFC 1951 — stored, fixed Huffman and dynamic Huffman — and the
// Adler-32 checked) back to its bytes. A lab that bakes a tree's data off a public dataset reads
// it with this (a deflated GeoTIFF tile of an elevation model), so a bake needs no compression
// dependency. Not for the simulation or a shipped game.
//
// ge:summary The labs' inflater: a zlib stream's deflate blocks back to bytes
// ge:keywords inflate deflate zlib huffman decompress adler32 geotiff
// ge:skill labs
// ge:test tests/lab_raster_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ge::lab {

// The bytes a zlib stream holds, or nothing when it is malformed or its checksum fails.
std::optional<std::vector<uint8_t>> inflate_zlib(std::span<const uint8_t> stream);

}  // namespace ge::lab
