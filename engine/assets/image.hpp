// A MODEL'S IMAGE: a texture as RGBA8, straight alpha, its colours as stored (sRGB for a base
// colour), row-major from the top, the layout of a lab picture (`lab::Picture`) and of the 2D
// layer's image (`ui::Image`), so the bytes move between them unchanged.
//
// THE PNG READER takes every PNG a model ships with: grey, grey with alpha, RGB and RGBA at 8
// bits, and a PALETTE at 1, 2, 4 or 8 bits with its transparency (`tRNS`); any of the five row
// filters, through the inflater (core/inflate.hpp). Not 16-bit or interlaced images: nothing
// comes back, and the caller says what it skipped. It runs at load, never on a frame's path.
//
// `texel` reads the image's nearest texel at a glTF texture coordinate (u to the right, v down
// from the top, repeating), the way a NEAREST sampler does; `srgb_to_linear` is the colour
// transfer a base colour texture is stored in.
//
// ge:summary A model's RGBA8 image, the PNG reader (palette included) and a nearest texel
// ge:keywords assets image texture png decode palette plte trns crc32 texel nearest srgb
// ge:keywords srgb_to_linear base-colour-texture colormap atlas
// ge:skill assets
// ge:test tests/assets_texture_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "core/inflate.hpp"

namespace ge::assets {

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;  // four bytes a pixel, from the top row
};

// The CRC-32 PNG's chunks are checked with: core's (core/inflate.hpp).
using core::crc32;

// The image a PNG holds, or nothing when the bytes are not a PNG this reader takes (16-bit or
// interlaced) or a checksum fails.
std::optional<Image> decode_png(std::span<const uint8_t> bytes);

// The texel nearest the texture coordinate (u, v), repeating outside 0..1: four bytes.
const uint8_t* texel(const Image& image, float u, float v);

// An sRGB-encoded channel (0..255) as linear light (0..1).
float srgb_to_linear(uint8_t c);

}  // namespace ge::assets
