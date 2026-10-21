// engine/lab's raster and PNG writer: the checksums against their published check values, the
// PNG's framing, blending, and the font's glyphs.
//
// ge:summary Tests the labs' raster painters, the 5x7 font and the PNG writer's framing
// ge:keywords test lab raster png crc32 font blend picture
// ge:skill labs testing
#include <cstring>
#include <string>

#include "check.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"

using namespace ge::lab;

namespace {

// The red byte of pixel (x, y).
uint8_t red(const Picture& p, int x, int y) {
  return p.rgba[(static_cast<size_t>(y) * p.width + x) * 4];
}

int lit(const Picture& p) {
  int n = 0;
  for (size_t i = 0; i < p.rgba.size(); i += 4) n += p.rgba[i] != 0;
  return n;
}

}  // namespace

TEST(crc32_check_value) {
  const char* s = "123456789";
  CHECK(crc32(reinterpret_cast<const uint8_t*>(s), 9) == 0xcbf43926u);
}

TEST(png_is_framed) {
  Picture p = create_picture(3, 2, {10, 20, 30});
  const auto png = encode_png(p);
  const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  CHECK(png.size() > 8 + 25 + 12);
  CHECK(std::memcmp(png.data(), sig, 8) == 0);
  CHECK(std::memcmp(png.data() + 12, "IHDR", 4) == 0);
  CHECK(std::memcmp(png.data() + png.size() - 8, "IEND", 4) == 0);
  // The IHDR's CRC covers its type and data.
  const uint32_t crc =
      (uint32_t(png[29]) << 24) | (uint32_t(png[30]) << 16) | (uint32_t(png[31]) << 8) | png[32];
  CHECK(crc == crc32(png.data() + 12, 17));
}

TEST(png_stored_blocks_span_a_big_picture) {
  // 200 x 100 RGBA is 80100 raw bytes: two stored blocks.
  Picture p = create_picture(200, 100, {1, 2, 3});
  const auto png = encode_png(p);
  CHECK(png.size() > 80100);
}

TEST(blend_and_clip) {
  Picture p = create_picture(4, 4, {0, 0, 0});
  set_pixel(p, 1, 1, {255, 255, 255, 128});
  CHECK(red(p, 1, 1) == 128);
  set_pixel(p, -1, 9, {255, 0, 0});  // off the picture: dropped, no crash
  fill_rect(p, 0, 0, 2, 2, {200, 0, 0});
  CHECK(p.rgba[0] == 200);
  CHECK(red(p, 1, 1) == 200);
}

TEST(font_glyphs) {
  Picture p = create_picture(20, 10, {0, 0, 0});
  draw_text(p, 0, 0, "-", {255, 255, 255});
  CHECK(lit(p) == 5);
  Picture q = create_picture(20, 10, {0, 0, 0});
  draw_text(q, 0, 0, "a", {255, 255, 255});
  Picture r = create_picture(20, 10, {0, 0, 0});
  draw_text(r, 0, 0, "A", {255, 255, 255});
  CHECK(q.rgba == r.rgba);  // lower case folds to upper
  Picture s = create_picture(20, 10, {0, 0, 0});
  draw_text(s, 0, 0, "~", {255, 255, 255});
  CHECK(lit(s) == 0);  // a glyph the font lacks draws blank
  CHECK(text_width("abc", 2) == 3 * kGlyphW * 2);
}

TEST(line_reaches_both_ends) {
  Picture p = create_picture(10, 10, {0, 0, 0});
  draw_line(p, 1, 1, 8, 5, {255, 255, 255});
  CHECK(red(p, 1, 1) == 255);
  CHECK(red(p, 8, 5) == 255);
}
