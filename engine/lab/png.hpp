// THE PNG WRITER the labs save pictures with: RGBA8, deflate in STORED blocks (no compression,
// no dependency), the CRC-32 and the Adler-32 the format needs. A lab picture is a few
// megabytes at most, and a compression dependency is not worth the saving. Any viewer, and an
// agent's image reader, opens it. The PACKED writers (grey for committed files, RGB for a
// player's screenshot) filter each row and deflate it (deflate.hpp).
//
// THE READER brings a lab's own pictures back for a diff (diff.hpp): assets/image.hpp's PNG
// reader (8-bit RGBA, RGB, grey, grey with alpha and palettes, any row filter), so it also reads
// a PNG another tool compressed.
//
// ge:summary The labs' PNG writer and reader: RGBA8, stored deflate, CRC-32 and Adler-32
// ge:keywords png encode decode read image file crc32 adler32 deflate screenshot grey
// ge:keywords encode_png_grey encode_png_rgb write_file packed-png rgb
// ge:skill labs
// ge:test tests/lab_raster_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "lab/raster.hpp"

namespace ge::lab {

using core::crc32;  // the one CRC-32, core/inflate.hpp's

std::vector<uint8_t> encode_png(const Picture& picture);

// Write `picture` to `path`; false (and nothing half-written left behind) if it could not.
bool write_png(const std::string& path, const Picture& picture);

// A COMMITTED picture's PNG: 8-bit grey, each row under the filter that packs it best, deflated
// (deflate.hpp), the same bytes on every platform. For files the tree keeps (an atlas, a
// texture layer), where the stored writer above would be megabytes.
std::vector<uint8_t> encode_png_grey(int width, int height, std::span<const uint8_t> grey);
// A PACKED colour PNG (a screenshot a player keeps, capture/screenshot.hpp): `rgba`'s rows top to
// bottom written as 8-bit RGB (the alpha dropped: a frame is opaque), filtered and deflated as
// the grey one is.
std::vector<uint8_t> encode_png_rgb(int width, int height, std::span<const uint8_t> rgba);
// Write `bytes` to `path` as they are; false if it could not.
bool write_file(const std::string& path, std::span<const uint8_t> bytes);

// The picture a PNG holds, as RGBA8; nothing when the bytes are not a PNG the reader takes
// (16-bit or interlaced images) or a checksum fails.
std::optional<Picture> decode_png(std::span<const uint8_t> bytes);
std::optional<Picture> read_png(const std::string& path);

}  // namespace ge::lab
