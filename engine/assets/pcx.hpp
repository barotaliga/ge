// A PALETTED IMAGE IN PCX: the 8-bit, one-plane, run-length-encoded form, the one a classic
// player skin file is (`skins/<name>.pcx`: a skin in the game's palette, recoloured by the
// player's top and bottom colours like the model's own skin). The reader keeps the INDICES,
// because a skin's colour rows are what a player's colours replace, and the file's own palette
// (the 768 bytes after the 0x0C marker at the end), which a game may ignore for its own.
//
// - The header is 128 bytes: the maker byte 0x0A, the version, encoding 1 (runs), 8 bits a pixel,
//   the window (xmin, ymin, xmax, ymax, inclusive), one plane, the bytes a line (at least the
//   width, even). Anything else is refused with what was found and where.
// - The runs: a byte with its top two bits set is a count (its low six bits) of the next byte;
//   any other byte is itself. A run may cross the end of a line (writers differ), so the lines
//   are decoded as one stream, each `bytes_per_line` long with its padding dropped.
// - A file whose pixels run out early is refused; one whose palette is missing reads with the
//   palette empty (`palette.empty()`), since a skin needs only its indices.
//
// `encode_pcx` writes the same form (runs of up to 63, a single byte at 0xC0 or above as a run of
// one), for the tests and for any tool that writes a skin. It runs at load, never on a frame.
//
// ge:summary The 8-bit PCX reader and writer: indices kept, the palette, runs across lines
// ge:keywords assets pcx paletted indexed image run-length rle skin player-skin palette decode
// ge:keywords encode decode_pcx encode_pcx
// ge:skill assets
// ge:test tests/assets_pcx_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::assets {

struct IndexedImage {
  int width = 0, height = 0;
  std::vector<uint8_t> indices;  // width * height, rows from the top
  std::vector<uint8_t> palette;  // 768 bytes, rgb a colour (the file's), or empty
};

// The image a PCX holds. False with `error` saying what failed, at which byte, and the next step
// ("<where>: <what> at byte N; next: ..."); `where` names the file in the message.
bool decode_pcx(std::span<const uint8_t> bytes, std::string_view where, IndexedImage* out,
                std::string* error);

// The image as a PCX file, with `palette` (768 bytes) appended when it has one.
std::vector<uint8_t> encode_pcx(const IndexedImage& image);

// The image's colours through a palette (768 bytes, the file's when `palette` is empty): RGBA8,
// opaque, rows from the top.
std::vector<uint8_t> pcx_rgba(const IndexedImage& image, std::span<const uint8_t> palette = {});

}  // namespace ge::assets
