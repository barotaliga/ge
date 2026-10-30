// THE PALETTE AND THE COLOUR MAP: the two tables an indexed picture's colours come from. The
// PALETTE (`gfx/palette.lmp`) is 256 colours of three bytes each, red, green, blue, 768 bytes in
// all. The COLOUR MAP (`gfx/colormap.lmp`) is 64 rows of 256 indices, one row a light level from
// full bright (row 0) to dark (row 63): row r, column i is the index index i is drawn with at
// light level r. The file may carry one trailing byte past the 16384 (the count of full-bright
// columns some tools write); it is read and kept, never required.
//
// The readers refuse a table of the wrong size with the sizes named, and an index in the colour
// map is in range by construction (a byte). `write_palette` and `write_colormap` make both for
// tests and fixtures.
//
// ge:summary Reads and writes the 256-colour palette and the 64-level colour map of indexed art
// ge:keywords vfs palette colormap colour-map light-level indexed lmp fullbright read write
// ge:skill vfs
// ge:test tests/vfs_palette_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::vfs {

inline constexpr size_t kPaletteColours = 256;
inline constexpr size_t kColormapLevels = 64;

struct Palette {
  std::array<uint8_t, 3 * kPaletteColours> rgb{};  // red, green, blue per index
};

struct Colormap {
  std::array<uint8_t, kColormapLevels * kPaletteColours> map{};  // [level * 256 + index]
  int fullbright = -1;  // the trailing byte when the file carries one, else -1

  uint8_t at(int level, uint8_t index) const { return map[size_t(level) * 256 + index]; }
};

// The palette in `bytes` (exactly 768); false with the reason in `*error`.
bool read_palette(std::span<const uint8_t> bytes, Palette* out, std::string* error);
// The colour map in `bytes` (16384, or 16385 with the trailing byte); false with the reason.
bool read_colormap(std::span<const uint8_t> bytes, Colormap* out, std::string* error);

std::vector<uint8_t> write_palette(const Palette& palette);
std::vector<uint8_t> write_colormap(const Colormap& colormap);

}  // namespace ge::vfs
