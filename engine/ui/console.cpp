// The drop-down console as a draw list (console.hpp).
//
// ge:summary Builds the console's draw list: panel, background, lines, input, hint, list, notify
// ge:keywords ui console draw drop-down panel notify suggestion hint cursor
// ge:skill ui console
// ge:test tests/ui_console_test.cpp
#include "ui/console.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "ui/background.hpp"
#include "ui/font.hpp"

namespace ge::ui {
namespace {

namespace con = ge::console;

Color color_of(con::Rgb c, float alpha) {
  return hex((static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b, alpha);
}

std::string detail_of(const con::Suggestion& s) {
  if (s.kind == con::Suggestion::Kind::cvar) return "= " + s.detail;
  if (s.kind == con::Suggestion::Kind::alias) return ": " + s.detail;
  return s.detail;
}

// The text that fits `width` px, cut with "..." when it does not.
std::string fitted(const Font& f, float size, const std::string& text, float width) {
  if (f.advance(text, size) <= width + 0.5f) return text;  // half a pixel of float slack
  const float dots = f.advance("...", size);
  return text.substr(0, f.fit(text, size, std::max(0.0f, width - dots))) + "...";
}

}  // namespace

void draw_console(DrawList& list, const con::ConsoleView& view, const ConsoleStyle& style,
                  const Image* image, double time_s) {
  const Font& font = style.font ? *style.font : ui_font(Weight::medium);
  const auto w = static_cast<float>(list.width());
  const auto h = static_cast<float>(list.height());
  const float s = h / 720.0f;
  const float text = style.text_px * s, row = style.row_px * s, margin = style.margin_px * s;
  const float ascent = font.metrics(text).ascent;
  const float cover = std::round(static_cast<float>(view.cover()) * h);

  if (cover <= 0) {
    // Closed: the notify lines, each on its band.
    float y = margin;
    for (const con::ViewLine* l : view.notify()) {
      const float tw = font.advance(l->text, text);
      list.rect({margin - 4 * s, y, tw + 8 * s, row}, style.notify_band, 3 * s);
      list.text(font, text, margin, y + (row + ascent) * 0.5f - 1 * s, l->text,
                style.kinds[static_cast<size_t>(l->kind)]);
      y += row;
    }
    return;
  }

  // The panel: the con_bg* gradient at con_bgalpha, the image over it, the edge under it.
  const con::Background bg = view.background();
  const auto alpha = static_cast<float>(bg.alpha);
  const Rect panel{0, 0, w, cover};
  draw_background(list,
                  Background::gradient(180, color_of(bg.top, alpha), color_of(bg.bottom, alpha)),
                  panel, time_s);
  if (image != nullptr && !image->empty()) {
    Background pic = Background::image(*image);
    pic.tint({1, 1, 1, alpha});
    draw_background(list, pic, panel, time_s);
  }
  list.rect({0, cover - style.edge_px * s, w, style.edge_px * s}, style.edge);

  // The lines, newest just over the input line, the page growing up from it.
  list.push_clip(panel);
  const float input_top = cover - margin - row;
  const int rows = std::max(1, static_cast<int>((input_top - margin) / row));
  const auto page = view.page(rows);
  float y = input_top - static_cast<float>(page.size()) * row;
  for (const con::ViewLine* l : page) {
    list.text(font, text, margin, y + ascent, l->text, style.kinds[static_cast<size_t>(l->kind)]);
    y += row;
  }

  // The input line: the prompt and text, the hint after it, the cursor, the scroll marker.
  const std::string shown = "] " + view.input();
  const float base = input_top + ascent;
  const float typed_w = list.text(font, text, margin, base, shown, style.input);
  const std::string hint = view.hint();
  if (!hint.empty()) list.text(font, text, margin + typed_w, base, hint, style.hint);
  const bool lit = style.cursor_blink_s <= 0 ||
                   static_cast<long long>(std::floor(time_s / style.cursor_blink_s)) % 2 == 0;
  if (lit) {
    const std::string before = "] " + view.input().substr(0, view.cursor());
    const float cx = margin + font.advance(before, text);
    list.rect({cx, input_top + 2 * s, 2 * s, row - 4 * s}, style.cursor);
  }
  if (view.scroll() > 0) list.text(font, text, w - margin, base, "^ ^ ^", style.edge, Align::right);
  list.pop_clip();

  // The suggestions, under the panel's edge from where the name starts.
  const std::vector<con::Suggestion> suggestions = view.suggestions();
  if (suggestions.empty()) return;
  float name_w = 0, detail_w = 0;
  for (const con::Suggestion& sg : suggestions) {
    name_w = std::max(name_w, font.advance(sg.name, text));
    detail_w = std::max(detail_w, font.advance(detail_of(sg), text));
  }
  const float pad = 6 * s, gap = 24 * s;
  const float x = margin + font.advance(view.input().starts_with("/") ? "] /" : "] ", text) - pad;
  const float box_w = std::min(w - x - margin, name_w + gap + detail_w + 2 * pad);
  const float top = cover + 2 * s;
  list.rect({x, top, box_w, static_cast<float>(suggestions.size()) * row + 2 * pad},
            style.list_fill, 3 * s);
  float ry = top + pad;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    const con::Suggestion& sg = suggestions[i];
    const bool selected = static_cast<int>(i) == view.selected();
    if (selected) list.rect({x, ry, box_w, row}, style.list_lit);
    const float rb = ry + (row + ascent) * 0.5f - 1 * s;
    list.text(font, text, x + pad, rb, sg.name, selected ? style.input : style.list_name);
    const float dx = x + pad + name_w + gap;
    list.text(font, text, dx, rb, fitted(font, text, detail_of(sg), x + box_w - pad - dx),
              style.list_detail);
    ry += row;
  }
}

}  // namespace ge::ui
