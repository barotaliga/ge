// THE INFLATER the engine reads compressed files with: a zlib stream (RFC 1950: the two header
// bytes, the deflate blocks of RFC 1951 — stored, fixed Huffman and dynamic Huffman — and the
// Adler-32 checked) back to its bytes. A model's PNG texture is read with it at load
// (assets/image.hpp), a zip archive's entries (vfs/zip.hpp), and a lab that bakes a tree's data off
// a public dataset (a deflated GeoTIFF tile of an elevation model), so neither needs a compression
// dependency. A zip archive's entries are raw deflate streams with no zlib wrapper (`inflate_raw`),
// checked with the CRC-32 beside it (`crc32`, also PNG's chunk check). Load time only: nothing on a
// frame's path or in the simulation.
//
// ge:summary The inflater: a zlib stream's deflate blocks back to bytes
// ge:keywords inflate deflate zlib huffman decompress adler32 crc32 geotiff raw zip
// ge:skill determinism assets labs
// ge:test tests/lab_raster_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ge::core {

// The CRC-32 of ISO 3309 (zip's entries, PNG's chunks), continuing from `crc`.
uint32_t crc32(const uint8_t* bytes, size_t n, uint32_t crc = 0);

// The bytes a zlib stream holds, or nothing when it is malformed or its checksum fails.
std::optional<std::vector<uint8_t>> inflate_zlib(std::span<const uint8_t> stream);
// The bytes a raw deflate stream holds (no header, no checksum: the container carries those),
// or nothing when it is malformed or does not come to exactly `expected_size` bytes.
std::optional<std::vector<uint8_t>> inflate_raw(std::span<const uint8_t> stream,
                                                size_t expected_size);

}  // namespace ge::core
