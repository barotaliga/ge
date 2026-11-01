// A TGA PICTURE as RGBA8 (assets/image.hpp's `Image`, straight alpha, from the top row): the
// forms a game's replacement textures and skins are saved in.
//
// - The image types: 2 (true colour), 3 (grey), 1 (colour-mapped), and their run-length packed
//   forms 10, 11 and 9. True colour at 24 bits (opaque) or 32 (its alpha kept), and 16 (5-5-5
//   with the top bit as a one-bit alpha); grey at 8 bits; colour-mapped at 8 bits a pixel over a
//   map of 24 or 32-bit entries.
// - The header is 18 bytes, then the id field (skipped), then the colour map, then the pixels in
//   blue, green, red, alpha order. The descriptor's bit 5 says the rows run from the top; else
//   from the bottom (the common case), and the image is turned the right way up. A pixel run may
//   cross the end of a row.
// - Anything else (another type, a depth the type does not take, a colour map index past the
//   map, pixels or runs past the end of the file) gives nothing back, with the reason in `why`.
//
// `encode_tga` writes type 2 or 10 at 32 bits from the top, for the tests and for any tool that
// writes a texture. Both run at load, never on a frame.
//
// ge:summary The TGA reader (true colour, grey, colour-mapped, packed or not) and writer
// ge:keywords assets tga targa decode_tga encode_tga rle run-length alpha rgba image texture skin
// ge:keywords replacement-texture
// ge:skill assets
// ge:test tests/assets_tga_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/image.hpp"

namespace ge::assets {

// The picture a TGA holds, or nothing (the reason in `*why` when given).
std::optional<Image> decode_tga(std::span<const uint8_t> bytes, std::string* why = nullptr);

// The image as a 32-bit TGA from the top row, run-length packed when `packed`.
std::vector<uint8_t> encode_tga(const Image& image, bool packed = false);

}  // namespace ge::assets
