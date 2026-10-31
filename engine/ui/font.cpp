// A TrueType font over stb_truetype, defined: loading, metrics, the advance with kerning, the
// glyph masks cached by glyph, size and quarter-pixel phase, UTF-8 decoding, and the compiled-in
// family. This is the only file that includes stb_truetype, and it compiles the implementation
// with the engine's libm in place of the platform's, so a glyph's coverage is the same bits on
// every row and a lab picture's digest with it.
//
// ge:summary The TrueType font over stb_truetype: glyph cache, kerning, UTF-8, built-in fonts
// ge:keywords ui font stb_truetype glyph cache kerning utf8 codepoint embed barlow
// ge:keywords grid-font load_grid area-mean pixel-font
// ge:skill ui
// ge:test tests/ui_font_test.cpp
#include "ui/font.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_ifloor(x) (static_cast<int>(std::floor(x)))
#define STBTT_iceil(x) (static_cast<int>(std::ceil(x)))
#define STBTT_sqrt(x) std::sqrt(x)
#define STBTT_pow(x, y) ::ge::core::libm::pow(x, y)
#define STBTT_fmod(x, y) std::fmod(x, y)
#define STBTT_cos(x) ::ge::core::libm::cos(x)
#define STBTT_acos(x) ::ge::core::libm::acos(x)
#define STBTT_fabs(x) std::fabs(x)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wshadow"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <stb_truetype.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

extern const unsigned char ge_ui_font_semibold[];
extern const std::size_t ge_ui_font_semibold_size;
extern const unsigned char ge_ui_font_medium[];
extern const std::size_t ge_ui_font_medium_size;

