// The drop-down console as a draw list (console.hpp).
//
// ge:summary Builds the console's draw list: panel, background, lines, input, hint, list, notify
// ge:keywords ui console draw drop-down panel notify suggestion hint cursor con_clock clock
// ge:keywords con_scale foot download-bar remote-prompt wrap line-wrap long-line
// ge:skill ui console
// ge:test tests/ui_console_test.cpp
#include "ui/console.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "charset/charset.hpp"
#include "ui/background.hpp"
#include "ui/charset_font.hpp"
#include "ui/font.hpp"

namespace ge::ui {
namespace {

namespace con = ge::console;

Color color_of(con::Rgb c, float alpha) {
  return hex((static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b, alpha);
}

std::string detail_of(const con::Suggestion& s) {
  std::string out = s.kind == con::Suggestion::Kind::cvar    ? "= " + s.detail
                    : s.kind == con::Suggestion::Kind::alias ? ": " + s.detail
                                                             : s.detail;
  if (!s.choices.empty()) out += "  (" + s.choices + ")";  // the few values it takes
  if (!s.note.empty()) out += (out.empty() ? "" : "  ") + s.note;
  return out;
}

// The text that fits `width` px, cut with "..." when it does not.
std::string fitted(const Font& f, float size, const std::string& text, float width) {
  if (f.advance(text, size) <= width + 0.5f) return text;  // half a pixel of float slack
  const float dots = f.advance("...", size);
  return text.substr(0, f.fit(text, size, std::max(0.0f, width - dots))) + "...";
}

// The cursor at `x` on the row from `top` (`row` tall, the text's baseline at `base`): the font's
// box glyph when it has one (ConsoleStyle::cursor_glyph), else a thin bar.
void draw_cursor(DrawList& list, const Font& font, int glyph, float size, float x, float top,
                 float row, float base, float s, Color color) {
  if (glyph > 0) {
    list.text(font, size, x, base, std::string(1, static_cast<char>(glyph)), color);
    return;
  }
  list.rect({x, top + 2 * s, 2 * s, row - 4 * s}, color);
}

// `text` wrapped to `width` px: each row's bytes [first, second), broken after the last space that
// fits (the space dropped), or where the width ends when a word is longer than the row. One row
// when it fits or there is no width to wrap to.
std::vector<std::pair<size_t, size_t>> wrapped(const Font& f, float size, std::string_view text,
                                               float width) {
  std::vector<std::pair<size_t, size_t>> rows;
  if (width <= 0 || f.advance(text, size) <= width + 0.5f) {
    rows.emplace_back(0, text.size());
    return rows;
  }
  size_t from = 0;
  while (from < text.size()) {
    size_t n = f.fit(text.substr(from), size, width);
    if (from + n >= text.size()) {
      rows.emplace_back(from, text.size());
      break;
    }
    if (n == 0) {  // not even a letter fits: one code point a row
      n = 1;
      while (from + n < text.size() && (static_cast<unsigned char>(text[from + n]) & 0xC0) == 0x80)
        ++n;
    }
    const size_t space = text.substr(from, n + 1).rfind(' ');
    const bool at_space = space != std::string_view::npos && space > 0;
    const size_t end = at_space ? from + space : from + n;
    rows.emplace_back(from, end);
    from = at_space ? end + 1 : end;
  }
  if (rows.empty()) rows.emplace_back(0, 0);
  return rows;
}

// One row of the scrollback as drawn: a line, and the bytes of its text on this row (a line too
// long for the panel wraps onto the rows under it).
struct Row {
  const con::ViewLine* line = nullptr;
  size_t from = 0, to = 0;
};

// The scrollback's page as drawn: the font, its size, where its rows are and which piece of which
// line is on each. `draw_console` draws by it and `console_spot` reads a point by it, so a click
// lands on the letter drawn there.
struct Page {
  const Font* font = nullptr;
  float text = 0, margin = 0, row = 0, ascent = 0, top = 0, bottom = 0;
  std::vector<Row> rows;
};

Page page_of(const con::ConsoleView& view, const ConsoleStyle& style, float w, float h) {
  Page p;
  const Font* named = style.font ? nullptr : charset_font(view.font());
  p.font = style.font ? style.font : named ? named : &ui_font(Weight::medium);
  const float s = h / 720.0f * static_cast<float>(view.text_scale());  // con_scale
  p.text = style.text_px * s;
  p.margin = style.margin_px * s;
  const VMetrics m = p.font->metrics(p.text);
  p.ascent = m.ascent;
  p.row = style.row_px * s * std::max(1.0f, (m.ascent + m.descent) / p.text);
  const float cover = std::round(static_cast<float>(view.cover()) * h);
  const float input_top = cover - p.margin - p.row;
  p.bottom = view.foot().empty() ? input_top : input_top - p.row;
  const int rows = std::max(1, static_cast<int>((p.bottom - p.margin) / p.row));
  // Each line at least a row, so the page's `rows` newest lines fill it; wrapped, the oldest rows
  // that do not fit fall off the top.
  for (const con::ViewLine* l : view.page(rows)) {
    const float stamp_w = l->stamp.empty() ? 0.0f : p.font->advance(l->stamp, p.text);
    for (const auto& [from, to] : wrapped(*p.font, p.text, l->text, w - 2 * p.margin - stamp_w))
      p.rows.push_back({l, from, to});
  }
  if (p.rows.size() > static_cast<size_t>(rows)) p.rows.erase(p.rows.begin(), p.rows.end() - rows);
  p.top = p.bottom - static_cast<float>(p.rows.size()) * p.row;
  return p;
}

}  // namespace

std::optional<con::TextSpot> console_spot(const con::ConsoleView& view, const ConsoleStyle& style,
                                          int width, int height, float x, float y) {
  if (view.cover() <= 0) return std::nullopt;
  const Page p = page_of(view, style, static_cast<float>(width), static_cast<float>(height));
  if (p.rows.empty()) return std::nullopt;
  // Above the page: its first row's start; below it (the input line): its last row's end.
  const int last = static_cast<int>(p.rows.size()) - 1;
  const int i = std::clamp(static_cast<int>(std::floor((y - p.top) / p.row)), 0, last);
  const Row& r = p.rows[static_cast<size_t>(i)];
  const con::ViewLine& l = *r.line;
  if (y >= p.bottom) return con::TextSpot{l.n, r.to};
  if (y < p.top) return con::TextSpot{l.n, r.from};
  // The letter under the point: the row's text that fits up to half a letter past it.
  const float half = 0.5f * p.font->advance("M", p.text);
  const float stamp_w = l.stamp.empty() ? 0.0f : p.font->advance(l.stamp, p.text);
  const std::string_view piece = std::string_view(l.text).substr(r.from, r.to - r.from);
  const size_t col = p.font->fit(piece, p.text, std::max(0.0f, x - p.margin - stamp_w + half));
  return con::TextSpot{l.n, r.from + std::min(col, piece.size())};
}

void select_with_pointer(con::ConsoleView& view, const ConsoleStyle& style, int width, int height,
                         float x, float y, bool down, bool& held) {
  if (down) {
    if (const auto at = console_spot(view, style, width, height, x, y)) {
      if (!held) {
        if (y < static_cast<float>(view.cover()) * static_cast<float>(height))
          view.select_begin(*at);
        else
          view.select_clear();
      } else {
        view.select_to(*at);
      }
    }
  }
  held = down;
}

void draw_console(DrawList& list, const con::ConsoleView& view, const ConsoleStyle& style,
                  const Image* image, double time_s) {
  // The style's font, else con_font's charset, else the UI font.
  const Font* named = style.font ? nullptr : charset_font(view.font());
  const Font& font = style.font ? *style.font : named ? *named : ui_font(Weight::medium);
  // The cursor's box: the style's glyph, else a charset's block (the UI font has none: a bar).
  const int cursor_glyph = style.cursor_glyph > 0 ? style.cursor_glyph
                           : named != nullptr     ? static_cast<int>(charset::Sym::block)
                                                  : 0;
  const auto w = static_cast<float>(list.width());
  const auto h = static_cast<float>(list.height());
  const float s = h / 720.0f * static_cast<float>(view.text_scale());  // con_scale
  const float text = style.text_px * s, margin = style.margin_px * s;
  const VMetrics m = font.metrics(text);
  const float ascent = m.ascent;
  // A pixel charset draws taller than asked (a whole multiple of its grid): its rows grow with it.
  const float row = style.row_px * s * std::max(1.0f, (m.ascent + m.descent) / text);
  const float cover = std::round(static_cast<float>(view.cover()) * h);

  if (cover <= 0) {
    // Closed: the notify lines, each on its band.
    float y = margin;
    for (const con::ViewLine* l : view.notify()) {
      // A line too long for the screen wraps onto the rows under it, each on its band.
      for (const auto& [from, to] : wrapped(font, text, l->text, w - 2 * margin - 8 * s)) {
        const std::string piece = l->text.substr(from, to - from);
        const float tw = font.advance(piece, text);
        list.rect({margin - 4 * s, y, tw + 8 * s, row}, style.notify_band, 3 * s);
        list.text(font, text, margin, y + (row + ascent) * 0.5f - 1 * s, piece,
                  style.kinds[static_cast<size_t>(l->kind)]);
        y += row;
      }
    }
    // The chat line under them (a `messagemode`): its prompt in the route's colour, the text, the
    // cursor.
    if (view.chatting()) {
      const std::string prompt = std::string(view.chat_prompt()) + " ";
      const std::string& typed = view.chat_input();
      const float pw = font.advance(prompt, text);
      const float tw = font.advance(typed, text);
      list.rect({margin - 4 * s, y, pw + tw + 14 * s, row}, style.notify_band, 3 * s);
      const float base = y + (row + ascent) * 0.5f - 1 * s;
      const con::LineKind kind = view.chat_to() == con::ChatTo::team     ? con::LineKind::team
                                 : view.chat_to() == con::ChatTo::nearby ? con::LineKind::nearby
                                 : view.chat_to() == con::ChatTo::one    ? con::LineKind::whisper
                                                                         : con::LineKind::chat;
      list.text(font, text, margin, base, prompt, style.kinds[static_cast<size_t>(kind)]);
      list.text(font, text, margin + pw, base, typed, style.input);
      const bool lit = style.cursor_blink_s <= 0 ||
                       static_cast<long long>(std::floor(time_s / style.cursor_blink_s)) % 2 == 0;
      if (lit) {
        const float cx = margin + pw + font.advance(typed.substr(0, view.chat_cursor()), text);
        draw_cursor(list, font, cursor_glyph, text, cx, y, row, base, s, style.cursor);
      }
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
    Drift held;
    held.focus_from_y = held.focus_to_y = style.image_focus_y;
    Background pic = Background::image(*image, Fit::cover, held);
    pic.tint({1, 1, 1, alpha});
    draw_background(list, pic, panel, time_s);
  }
  if (style.edge_px > 0)
    list.rect({0, cover - style.edge_px * s, w, style.edge_px * s}, style.edge);

  // The lines, newest just over the input line, the page growing up from it.
  list.push_clip(panel);
  const float input_top = cover - margin - row;
  // The foot (what a game joining loads), on the row over the input line: a bar and its words.
  const bool footed = !view.foot().empty();
  if (footed) {
    float fx = margin;
    const auto share = static_cast<float>(std::clamp(view.foot_progress(), 0.0, 1.0));
    if (view.foot_progress() >= 0 && style.bar_glyphs) {  // the sheet's own bar and its knob
      constexpr int kMiddle = 16;
      // U+0080..U+0083 (the bar's ends, its middle, its knob) as their UTF-8 bytes: a "\u0080"
      // is read through MSVC's narrow code page, which has no such characters.
      std::string bar = "\xC2\x80";
      for (int i = 0; i < kMiddle; ++i) bar += "\xC2\x81";
      bar += "\xC2\x82";
      const float cell = font.advance("\xC2\x81", text);
      const float base = input_top - row + ascent;
      list.text(font, text, fx, base, bar, style.input);
      list.text(font, text, fx + cell * (1 + share * (kMiddle - 1)), base, "\xC2\x83", style.input);
      fx += cell * (kMiddle + 3);
    } else if (view.foot_progress() >= 0) {
      const float bw = std::min(w * 0.3f, 240 * s), bh = row * 0.5f, by = input_top - row * 0.75f;
      list.rect({fx, by, bw, bh}, style.list_fill);
      list.rect({fx, by, bw * share, bh}, style.edge);
      fx += bw + 8 * s;
    }
    list.text(font, text, fx, input_top - row + ascent, view.foot(), style.hint);
  }
  const Page page = page_of(view, style, w, h);  // the rows, a long line wrapped
  float y = page.top;
  const bool has_sel = view.has_selection();
  const con::TextSpot sel_a = view.selection_first(), sel_b = view.selection_last();
  Color stamp_colour = hex(0xaaaaaa);  // a line's time (console/view.hpp's stamps)
  colour_from_text(view.stamp_colour, stamp_colour);
  for (const Row& r : page.rows) {
    const con::ViewLine* l = r.line;
    // A stamped line: its time first, the text after it (its wrapped rows under the text).
    const float sx = l->stamp.empty() ? 0.0f : font.advance(l->stamp, text);
    if (sx > 0 && r.from == 0) list.text(font, text, margin, y + ascent, l->stamp, stamp_colour);
    const std::string_view piece = std::string_view(l->text).substr(r.from, r.to - r.from);
    if (has_sel && l->n >= sel_a.line && l->n <= sel_b.line) {
      // The selection behind its letters; a row it runs past ends a letter's width beyond.
      const size_t a = l->n == sel_a.line ? std::clamp(sel_a.column, r.from, r.to) : r.from;
      const size_t b = l->n == sel_b.line ? std::clamp(sel_b.column, r.from, r.to) : r.to;
      const float x0 = margin + sx + font.advance(piece.substr(0, a - r.from), text);
      float x1 = margin + sx + font.advance(piece.substr(0, b - r.from), text);
      if (l->n != sel_b.line || sel_b.column > r.to) x1 += 0.5f * font.advance("M", text);
      if (x1 > x0) list.rect({x0, y, x1 - x0, row}, style.selection);
    }
    list.text(font, text, margin + sx, y + ascent, std::string(piece),
              style.kinds[static_cast<size_t>(l->kind)]);
    y += row;
  }

  // The input line: the prompt and text, the hint after it, the cursor, the scroll marker.
  const std::string prompt(view.prompt());  // `] `, or `# ` in remote mode
  const std::string shown = prompt + view.input();
  const float base = input_top + ascent;
  const float typed_w = list.text(font, text, margin, base, shown, style.input);
  const std::string hint = view.hint();
  if (!hint.empty()) list.text(font, text, margin + typed_w, base, hint, style.hint);
  const bool lit = style.cursor_blink_s <= 0 ||
                   static_cast<long long>(std::floor(time_s / style.cursor_blink_s)) % 2 == 0;
  if (lit) {
    const std::string before = prompt + view.input().substr(0, view.cursor());
    const float cx = margin + font.advance(before, text);
    draw_cursor(list, font, cursor_glyph, text, cx, input_top, row, base, s, style.cursor);
  }
  // con_clock: the local time at the input line's right, the scroll marker a row above it.
  if (view.clock_shown()) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char clock[8];
    std::snprintf(clock, sizeof clock, "%02d:%02d", local.tm_hour, local.tm_min);
    list.text(font, text, w - margin, base, clock, style.hint, Align::right);
  }
  if (view.scroll() > 0)
    list.text(font, text, w - margin, view.clock_shown() ? base - row : base, "^ ^ ^", style.edge,
              Align::right);
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
  const float x =
      margin +
      font.advance(std::string(view.prompt()) +
                       view.input().substr(0, std::min(view.input().size(), view.suggest_at())),
                   text) -
      pad;
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
