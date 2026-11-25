// The charsets as grid fonts: a name the charsets know builds a font (the same one every call),
// an unknown name builds none; its metrics and advance are shares of the cell, its glyphs ink
// their cells and a code point past the grid draws the missing glyph; a pixel font draws at whole
// multiples of its grid; a game's glyph sheet is a square pixel font shaded by its brightness.
//
// ge:summary Tests the charsets as grid fonts: lookup, metrics, advance, masks, pixel snapping
// ge:keywords test ui charset grid-font font atlas monospace pixel-font load_grid
// ge:skill ui testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "ui/charset_font.hpp"
#include "ui/font.hpp"
#include "ui/image.hpp"

using namespace ge::ui;

namespace {

int lit(const GlyphMask& m) {
  int n = 0;
  for (uint8_t a : m.alpha) n += a > 128;
  return n;
}

}  // namespace

TEST(a_charset_by_name_and_none_for_an_unknown_one) {
  const Font* mono = charset_font("mono");
  CHECK(mono != nullptr && mono->loaded() && mono->grid());
  CHECK(charset_font("mono") == mono);  // built once
  CHECK(charset_font("no_such_font") == nullptr);
  CHECK(charset_font("") == nullptr);
  CHECK(!ui_font().grid());
}

TEST(a_drawn_grid_font_is_monospaced_in_shares_of_its_cell) {
  const Font& f = *charset_font("mono");
  const VMetrics m = f.metrics(20);
  CHECK(std::fabs(m.ascent + m.descent - 20) < 0.01f && m.line_gap == 0);
  CHECK(std::fabs(f.advance("i", 20) - f.advance("W", 20)) < 0.01f);
  CHECK(std::fabs(f.advance("abcd", 20) - 4 * f.advance("a", 20)) < 0.01f);
  CHECK(std::fabs(f.advance("a", 40) - 2 * f.advance("a", 20)) < 0.01f);
  CHECK(f.kern(f.glyph_index('A'), f.glyph_index('V'), 40) == 0);
}

TEST(a_glyph_inks_its_cell_and_past_the_grid_is_the_missing_one) {
  const Font& f = *charset_font("mono");
  CHECK(f.glyph_index('A') == 'A');
  CHECK(f.glyph_index(0xe5) == f.glyph_index(0x20ac));  // both past the grid: one glyph
  const GlyphMask& a = f.mask(f.glyph_index('A'), 32, 0);
  CHECK(a.h == 32 && static_cast<int>(a.alpha.size()) == a.w * a.h);
  CHECK(lit(a) > 20);
  CHECK(a.y0 < 0);  // the cell stands on the baseline
  CHECK(lit(f.mask(f.glyph_index(' '), 32, 0)) == 0);
  CHECK(f.mask(f.glyph_index('A'), 32, 0.5f).alpha != a.alpha);  // a drawn face takes the phase
}

TEST(a_pixel_font_draws_at_whole_multiples_of_its_grid) {
  const Font& f = *charset_font("terminal");
  CHECK(f.grid());
  CHECK(f.metrics(20).ascent + f.metrics(20).descent == 16);
  CHECK(f.metrics(40).ascent + f.metrics(40).descent == 32);
  CHECK(f.metrics(8).ascent + f.metrics(8).descent == 16);  // never below one grid
  const GlyphMask& m = f.mask(f.glyph_index('H'), 32, 0);
  CHECK(m.h == 32);
  // Nearest: every texel doubled, so each 2x2 block of the mask is one value.
  bool blocks = true;
  for (int y = 0; y + 1 < m.h; y += 2)
    for (int x = 0; x + 1 < m.w; x += 2) {
      const uint8_t v = m.alpha[static_cast<size_t>(y) * m.w + x];
      blocks = blocks && m.alpha[static_cast<size_t>(y) * m.w + x + 1] == v &&
               m.alpha[static_cast<size_t>(y + 1) * m.w + x] == v;
    }
  CHECK(blocks && lit(m) > 20);
}

