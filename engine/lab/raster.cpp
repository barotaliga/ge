// The raster's painters and its 5x7 font (the printable ASCII the labs caption with). Every painter
// rounds with num::round and blends in doubles before the one truncation to a byte, so a picture's
// bytes are the same on every compiler row.
//
// ge:summary The raster's painters and the 5x7 glyph table
// ge:keywords raster painters font glyph font-5x7 blend line disc text
// ge:skill labs
// ge:test tests/lab_raster_test.cpp
#include "lab/raster.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::lab {

namespace {

struct Glyph {
  char ch;
  uint8_t rows[7];
};

// One 5-bit row per scanline, MSB leftmost.
constexpr Glyph kFont[] = {
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
    {'C', {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}},
    {'D', {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
    {'G', {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f}},
    {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'I', {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'Q', {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}},
    {'X', {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}},
    {'0', {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}},
    {'1', {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}},
    {'2', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}},
    {'3', {0x1f, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0e}},
    {'4', {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}},
    {'5', {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e}},
    {'6', {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e}},
    {'7', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}},
    {'9', {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c}},
    {'-', {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x0c, 0x04, 0x08}},
    {':', {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'%', {0x19, 0x1a, 0x02, 0x04, 0x08, 0x0b, 0x13}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'[', {0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e}},
    {']', {0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e}},
    {'\'', {0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00}},
    {'"', {0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00}},
    {'=', {0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00}},
    {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},
    {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},
    {'?', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f}},
    {'#', {0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a}},
    {'*', {0x00, 0x04, 0x15, 0x0e, 0x15, 0x04, 0x00}},
};

const uint8_t* glyph(char ch) {
  if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
  for (const Glyph& g : kFont) {
    if (g.ch == ch) return g.rows;
  }
  return nullptr;
}

int round_px(double v) { return static_cast<int>(core::num::round(v)); }

}  // namespace

Picture create_picture(int width, int height, Rgba background) {
  Picture p{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * height * 4)};
  for (size_t i = 0; i < p.rgba.size(); i += 4) {
    p.rgba[i] = background.r;
    p.rgba[i + 1] = background.g;
    p.rgba[i + 2] = background.b;
    p.rgba[i + 3] = background.a;
  }
  return p;
}

void set_pixel(Picture& p, int x, int y, Rgba c) {
  if (x < 0 || y < 0 || x >= p.width || y >= p.height || c.a == 0) return;
  uint8_t* o = &p.rgba[(static_cast<size_t>(y) * p.width + x) * 4];
  if (c.a == 255) {
    o[0] = c.r;
    o[1] = c.g;
    o[2] = c.b;
    o[3] = 255;
    return;
  }
  const double t = c.a / 255.0;
  o[0] = static_cast<uint8_t>(o[0] + (c.r - o[0]) * t);
  o[1] = static_cast<uint8_t>(o[1] + (c.g - o[1]) * t);
  o[2] = static_cast<uint8_t>(o[2] + (c.b - o[2]) * t);
  if (c.a > o[3]) o[3] = c.a;
}

void fill_rect(Picture& p, double x, double y, double w, double h, Rgba c) {
  const int x0 = std::max(0, static_cast<int>(std::floor(x)));
  const int y0 = std::max(0, static_cast<int>(std::floor(y)));
  const int x1 = std::min(p.width, static_cast<int>(std::ceil(x + w)));
  const int y1 = std::min(p.height, static_cast<int>(std::ceil(y + h)));
  for (int py = y0; py < y1; py++) {
    for (int px = x0; px < x1; px++) set_pixel(p, px, py, c);
  }
}

