// A TrueType font over stb_truetype, defined: loading, metrics, the advance with kerning, the
// glyph masks cached by glyph, size and quarter-pixel phase, UTF-8 decoding, and the compiled-in
// family. This is the only file that includes stb_truetype, and it compiles the implementation
// with the engine's libm in place of the platform's, so a glyph's coverage is the same bits on
// every row and a lab picture's digest with it.
//
// ge:summary The TrueType font over stb_truetype: glyph cache, kerning, UTF-8, built-in fonts
// ge:keywords ui font stb_truetype glyph cache kerning utf8 codepoint embed barlow
// ge:skill ui
// ge:test tests/ui_font_test.cpp
#include "ui/font.hpp"

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

namespace {

float scale_of(const stbtt_fontinfo& info, float size) {
  return stbtt_ScaleForPixelHeight(&info, size);
}

}  // namespace

VMetrics Font::metrics(float size) const {
  if (!impl_) return {};
  const float s = scale_of(impl_->info, size);
  return {static_cast<float>(impl_->ascent) * s, static_cast<float>(-impl_->descent) * s,
          static_cast<float>(impl_->line_gap) * s};
}

int Font::glyph_index(uint32_t codepoint) const {
  return impl_ ? stbtt_FindGlyphIndex(&impl_->info, static_cast<int>(codepoint)) : 0;
}

float Font::glyph_advance(int glyph, float size) const {
  if (!impl_) return 0;
  int adv = 0, lsb = 0;
  stbtt_GetGlyphHMetrics(&impl_->info, glyph, &adv, &lsb);
  return static_cast<float>(adv) * scale_of(impl_->info, size);
}

float Font::kern(int left, int right, float size) const {
  if (!impl_) return 0;
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
  GlyphMask m;
  if (impl_) {
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
