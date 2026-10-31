// The campaign's screens (level_screens.hpp).
//
// ge:summary Draws the intermission overlay, the typed finale and the solo panel
// ge:keywords draw_intermission draw_level_text draw_solo_panel figure_names big-figures
// ge:skill ui
// ge:test tests/shooters_edge_level_screens_test.cpp
#include "shooters_edge/menu/level_screens.hpp"

#include <algorithm>
#include <cmath>

#include "shooters_edge/menu/classic_view.hpp"
#include "ui/font.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr float kScreenW = 320, kScreenH = 200;
constexpr float kTitleY = 24, kInterY = 56, kFiguresX = 160;
constexpr float kRowTop = 10, kRowGap = 40;  // the figures' rows within the labels' picture
constexpr float kPanelY = 152, kPanelH = 24;
constexpr float kTextMiddle = 0.4f;  // the typed text's block, its middle down the screen
constexpr size_t kPanelChars = 40;
const ui::Color kInk{0.92f, 0.86f, 0.74f, 1};

// The classic screen's type: the glyph sheet, or the engine's font when the packs have none.
struct Type {
  const ClassicPen& pen;
  bool glyphs;

  void text(std::string_view s, float x, float y) const {
    if (glyphs) {
      pen.text(s, x, y, false);
      return;
    }
    const ClassicLayout& l = pen.l;
    pen.dl.text(ui::ui_font(), 8 * l.scale, l.x + x * l.scale, l.y + (y + 7) * l.scale, s, kInk);
  }
  // `s` centred on the screen at row `y`.
  void centred(std::string_view s, float y) const {
    if (glyphs) {
      text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), y);
      return;
    }
    const ClassicLayout& l = pen.l;
    pen.dl.text(ui::ui_font(), 8 * l.scale, l.x + kScreenW / 2 * l.scale, l.y + (y + 7) * l.scale,
                s, kInk, ui::Align::center);
  }
};

// The big figures `names` from (x, y); the x after them. A figure the packs lack is drawn in the
// engine's font in its cell.
float figures(const ClassicPen& pen, const std::vector<std::string>& names, float x, float y) {
  for (const std::string& name : names) {
    const ui::Image* img = pen.art.find(name);
    const bool narrow = name == "num_colon" || name == "num_slash";
    const float w = img != nullptr ? static_cast<float>(img->width) : narrow ? 16 : 24;
    if (img != nullptr) {
      pen.picture(img, x, y);
    } else {
      const std::string_view what = std::string_view(name).substr(4);
      const std::string c = what == "minus"   ? "-"
                            : what == "colon" ? ":"
                            : what == "slash" ? "/"
                                              : std::string(what);
      const ClassicLayout& l = pen.l;
      pen.dl.text(ui::ui_font(), 20 * l.scale, l.x + (x + w / 2) * l.scale,
                  l.y + (y + 20) * l.scale, c, kInk, ui::Align::center);
    }
    x += w;
  }
  return x;
}

std::vector<std::string> joined(std::vector<std::string> a, const char* mid,
                                const std::vector<std::string>& b) {
  a.emplace_back(mid);
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

void darken(ui::DrawList& dl) {
  dl.rect({0, 0, static_cast<float>(dl.width()), static_cast<float>(dl.height())},
          ui::hex(0x000000, 0.6f));
}

std::string two(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

}  // namespace

std::vector<std::string> figure_names(int n, int digits) {
  std::vector<std::string> out;
  if (n < 0) out.emplace_back("num_minus");
  std::string d = std::to_string(n < 0 ? -static_cast<long>(n) : n);
  if (d.size() < static_cast<size_t>(digits))
    d.insert(0, static_cast<size_t>(digits) - d.size(), '0');
  for (char c : d) out.push_back(std::string("num_") + c);
  return out;
}

void draw_intermission(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                       const LevelStats& s) {
  darken(dl);
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const Type type{pen, !art.glyphs.empty()};
  if (const ui::Image* complete = art.find("complete"))
    pen.picture(complete, std::floor((kScreenW - static_cast<float>(complete->width)) / 2),
                kTitleY);
  else
    type.centred("LEVEL COMPLETE", kTitleY + 8);
  if (const ui::Image* inter = art.find("inter")) {
    pen.picture(inter, 0, kInterY);
  } else {
    constexpr std::string_view kLabels[] = {"TIME", "SECRETS", "KILLS"};
    for (int i = 0; i < 3; i++)
      type.text(kLabels[i], 32, kInterY + kRowTop + 8 + kRowGap * static_cast<float>(i));
  }
  const float y0 = kInterY + kRowTop;
  figures(pen, joined(figure_names(s.minutes), "num_colon", figure_names(s.seconds, 2)), kFiguresX,
          y0);
  figures(pen, joined(figure_names(s.secrets), "num_slash", figure_names(s.total_secrets)),
          kFiguresX, y0 + kRowGap);
  figures(pen, joined(figure_names(s.kills), "num_slash", figure_names(s.total_kills)), kFiguresX,
          y0 + 2 * kRowGap);
}

void draw_level_text(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     std::string_view text, size_t typed) {
  darken(dl);
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const Type type{pen, !art.glyphs.empty()};
  size_t lines = 1;
  for (char c : text) lines += c == '\n' ? 1 : 0;
  const float top =
      std::max(8.0f, std::floor(kScreenH * kTextMiddle - 4 * static_cast<float>(lines)));
  size_t at = 0, left = std::min(typed, text.size());
  for (size_t row = 0; row < lines && left > 0; row++) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = text.substr(at, end - at);
    const size_t shown = std::min(left, line.size());
    // Centred by the whole line, so the text stays put while it is typed.
    const float x = std::floor((kScreenW - 8 * static_cast<float>(line.size())) / 2);
    type.text(line.substr(0, shown), x, top + 8 * static_cast<float>(row));
    left -= shown;
    if (left > 0) left--;  // the line's break
    at = end + 1;
  }
}

void draw_solo_panel(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     std::string_view title, const LevelStats& s) {
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const Type type{pen, !art.glyphs.empty()};
  dl.rect(pen.at(0, kPanelY, kScreenW, kPanelH), ui::hex(0x000000, 0.75f));
  type.centred(title.substr(0, kPanelChars), kPanelY + 4);
  const std::string row = "Kills " + std::to_string(s.kills) + "/" + std::to_string(s.total_kills) +
                          "  Secrets " + std::to_string(s.secrets) + "/" +
                          std::to_string(s.total_secrets) + "  Time " + std::to_string(s.minutes) +
                          ":" + two(s.seconds);
  type.centred(std::string_view(row).substr(0, kPanelChars), kPanelY + 13);
}

}  // namespace shooters_edge::menu