namespace ge::ui {

struct Font::Impl {
  stbtt_fontinfo info{};
  int ascent = 0, descent = 0, line_gap = 0;  // font units
  std::vector<GridFace> grid;  // a grid font's faces, smallest first; empty: TrueType
};

Font::Font() = default;
Font::~Font() = default;
Font::Font(Font&&) noexcept = default;
Font& Font::operator=(Font&&) noexcept = default;

bool Font::load(std::span<const uint8_t> ttf, std::string* why) {
  impl_.reset();
  cache_.clear();
  const int offset = ttf.empty() ? -1 : stbtt_GetFontOffsetForIndex(ttf.data(), 0);
  auto impl = std::make_unique<Impl>();
  if (offset < 0 || !stbtt_InitFont(&impl->info, ttf.data(), offset)) {
    if (why) *why = "not a TrueType or OpenType font (stb_truetype could not read its tables)";
    return false;
  }
  stbtt_GetFontVMetrics(&impl->info, &impl->ascent, &impl->descent, &impl->line_gap);
  impl_ = std::move(impl);
  return true;
}

bool Font::load_grid(std::vector<GridFace> faces, std::string* why) {
  impl_.reset();
  cache_.clear();
  for (const GridFace& f : faces) {
    if (f.size <= 0 || f.columns <= 0 || f.rows <= 0 ||
        f.coverage.size() != static_cast<size_t>(f.size) * static_cast<size_t>(f.size)) {
      if (why) *why = "a grid face's coverage is not size x size bytes, or its grid is empty";
      return false;
    }
  }
  if (faces.empty()) {
    if (why) *why = "a grid font needs at least one face";
    return false;
  }
  std::sort(faces.begin(), faces.end(),
            [](const GridFace& a, const GridFace& b) { return a.size < b.size; });
  auto impl = std::make_unique<Impl>();
  impl->grid = std::move(faces);
  impl_ = std::move(impl);
  return true;
}

bool Font::grid() const { return impl_ && !impl_->grid.empty(); }

namespace {

float scale_of(const stbtt_fontinfo& info, float size) {
  return stbtt_ScaleForPixelHeight(&info, size);
}

// The cell height a grid font draws at `size`: a pixel face's at a whole multiple of its grid.
float grid_cell(const std::vector<GridFace>& faces, float size) {
  const int pixel_h = faces.front().pixel_h;
  if (pixel_h <= 0) return size;
  const float h = static_cast<float>(pixel_h);
  return std::max(1.0f, std::floor(size / h)) * h;
}

// The smallest face whose cells are at least `cell` px tall (only ever shrunk), else the largest.
const GridFace& face_for(const std::vector<GridFace>& faces, float cell) {
  for (const GridFace& f : faces) {
    const int ch = f.size / f.rows;  // whole pixels a cell
    if (static_cast<float>(ch) >= cell) return f;
  }
  return faces.back();
}

// A grid glyph's cell scaled to `cell` px tall at the pen's `shift` (0..1 px): an area mean of
// the texels under each pixel, or a pixel face's texel at each pixel's centre.
GlyphMask grid_mask(const std::vector<GridFace>& faces, int glyph, float cell, float shift) {
  const GridFace& f = face_for(faces, cell);
  const int cw = f.size / f.columns, ch = f.size / f.rows;
  const int sx0 = (glyph % f.columns) * cw, sy0 = (glyph / f.columns) * ch;
  const float scale = cell / static_cast<float>(ch);  // pixels a texel
  const bool pixel = f.pixel_h > 0;
  if (pixel) shift = 0;
  GlyphMask m;
  m.w = static_cast<int>(std::ceil(static_cast<float>(cw) * scale + shift));
  m.h = static_cast<int>(std::ceil(cell));
  m.x0 = 0;
  m.y0 = -static_cast<int>(std::lround(f.baseline * cell));
  m.alpha.assign(static_cast<size_t>(m.w) * static_cast<size_t>(m.h), 0);
  auto texel = [&](int x, int y) {
    return static_cast<float>(f.coverage[static_cast<size_t>(sy0 + y) * f.size + sx0 + x]);
  };
  for (int oy = 0; oy < m.h; ++oy) {
    for (int ox = 0; ox < m.w; ++ox) {
      float v = 0;
      if (pixel) {
        const int x = static_cast<int>(std::floor((static_cast<float>(ox) + 0.5f) / scale));
        const int y = static_cast<int>(std::floor((static_cast<float>(oy) + 0.5f) / scale));
        if (x < cw && y < ch) v = texel(x, y);
      } else {
        const float ax = (static_cast<float>(ox) - shift) / scale, bx = ax + 1 / scale;
        const float ay = static_cast<float>(oy) / scale, by = ay + 1 / scale;
        float sum = 0;
        for (int y = std::max(0, static_cast<int>(std::floor(ay)));
             y < std::min(ch, static_cast<int>(std::ceil(by))); ++y) {
          const float wy =
              std::min(by, static_cast<float>(y + 1)) - std::max(ay, static_cast<float>(y));
          for (int x = std::max(0, static_cast<int>(std::floor(ax)));
               x < std::min(cw, static_cast<int>(std::ceil(bx))); ++x) {
            const float wx =
                std::min(bx, static_cast<float>(x + 1)) - std::max(ax, static_cast<float>(x));
            sum += texel(x, y) * wx * wy;
          }
        }
        v = sum * scale * scale;
      }
      m.alpha[static_cast<size_t>(oy) * m.w + ox] =
          static_cast<uint8_t>(std::min(255.0f, std::round(v)));
    }
  }
  return m;
}

}  // namespace

VMetrics Font::metrics(float size) const {
  if (!impl_) return {};
  if (grid()) {
    const float cell = grid_cell(impl_->grid, size);
    const float base = impl_->grid.front().baseline;
    return {base * cell, (1 - base) * cell, 0};
  }
  const float s = scale_of(impl_->info, size);
  return {static_cast<float>(impl_->ascent) * s, static_cast<float>(-impl_->descent) * s,
          static_cast<float>(impl_->line_gap) * s};
}

int Font::glyph_index(uint32_t codepoint) const {
  if (grid()) {
    const GridFace& f = impl_->grid.front();
    const auto cells = static_cast<uint32_t>(f.columns * f.rows);
    return static_cast<int>(codepoint > 0 && codepoint < cells ? codepoint : f.missing);
  }
  return impl_ ? stbtt_FindGlyphIndex(&impl_->info, static_cast<int>(codepoint)) : 0;
}

float Font::glyph_advance(int glyph, float size) const {
  if (!impl_) return 0;
  if (grid()) return impl_->grid.front().advance * grid_cell(impl_->grid, size);
  int adv = 0, lsb = 0;
  stbtt_GetGlyphHMetrics(&impl_->info, glyph, &adv, &lsb);
  return static_cast<float>(adv) * scale_of(impl_->info, size);
}

float Font::kern(int left, int right, float size) const {
  if (!impl_ || grid()) return 0;
  return static_cast<float>(stbtt_GetGlyphKernAdvance(&impl_->info, left, right)) *
         scale_of(impl_->info, size);
}

float Font::advance(std::string_view text, float size, float tracking) const {
  float pen = 0;
  int prev = -1;
  for (size_t i = 0; i < text.size();) {
    const int g = glyph_index(next_codepoint(text, i));
    if (prev >= 0) pen += kern(prev, g, size);
    pen += glyph_advance(g, size) + tracking;
    prev = g;
  }
  return pen;
}

size_t Font::fit(std::string_view text, float size, float width, float tracking) const {
  float pen = 0;
  int prev = -1;
  size_t fits = 0;
  for (size_t i = 0; i < text.size();) {
    const int g = glyph_index(next_codepoint(text, i));
    if (prev >= 0) pen += kern(prev, g, size);
    pen += glyph_advance(g, size) + tracking;
    if (pen > width) break;
    fits = i;
    prev = g;
  }
  return fits;
}

const GlyphMask& Font::mask(int glyph, float size, float phase) const {
  const int quarter = static_cast<int>(std::floor(phase * 4)) & 3;
  const auto size_key = static_cast<uint64_t>(std::lround(size * 16));
  const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(glyph)) << 32) |
                       (size_key << 2) | static_cast<uint64_t>(quarter);
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  // Drawn at the size the key holds, never the caller's: two sizes within 1/32 px of each other
  // share an entry, so the mask must not depend on which of them asked first.
  size = static_cast<float>(size_key) / 16;
  GlyphMask m;
  if (grid()) {
    m = grid_mask(impl_->grid, glyph, grid_cell(impl_->grid, size),
                  static_cast<float>(quarter) / 4);
  } else if (impl_) {
    const float s = scale_of(impl_->info, size);
    const float shift = static_cast<float>(quarter) / 4;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBoxSubpixel(&impl_->info, glyph, s, s, shift, 0, &x0, &y0, &x1, &y1);
    m.w = x1 - x0, m.h = y1 - y0, m.x0 = x0, m.y0 = y0;
    if (m.w > 0 && m.h > 0) {
      m.alpha.assign(static_cast<size_t>(m.w) * m.h, 0);
      stbtt_MakeGlyphBitmapSubpixel(&impl_->info, m.alpha.data(), m.w, m.h, m.w, s, s, shift, 0,
                                    glyph);
    } else {
      m.w = m.h = 0;
    }
  }
  return cache_.emplace(key, std::move(m)).first->second;
}