void draw_line(Picture& p, double x0, double y0, double x1, double y1, Rgba c, double thickness) {
  const int steps = std::max(1, static_cast<int>(std::ceil(core::hypot(x1 - x0, y1 - y0))));
  const double r = thickness / 2;
  // A blended stroke must not paint a pixel twice, or its seams read darker: remember the
  // last pixel painted (a one-pixel line revisits only its neighbour).
  int last_x = INT32_MIN, last_y = INT32_MIN;
  for (int i = 0; i <= steps; i++) {
    const double t = static_cast<double>(i) / steps;
    const double x = x0 + (x1 - x0) * t;
    const double y = y0 + (y1 - y0) * t;
    if (thickness <= 1) {
      const int px = round_px(x), py = round_px(y);
      if (px == last_x && py == last_y) continue;
      last_x = px;
      last_y = py;
      set_pixel(p, px, py, c);
      continue;
    }
    for (int dy = static_cast<int>(-r); dy <= static_cast<int>(r); dy++) {
      for (int dx = static_cast<int>(-r); dx <= static_cast<int>(r); dx++) {
        if (dx * dx + dy * dy <= r * r) set_pixel(p, round_px(x + dx), round_px(y + dy), c);
      }
    }
  }
}

void fill_disc(Picture& p, double cx, double cy, double radius, Rgba c) {
  for (int y = static_cast<int>(std::floor(cy - radius)); y <= cy + radius; y++) {
    for (int x = static_cast<int>(std::floor(cx - radius)); x <= cx + radius; x++) {
      const double dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= radius * radius) set_pixel(p, x, y, c);
    }
  }
}

void draw_ring(Picture& p, double cx, double cy, double radius, Rgba c) {
  const double inner = std::max(0.0, radius - 1);
  for (int y = static_cast<int>(std::floor(cy - radius)); y <= cy + radius; y++) {
    for (int x = static_cast<int>(std::floor(cx - radius)); x <= cx + radius; x++) {
      const double d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
      if (d2 <= radius * radius && d2 >= inner * inner) set_pixel(p, x, y, c);
    }
  }
}

void draw_text(Picture& p, double x, double y, std::string_view text, Rgba c, int scale) {
  const int ox = round_px(x), oy = round_px(y);
  int cx = ox;
  for (const char ch : text) {
    if (const uint8_t* g = glyph(ch)) {
      for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
          if (!(g[row] & (0x10 >> col))) continue;
          fill_rect(p, cx + col * scale, oy + row * scale, scale, scale, c);
        }
      }
    }
    cx += kGlyphW * scale;
  }
}

int text_width(std::string_view text, int scale) {
  return static_cast<int>(text.size()) * kGlyphW * scale;
}

void blit(Picture& dst, const Picture& src, int x, int y) {
  for (int sy = 0; sy < src.height; sy++) {
    const int dy = y + sy;
    if (dy < 0 || dy >= dst.height) continue;
    for (int sx = 0; sx < src.width; sx++) {
      const int dx = x + sx;
      if (dx < 0 || dx >= dst.width) continue;
      const uint8_t* s = &src.rgba[(static_cast<size_t>(sy) * src.width + sx) * 4];
      set_pixel(dst, dx, dy, Rgba{s[0], s[1], s[2], s[3]});
    }
  }
}

Picture resized(const Picture& src, int width, int height) {
  Picture out;
  out.width = std::max(width, 0);
  out.height = std::max(height, 0);
  out.rgba.assign(static_cast<size_t>(out.width) * out.height * 4, 0);
  if (src.width <= 0 || src.height <= 0) return out;
  const int64_t sw = src.width, sh = src.height, dw = out.width, dh = out.height;
  for (int64_t dy = 0; dy < dh; dy++) {
    const int64_t y0 = dy * sh / dh, y1 = std::max(y0 + 1, (dy + 1) * sh / dh);
    for (int64_t dx = 0; dx < dw; dx++) {
      const int64_t x0 = dx * sw / dw, x1 = std::max(x0 + 1, (dx + 1) * sw / dw);
      uint64_t sum[4] = {0, 0, 0, 0};
      for (int64_t y = y0; y < y1; y++) {
        const uint8_t* row = &src.rgba[static_cast<size_t>(y * sw + x0) * 4];
        for (int64_t x = x0; x < x1; x++, row += 4) {
          for (int c = 0; c < 4; c++) sum[c] += row[c];
        }
      }
      const uint64_t n = static_cast<uint64_t>((y1 - y0) * (x1 - x0));
      uint8_t* d = &out.rgba[static_cast<size_t>(dy * dw + dx) * 4];
      for (int c = 0; c < 4; c++) d[c] = static_cast<uint8_t>((sum[c] + n / 2) / n);
    }
  }
  return out;
}

}  // namespace ge::lab
