// engine/lab's raster and PNG writer: the checksums against their published check values, the
// PNG's framing, blending, the font's glyphs, and the inflater over every deflate block kind.
//
// ge:summary Tests the labs' raster painters, the 5x7 font, the PNG writer and the inflater
// ge:keywords test lab raster png crc32 font blend picture inflate
// ge:skill labs testing
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/inflate.hpp"
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

// The inflater: a stored stream (the PNG writer's IDAT), a fixed-Huffman one and a dynamic one
// (each written by a stock zlib at level 9), and a torn one refused.
TEST(inflate_reads_every_block_kind) {
  const std::vector<uint8_t> fixed{120, 218, 43,  201, 72,  85,  40,  206, 203, 47,  215,
                                   81,  40,  193, 96,  41,  228, 231, 129, 217, 185, 249,
                                   165, 121, 37,  137, 153, 121, 0,   96,  221, 15,  250};
  const auto a = ge::core::inflate_zlib(fixed);
  CHECK(a && std::string(a->begin(), a->end()) == "the snow, the snow, the snow on the mountain");
  const std::vector<uint8_t> dynamic{
      120, 218, 213, 139, 219, 9,   128, 48,  16,  4,   91,  217, 2,   196, 6,   172, 38,  144,
      135, 129, 123, 132, 75,  78,  73,  247, 74,  172, 194, 207, 153, 217, 181, 26,  75,  66,
      144, 136, 43,  16,  165, 185, 161, 55,  183, 37,  138, 19,  205, 3,   246, 155, 197, 56,
      19,  242, 219, 64,  85,  18,  204, 165, 35,  234, 45,  203, 115, 104, 200, 166, 188, 160,
      59,  115, 29,  24,  250, 93,  72,  213, 246, 7,   50,  222, 94,  184};
  std::string want;
  for (int i = 0; i < 6; i++) want += "ridge and valley, spur and gully; ";
  want += "the fall line runs down the map from the summit to the floor.";
  const auto b = ge::core::inflate_zlib(dynamic);
  CHECK(b && std::string(b->begin(), b->end()) == want);
  // A torn stream, and one whose checksum is off, are refused.
  CHECK(!ge::core::inflate_zlib(std::span(dynamic).first(40)));
  std::vector<uint8_t> bad = fixed;
  bad.back() ^= 1;
  CHECK(!ge::core::inflate_zlib(bad));
  // The same blocks with no zlib wrapper, as a zip entry stores them: read to the size the
  // container states, and refused at any other size.
  const std::span<const uint8_t> raw = std::span(dynamic).subspan(2, dynamic.size() - 6);
  const auto c = ge::core::inflate_raw(raw, want.size());
  CHECK(c && std::string(c->begin(), c->end()) == want);
  CHECK(!ge::core::inflate_raw(raw, want.size() + 1));
  // A gzip file (written by a stock gzip at -9n): read, and refused when torn or its CRC is off.
  // The second carries the original's name in its header, which is skipped.
  const std::vector<uint8_t> gz{31,  139, 8,   0,   0,   0,   0,   0,   2,   3,   43,  201,
                                72,  85,  40,  206, 203, 47,  87,  200, 207, 83,  40,  1,
                                178, 115, 243, 75,  243, 74,  18,  51,  243, 116, 192, 60,
                                108, 50,  92,  0,   76,  223, 115, 61,  51,  0,   0,   0};
  const auto g = ge::core::inflate_gzip(gz);
  CHECK(g && std::string(g->begin(), g->end()) ==
                 "the snow on the mountain, the snow on the mountain\n");
  CHECK(!ge::core::inflate_gzip(std::span(gz).first(30)));
  std::vector<uint8_t> gz_bad = gz;
  gz_bad[gz.size() - 8] ^= 1;
  CHECK(!ge::core::inflate_gzip(gz_bad));
  const std::vector<uint8_t> named{31,  139, 8,   8,   249, 195, 228, 106, 0, 3,
                                   103, 122, 110, 97,  109, 101, 0,   171, 0, 0,
                                   131, 22,  220, 140, 1,   0,   0,   0};
  const auto n = ge::core::inflate_gzip(named);
  CHECK(n && n->size() == 1 && (*n)[0] == 'x');
  // Stored blocks: the PNG writer's image data back to its filtered rows.
  ge::lab::Picture pic = ge::lab::create_picture(300, 200, {10, 20, 30});
  const std::vector<uint8_t> png = ge::lab::encode_png(pic);
  size_t at = 8;
  std::vector<uint8_t> idat;
  while (at + 8 <= png.size()) {
    const size_t len = (size_t(png[at]) << 24) | (size_t(png[at + 1]) << 16) |
                       (size_t(png[at + 2]) << 8) | png[at + 3];
    if (std::memcmp(&png[at + 4], "IDAT", 4) == 0) {
      idat.insert(idat.end(), png.begin() + long(at + 8), png.begin() + long(at + 8 + len));
    }
    at += 12 + len;
  }
  const auto rows = ge::core::inflate_zlib(idat);
  CHECK(rows && rows->size() == size_t(200) * (1 + 300 * 4));
  CHECK(rows && (*rows)[1] == 10 && (*rows)[2] == 20 && (*rows)[3] == 30);
}
