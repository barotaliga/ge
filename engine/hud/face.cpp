// The HUD's type defined: the charset runs as image boxes over atlases built on first use, and
// the TrueType runs handed to the draw list as they are.
//
// ge:summary The HUD's type defined: charset runs as atlas boxes, the atlas cache
// ge:keywords hud face charset atlas-cache image-box glyph build_atlas
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#include "hud/face.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace ge::hud {

namespace {

// The atlas a run `cell_h` px tall is drawn from: built at the multiple of 64 px whose cells are
// at least that tall (so a cell is shrunk by less than 1.5x and a bilinear sample does not
// alias), from 64 to 1024.
int atlas_size_for_cell(float cell_h) {
  int size = static_cast<int>((cell_h * charset::kRows + 63) / 64) * 64;
  return std::clamp(size, 64, 1024);
}

float charset_advance(float size, float tracking) { return size / 2 + tracking; }

}  // namespace

const ui::Image& atlas_image(charset::FontId font, int size) {
  // The frame's thread alone fills it (header).
  static std::map<std::pair<int, int>, ui::Image> cache;
  const std::pair<int, int> key{static_cast<int>(font), size};
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  const charset::Atlas atlas = charset::build_atlas(font, size);
  ui::Image img;
  img.width = img.height = size;
  img.rgba.assign(static_cast<size_t>(size) * size * 4, 255);
  for (size_t i = 0; i < atlas.coverage.size(); i++) img.rgba[i * 4 + 3] = atlas.coverage[i];
  return cache.emplace(key, std::move(img)).first->second;
}

float ascent(const Face& face, float size) {
  if (face.font != nullptr) return face.font->metrics(size).ascent;
  return static_cast<float>(charset::kBaselineFromTop) * size;
}

float descent(const Face& face, float size) {
  if (face.font != nullptr) return face.font->metrics(size).descent;
  return static_cast<float>(1 - charset::kBaselineFromTop) * size;
}

float text_width(const Face& face, float size, std::string_view text, float tracking) {
  if (face.font != nullptr) return face.font->advance(text, size, tracking);
  return static_cast<float>(text.size()) * charset_advance(size, tracking);
}

void draw_symbol(ui::DrawList& dl, charset::FontId font, uint8_t code, ui::Rect cell,
                 ui::Color color) {
  const int size = atlas_size_for_cell(cell.h);
  const charset::CellRect c = charset::cell_rect(size, code);
  ui::Box b;
  b.rect = cell;
  b.image = &atlas_image(font, size);
  b.uv = {static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.w),
          static_cast<float>(c.h)};
  b.fill = ui::solid(color);
  b.feather = 0.5f;
  dl.box(b);
}

float draw_text(ui::DrawList& dl, const Face& face, float size, float x, float baseline,
                std::string_view text, ui::Color color, ui::Align align, float tracking) {
  if (face.font != nullptr)
    return dl.text(*face.font, size, x, baseline, text, color, align, tracking);
  const float w = text_width(face, size, text, tracking);
  if (align == ui::Align::center) x -= w / 2;
  if (align == ui::Align::right) x -= w;
  const float top = baseline - ascent(face, size);
  const float step = charset_advance(size, tracking);
  for (size_t i = 0; i < text.size(); i++) {
    const auto ch = static_cast<unsigned char>(text[i]);
    if (ch == ' ') continue;
    draw_symbol(dl, face.charset, charset::code_of(ch),
                {x + static_cast<float>(i) * step, top, size / 2, size}, color);
  }
  return w;
}

float draw_shadowed(ui::DrawList& dl, const Face& face, float size, float x, float baseline,
                    std::string_view text, ui::Color color, ui::Color shadow, float drop,
                    ui::Align align, float tracking) {
  if (shadow.a > 0 && drop > 0) {
    // An outline of four offsets a little over half the drop, then the drop itself: the outline
    // carries the words over white snow, the drop gives them weight over anything.
    const float o = drop * 0.6f;
    const ui::Color rim = ui::with_alpha(shadow, shadow.a * 0.8f);
    for (const auto& [dx, dy] : {std::pair{-o, 0.0f}, {o, 0.0f}, {0.0f, -o}, {0.0f, o}})
      draw_text(dl, face, size, x + dx, baseline + dy, text, rim, align, tracking);
    draw_text(dl, face, size, x, baseline + drop, text, shadow, align, tracking);
  }
  return draw_text(dl, face, size, x, baseline, text, color, align, tracking);
}

}  // namespace ge::hud