uint32_t next_codepoint(std::string_view s, size_t& i) {
  const auto b = [&](size_t k) { return static_cast<uint8_t>(s[k]); };
  const uint8_t c = b(i);
  int extra = 0;
  uint32_t cp = 0;
  if (c < 0x80) {
    i++;
    return c;
  } else if ((c & 0xe0) == 0xc0) {
    extra = 1, cp = c & 0x1fu;
  } else if ((c & 0xf0) == 0xe0) {
    extra = 2, cp = c & 0x0fu;
  } else if ((c & 0xf8) == 0xf0) {
    extra = 3, cp = c & 0x07u;
  } else {
    i++;
    return 0xfffd;
  }
  if (i + static_cast<size_t>(extra) >= s.size()) {
    i++;
    return 0xfffd;
  }
  for (int k = 1; k <= extra; k++) {
    const uint8_t cc = b(i + static_cast<size_t>(k));
    if ((cc & 0xc0) != 0x80) {
      i++;
      return 0xfffd;
    }
    cp = (cp << 6) | (cc & 0x3fu);
  }
  i += static_cast<size_t>(extra) + 1;
  return cp;
}

const Font& ui_font(Weight weight) {
  static const Font semibold = [] {
    Font f;
    f.load({ge_ui_font_semibold, ge_ui_font_semibold_size});
    return f;
  }();
  static const Font medium = [] {
    Font f;
    f.load({ge_ui_font_medium, ge_ui_font_medium_size});
    return f;
  }();
  return weight == Weight::medium ? medium : semibold;
}

}  // namespace ge::ui
