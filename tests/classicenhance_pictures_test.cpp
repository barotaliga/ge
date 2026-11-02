// The pack's 2D pictures enhanced (plugins/classicenhance/pictures.hpp's `enhance_picture`), on
// pictures made here: the result is `scale` times the size; see-through texels stay see-through
// away from the drawing and opaque ones opaque; a stroke one texel wide comes out one texel wide;
// the see-through index's colour never bleeds into an edge; the font sheet is enhanced a cell at
// a time, so a glyph that fills its cell does not reach into its neighbour; and the result is the
// same bits every run.
//
// ge:summary Tests the 2D pictures' upscale: size, see-through kept, stroke width, cells, no bleed
// ge:keywords test classicenhance pictures enhance_picture alpha stroke glyph-sheet conchars bleed
// ge:skill classicenhance testing
#include <cstdlib>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/pictures.hpp"

namespace ce = ge::classicenhance;

namespace {

// Grey ramp palette, with the see-through index 255 a loud magenta that must never show.
ce::Palette palette() {
  ce::Palette p;
  for (int i = 0; i < 256; i++) {
    p.rgb[size_t(i) * 3] = uint8_t(i);
    p.rgb[size_t(i) * 3 + 1] = uint8_t(i);
    p.rgb[size_t(i) * 3 + 2] = uint8_t(i);
  }
  p.rgb[size_t{255} * 3 + 1] = 0;  // 255, 0, 255
  return p;
}

int px(const ge::assets::Image& img, int x, int y, int c) {
  return img.rgba[(size_t(y) * size_t(img.width) + size_t(x)) * 4 + size_t(c)];
}

// A `w` x `h` picture all see-through (255) but for a vertical stroke of index 200 at column `sx`.
ce::PackPicture stroke(int w, int h, int sx) {
  ce::PackPicture p;
  p.name = "stroke";
  p.picture.width = w;
  p.picture.height = h;
  p.picture.indices.assign(size_t(w) * size_t(h), 255);
  for (int y = 1; y < h - 1; y++) p.picture.indices[size_t(y) * size_t(w) + size_t(sx)] = 200;
  return p;
}

}  // namespace

TEST(picture_comes_out_scale_times_the_size) {
  const ce::PackPicture p = stroke(12, 10, 5);
  ce::EnhanceOptions o;
  o.scale = 4;
  const ge::assets::Image big = ce::enhance_picture(p, palette(), o);
  CHECK(big.width == 48 && big.height == 40);
  CHECK(big.rgba.size() == size_t{48} * 40 * 4);
  CHECK(ce::enhance_picture(p, palette(), o).rgba == big.rgba);  // the same bits every run
}

TEST(picture_keeps_its_see_through_texels_and_a_strokes_width) {
  const ce::PackPicture p = stroke(12, 10, 5);
  ce::EnhanceOptions o;
  o.scale = 4;
  const ge::assets::Image big = ce::enhance_picture(p, palette(), o);
  // Far from the stroke: see-through. In its middle: opaque.
  CHECK(px(big, 2, 20, 3) == 0 && px(big, 45, 20, 3) == 0);
  CHECK(px(big, 22, 20, 3) == 255);
  // Across a middle row the opaque run (alpha at least a half) is one source texel: 4 pixels.
  int run = 0;
  for (int x = 0; x < big.width; x++) run += px(big, x, 20, 3) >= 128 ? 1 : 0;
  CHECK_MSG(run >= 3 && run <= 5, std::to_string(run));
  // No magenta anywhere a pixel shows: the stroke's grey was spread into the see-through texels.
  for (int y = 0; y < big.height; y++) {
    for (int x = 0; x < big.width; x++) {
      if (px(big, x, y, 3) == 0) continue;
      CHECK(std::abs(px(big, x, y, 0) - px(big, x, y, 1)) < 24);
    }
  }
}

TEST(picture_opaque_stays_opaque_to_its_edges) {
  ce::PackPicture p;
  p.name = "sbar";
  p.picture.width = 8;
  p.picture.height = 4;
  p.picture.indices.assign(32, 100);
  ce::EnhanceOptions o;
  o.scale = 2;
  const ge::assets::Image big = ce::enhance_picture(p, palette(), o);
  for (size_t i = 3; i < big.rgba.size(); i += 4) CHECK(big.rgba[i] == 255);
  // A flat picture stays its colour, edges included (padded, not wrapped).
  CHECK(std::abs(px(big, 0, 0, 0) - 100) <= 2 && std::abs(px(big, 15, 7, 0) - 100) <= 2);
}

TEST(glyph_sheet_is_enhanced_a_cell_at_a_time) {
  // The sheet: cell 0 filled with a bright index, every other cell see-through (index 0).
  ce::PackPicture p;
  p.name = "conchars";
  p.transparent = 0;
  p.picture.width = p.picture.height = 128;
  p.picture.indices.assign(size_t{128} * 128, 0);
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) p.picture.indices[size_t(y) * 128 + size_t(x)] = 220;
  ce::EnhanceOptions o;
  o.scale = 4;
  const ge::assets::Image big = ce::enhance_picture(p, palette(), o);
  CHECK(big.width == 512 && big.height == 512);
  // The filled cell is opaque to its right edge; its neighbour is clear from its left edge.
  CHECK(px(big, 31, 16, 3) == 255 && px(big, 32, 16, 3) == 0);
  CHECK(px(big, 16, 31, 3) == 255 && px(big, 16, 32, 3) == 0);
}