TEST(a_glyph_sheet_is_a_square_pixel_font_shaded_by_its_brightness) {
  // A 128 x 128 sheet: 'A' a full white cell, 'B' a half-bright one, the rest see-through.
  Image sheet = make_image(128, 128, {0, 0, 0, 0});
  const auto fill = [&](int code, uint8_t v) {
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        uint8_t* px = &sheet.rgba[(static_cast<size_t>((code >> 4) * 8 + y) * 128 +
                                   static_cast<size_t>((code & 15) * 8 + x)) *
                                  4];
        px[0] = px[1] = px[2] = v;
        px[3] = 255;
      }
  };
  fill('A', 255);
  fill('B', 128);
  Font f;
  CHECK(sheet_font(sheet, f));
  CHECK(f.grid());
  CHECK(f.metrics(20).ascent + f.metrics(20).descent == 16);  // a whole multiple of 8
  CHECK(std::fabs(f.advance("AB", 16) - 32) < 0.01f);         // square cells
  const GlyphMask& a = f.mask(f.glyph_index('A'), 16, 0);
  const GlyphMask& b = f.mask(f.glyph_index('B'), 16, 0);
  CHECK(lit(a) == 16 * 16 && lit(b) == 0 && b.alpha[0] >= 120 && b.alpha[0] <= 136);
  CHECK(lit(f.mask(f.glyph_index('C'), 16, 0)) == 0);
  Font none;
  std::string why;
  CHECK(!sheet_font(make_image(100, 64, {0, 0, 0, 0}), none, &why) && !why.empty());

  // An upscaled sheet (32 px cells): ink run over a text cell's edge (a space's top, from the
  // row above) is cleared; a bar piece's cell, which tiles edge to edge, keeps its edges.
  Image big = make_image(512, 512, {0, 0, 0, 0});
  const auto ink = [&](int code, int x, int y) {
    uint8_t* px = &big.rgba[(static_cast<size_t>((code >> 4) * 32 + y) * 512 +
                             static_cast<size_t>((code & 15) * 32 + x)) *
                            4];
    px[0] = px[1] = px[2] = px[3] = 255;
  };
  for (int x = 0; x < 32; ++x) ink(' ', x, 0), ink(129, x, 0);  // a dash on the top rows
  ink('A', 16, 16);                                             // a letter's middle
  Font up;
  CHECK(sheet_font(big, up));
  CHECK(lit(up.mask(up.glyph_index(' '), 32, 0)) == 0);
  CHECK(lit(up.mask(up.glyph_index('A'), 32, 0)) > 0);
  CHECK(lit(up.mask(129, 32, 0)) > 0);
  // It is drawn at the classic sheet's sizes (whole multiples of 8 px), not of its 32 px cells,
  // and filtered: a cell drawn at 16 px from 32 texels has partial cover at its ink's edges.
  CHECK(up.metrics(20).ascent + up.metrics(20).descent == 16);
  CHECK(up.metrics(8).ascent + up.metrics(8).descent == 8);
  CHECK(std::fabs(up.advance("AB", 16) - 32) < 0.01f);
  Image soft = make_image(512, 512, {0, 0, 0, 0});
  for (int y = 9; y < 23; ++y)
    for (int x = 9; x < 23; ++x) {  // an odd-edged square: its edges fall inside a pixel
      uint8_t* px =
          &soft.rgba[(static_cast<size_t>(4 * 32 + y) * 512 + static_cast<size_t>(1 * 32 + x)) * 4];
      px[0] = px[1] = px[2] = px[3] = 255;
    }
  Font sf;
  CHECK(sheet_font(soft, sf));
  const GlyphMask& m = sf.mask(sf.glyph_index('A'), 16, 0);
  bool partial = false;
  for (const uint8_t v : m.alpha) partial = partial || (v > 0 && v < 255);
  CHECK(m.h == 16 && lit(m) > 0 && partial);
}
