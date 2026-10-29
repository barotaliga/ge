// The charsets as grid fonts (charset_font.hpp).
//
// ge:summary Builds a charset's atlases into a grid Font on first use and keeps it
// ge:keywords ui charset font grid-font atlas build cache
// ge:skill ui
// ge:test tests/ui_charset_font_test.cpp
#include "ui/charset_font.hpp"

#include <array>
#include <memory>
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

}  // namespace ge::ui
