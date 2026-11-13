// The charsets as grid fonts (charset_font.hpp).
//
// ge:summary Builds a charset's atlases into a grid Font on first use and keeps it
// ge:keywords ui charset font grid-font atlas build cache glyph-sheet sheet_font
// ge:skill ui
// ge:test tests/ui_charset_font_test.cpp
#include "ui/charset_font.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "charset/charset.hpp"

namespace ge::ui {

const Font* charset_font(std::string_view name) {
  namespace cs = ge::charset;
  const cs::FontInfo* info = cs::find_font(name);
  if (info == nullptr) return nullptr;
  static std::array<std::unique_ptr<Font>, cs::kFontCount> built;
  std::unique_ptr<Font>& font = built[static_cast<size_t>(info->id)];
  if (font) return font.get();
  std::vector<GridFace> faces;
  for (const int size : cs::kAtlasSizes) {
    cs::Atlas atlas = cs::build_atlas(info->id, size);
    faces.push_back({.size = size,
                     .columns = cs::kColumns,
                     .rows = cs::kRows,
                     .coverage = std::move(atlas.coverage),
                     .baseline = static_cast<float>(info->metrics.baseline),
                     .advance = 0.5f,
                     .missing = cs::kMissing,
                     .pixel_h = info->pixel ? info->grid_h : 0});
  }
  font = std::make_unique<Font>();
  font->load_grid(std::move(faces));
  return font.get();
}

bool sheet_font(const Image& sheet, Font& out, std::string* why) {
  constexpr int kCells = 16;
  if (sheet.empty() || sheet.width != sheet.height || sheet.width % kCells != 0) {
    if (why != nullptr)
      *why = "the glyph sheet is " + std::to_string(sheet.width) + " x " +
             std::to_string(sheet.height) + " px, not square in 16 x 16 whole cells";
    return false;
  }
  const int size = sheet.width;
  const size_t n = static_cast<size_t>(size) * static_cast<size_t>(size);
  std::vector<uint8_t> coverage(n);
  // Each pixel's brightness, the brightest pixel of the sheet full cover.
  int top = 1;
  for (size_t i = 0; i < n; i++) {
    const uint8_t* px = &sheet.rgba[i * 4];
    const int v = px[3] == 0 ? 0 : std::max({px[0], px[1], px[2]});
    coverage[i] = static_cast<uint8_t>(v);
    top = std::max(top, v);
  }
  for (uint8_t& c : coverage) c = static_cast<uint8_t>((c * 255 + top / 2) / top);
  std::vector<GridFace> faces;
  faces.push_back({.size = size,
                   .columns = kCells,
                   .rows = kCells,
                   .coverage = std::move(coverage),
                   .baseline = 0.875f,
                   .advance = 1.0f,
                   .missing = '?',
                   .pixel_h = size / kCells});
  return out.load_grid(std::move(faces), why);
}

}  // namespace ge::ui
