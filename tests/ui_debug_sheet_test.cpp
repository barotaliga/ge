// engine/ui/debug_sheet.hpp: the frame keeps every pixel and a band is added below it; the notes
// are drawn in the band, a heading in its own colour; a line wider than the picture wraps onto
// more rows (a taller band); the text grows with the picture's width.
//
// ge:summary Tests for the debug sheet: the frame kept, the band, the wrap, the heading
// ge:keywords test debug-sheet notes band wrap
// ge:skill testing ui
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "ui/debug_sheet.hpp"

namespace {

namespace gu = ge::ui;

std::vector<uint8_t> frame(int w, int h) {
  std::vector<uint8_t> px(static_cast<size_t>(w * h * 4));
  for (size_t i = 0; i < px.size(); i += 4) {
    px[i] = static_cast<uint8_t>(i % 251), px[i + 1] = 80, px[i + 2] = 160, px[i + 3] = 255;
  }
  return px;
}

// How many band pixels are not the band's fill: drawn text.
int inked(const gu::Image& im, int from_row) {
  int n = 0;
  for (int y = from_row; y < im.height; ++y)
    for (int x = 0; x < im.width; ++x) {
      const uint8_t* p = &im.rgba[(static_cast<size_t>(y) * static_cast<size_t>(im.width) +
                                   static_cast<size_t>(x)) *
                                  4];
      n += (p[0] != 0x11 || p[1] != 0x13 || p[2] != 0x18) ? 1 : 0;
    }
  return n;
}

}  // namespace

TEST(the_frame_is_kept_and_the_notes_go_below_it) {
  const std::vector<uint8_t> px = frame(320, 120);
  const std::vector<std::string> lines = {"# where", "player at (12 -40 24) units",
                                          "r_look modern"};
  const gu::Image im = gu::debug_sheet(320, 120, px, lines);
  CHECK(im.width == 320);
  CHECK(im.height > 120 + 3 * 16);
  CHECK(std::equal(px.begin(), px.end(), im.rgba.begin()));
  CHECK(inked(im, 120) > 200);
}

TEST(a_long_line_wraps_onto_more_rows) {
  const std::vector<uint8_t> px = frame(200, 50);
  const std::vector<std::string> one = {"short"};
  const std::vector<std::string> wide = {
      "a line much wider than two hundred pixels of eight pixel letters can hold in one row"};
  const gu::Image a = gu::debug_sheet(200, 50, px, one), b = gu::debug_sheet(200, 50, px, wide);
  CHECK(b.height > a.height + 2 * 16);
}

TEST(a_heading_is_drawn_in_its_colour) {
  const std::vector<uint8_t> px = frame(200, 20);
  const std::vector<std::string> lines = {"# gpu"};
  const gu::Image im = gu::debug_sheet(200, 20, px, lines);
  bool gold = false;
  for (size_t i = static_cast<size_t>(200 * 20 * 4); i + 3 < im.rgba.size(); i += 4)
    gold |= im.rgba[i] > 200 && im.rgba[i + 1] > 150 && im.rgba[i + 2] < 120;
  CHECK(gold);
}

TEST(the_text_grows_with_the_picture) {
  CHECK(gu::sheet_text_size(320) == 16);
  CHECK(gu::sheet_text_size(1280) == 16);
  CHECK(gu::sheet_text_size(1920) == 32);
  CHECK(gu::sheet_text_size(2560) == 32);
  CHECK(gu::sheet_text_size(3840) == 48);
}
