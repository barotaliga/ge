// A frame with its notes below it (debug_sheet.hpp).
//
// ge:summary A frame with its notes set below it: the wrap, the band, the raster
// ge:keywords debug-sheet notes band wrap debug_sheet
// ge:skill ui
// ge:test tests/ui_debug_sheet_test.cpp
#include "ui/debug_sheet.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "ui/charset_font.hpp"
#include "ui/draw.hpp"
#include "ui/raster.hpp"

namespace ge::ui {

namespace {

struct Row {
  std::string_view text;
  bool heading = false;
  bool first = false;  // a line's first row (a heading's rule goes above it)
};

// `line` cut into rows no wider than `width` px: at the last space that fits, else where it must.
void wrap(const Font& font, float size, float width, std::string_view line, bool heading,
          std::vector<Row>& out) {
  bool first = true;
  do {
    size_t n = font.fit(line, size, width);
    if (n < line.size()) {
      const size_t space = line.substr(0, n + 1).rfind(' ');
      if (space != std::string_view::npos && space > 0) n = space;
      n = std::max<size_t>(n, 1);  // never a row of nothing
    }
    out.push_back({line.substr(0, n), heading, first});
    line.remove_prefix(n);
    while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
    first = false;
  } while (!line.empty());
}

}  // namespace

float sheet_text_size(int width) {
  return 16.0f * static_cast<float>(std::max(1, (width + 640) / 1280));
}

Image debug_sheet(int width, int height, std::span<const uint8_t> rgba,
                  std::span<const std::string> lines, const SheetStyle& style) {
  const Font* font = charset_font(style.font);
  if (font == nullptr) font = charset_font("mono");
  const float size = style.size > 0 ? style.size : sheet_text_size(width);
  const float margin = size;
  const float row = size * 1.25f;
  std::vector<Row> rows;
  for (const std::string& l : lines) {
    const bool heading = l.rfind("# ", 0) == 0;
    const std::string_view text = heading ? std::string_view(l).substr(2) : std::string_view(l);
    if (font != nullptr)
      wrap(*font, size, std::max(1.0f, static_cast<float>(width) - 2 * margin), text, heading,
           rows);
  }
  int headings = 0;
  for (const Row& r : rows) headings += r.heading && r.first ? 1 : 0;
  const auto band = static_cast<int>(std::ceil(2 * margin + row * static_cast<float>(rows.size()) +
                                               0.5f * row * static_cast<float>(headings)));
  Image out = make_image(width, height + band, style.band);
  const size_t picture = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  if (rgba.size() >= picture) std::memcpy(out.rgba.data(), rgba.data(), picture);
  if (font == nullptr || rows.empty()) return out;
  DrawList list;
  list.begin(out.width, out.height);
  const float ascent = size * 0.8f;
  float y = static_cast<float>(height) + margin;
  for (const Row& r : rows) {
    if (r.heading && r.first) {
      if (y > static_cast<float>(height) + margin)
        list.rect({margin, y + 0.15f * row, static_cast<float>(width) - 2 * margin, 1.0f},
                  style.rule);
      y += 0.5f * row;
    }
    list.text(*font, size, margin, y + ascent, r.text, r.heading ? style.heading : style.text);
    y += row;
  }
  render(list, out);
  return out;
}

}  // namespace ge::ui
