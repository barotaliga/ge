// The palette and the colour map (engine/vfs/palette.hpp): both round trip byte for byte, the
// colour map with and without its trailing byte, a level and an index read the right cell, and a
// table of any other size is refused with the sizes named.
//
// ge:summary Tests: the palette and the colour map round trip and refuse a wrong size
// ge:keywords test vfs palette colormap round-trip refuse fullbright
// ge:skill vfs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "vfs/palette.hpp"

namespace vfs = ge::vfs;

TEST(the_palette_and_the_colour_map_round_trip) {
  vfs::Palette pal;
  for (size_t i = 0; i < pal.rgb.size(); ++i) pal.rgb[i] = uint8_t(i * 5);
  vfs::Palette back;
  std::string error;
  CHECK_MSG(vfs::read_palette(vfs::write_palette(pal), &back, &error), error);
  CHECK(back.rgb == pal.rgb);

  vfs::Colormap cm;
  for (size_t i = 0; i < cm.map.size(); ++i) cm.map[i] = uint8_t(i / 256 + i % 256);
  vfs::Colormap got;
  CHECK_MSG(vfs::read_colormap(vfs::write_colormap(cm), &got, &error), error);
  CHECK(got.map == cm.map && got.fullbright == -1);
  CHECK(got.at(10, 20) == 30);
  cm.fullbright = 32;
  const std::vector<uint8_t> bytes = vfs::write_colormap(cm);
  CHECK(bytes.size() == 16385);
  CHECK_MSG(vfs::read_colormap(bytes, &got, &error), error);
  CHECK(got.fullbright == 32);
}

TEST(a_table_of_the_wrong_size_is_refused_naming_the_sizes) {
  vfs::Palette pal;
  std::string error;
  CHECK(!vfs::read_palette(std::vector<uint8_t>(767), &pal, &error));
  CHECK(error.find("767") != std::string::npos && error.find("768") != std::string::npos);
  vfs::Colormap cm;
  CHECK(!vfs::read_colormap(std::vector<uint8_t>(16386), &cm, &error));
  CHECK(error.find("16386") != std::string::npos);
}
