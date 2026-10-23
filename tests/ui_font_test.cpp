// engine/ui's fonts: the built-in family loads, its metrics and advances behave, kerning narrows a
// pair, the glyph cache keys on size and phase, UTF-8 decodes, and a non-font is refused.
//
// ge:summary Tests the TrueType font: built-in family, metrics, advance, kerning, masks, UTF-8
// ge:keywords test ui font glyph kerning utf8 advance metrics barlow cache
// ge:skill ui testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "ui/font.hpp"

using namespace ge::ui;

TEST(the_builtin_family_loads_at_both_weights) {
  CHECK(ui_font(Weight::semibold).loaded());
  CHECK(ui_font(Weight::medium).loaded());
}

TEST(metrics_scale_with_size) {
  const Font& f = ui_font();
  const VMetrics a = f.metrics(20), b = f.metrics(40);
  CHECK(std::fabs(a.ascent + a.descent - 20) < 0.01f);
  CHECK(std::fabs(b.ascent - 2 * a.ascent) < 0.01f);
  CHECK(a.line() >= 20);
}

TEST(advance_grows_with_text_and_size_and_tracking) {
  const Font& f = ui_font();
  const float one = f.advance("PLAY", 30), two = f.advance("PLAYPLAY", 30);
  CHECK(one > 0 && two > one * 1.9f);
  CHECK(std::fabs(f.advance("PLAY", 60) - 2 * one) < 0.5f);
  CHECK(std::fabs(f.advance("PLAY", 30, 2) - (one + 8)) < 0.01f);
  CHECK(f.advance("", 30) == 0);
}

TEST(kerning_tucks_a_pair_in) {
  const Font& f = ui_font();
  const int a = f.glyph_index('A'), v = f.glyph_index('V');
  CHECK(a != 0 && v != 0);
  CHECK(f.kern(a, v, 100) < 0);
}

TEST(fit_ends_on_a_code_point_inside_the_width) {
  const Font& f = ui_font();
  const std::string s = "SKI ÅRE ÖVER";
  const size_t n = f.fit(s, 30, f.advance("SKI ", 30) + 1);
  CHECK(n == 4);
  CHECK(f.fit(s, 30, 1e6f) == s.size());
  CHECK(f.fit(s, 30, 0) == 0);
}

TEST(masks_are_cached_by_size_and_phase) {
  Font f;
  CHECK(!f.load({}));
  const Font& u = ui_font(Weight::medium);
  const int g = u.glyph_index('O');
  const size_t before = u.cached();
  const GlyphMask& m = u.mask(g, 24, 0);
  CHECK(m.w > 0 && m.h > 0 && static_cast<int>(m.alpha.size()) == m.w * m.h);
  CHECK(u.mask(g, 24, 0.1f).alpha == m.alpha);  // the same quarter
  u.mask(g, 24, 0.5f);
  u.mask(g, 25, 0);
  CHECK(u.cached() == before + 3);
  int lit = 0;
  for (uint8_t a : m.alpha) lit += a > 128;
  CHECK(lit > 10);
  CHECK(m.y0 < 0);  // a capital stands above the baseline
}

TEST(utf8_decodes_and_marks_malformed_bytes) {
  const std::string s = "A\xc3\xa5\xe2\x82\xac\xf0\x9f\x8f\x82\xff";
  size_t i = 0;
  CHECK(next_codepoint(s, i) == 'A');
  CHECK(next_codepoint(s, i) == 0xe5);
  CHECK(next_codepoint(s, i) == 0x20ac);
  CHECK(next_codepoint(s, i) == 0x1f3c2);
  CHECK(next_codepoint(s, i) == 0xfffd);
  CHECK(i == s.size());
  const std::string cut = "\xe2\x82";
  i = 0;
  CHECK(next_codepoint(cut, i) == 0xfffd && i == 1);
}

TEST(a_non_font_is_refused_with_a_reason) {
  const std::vector<uint8_t> junk(64, 7);
  Font f;
  std::string why;
  CHECK(!f.load(junk, &why));
  CHECK(!why.empty());
  CHECK(!f.loaded());
  CHECK(f.advance("X", 10) == 0);
}
