// The campaign's screens (level_screens.hpp).
//
// ge:summary Draws the intermission, the level's statistics, the solo scorebar, the typed finale
// ge:keywords draw_intermission draw_level_text draw_level_stats draw_solo_scorebar figure_names
// ge:keywords field_names padded classic-type big-figures
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
constexpr float kTitleX = 64, kTitleY = 24, kInterY = 56;
constexpr float kRowY[3] = {64, 104, 144};  // the time's, the secrets', the kills' figures
constexpr float kFieldX = 160, kSlashX = 232, kTotalX = 240;
constexpr float kColonX = 234, kTensX = 246, kUnitsX = 266;
constexpr float kFigureW = 24;       // a big figure's cell
constexpr float kBarY = 176;         // the status bar's row on the screen
constexpr float kTextMiddle = 0.4f;  // the typed text's block, its middle down the screen
const ui::Color kInk{0.92f, 0.86f, 0.74f, 1};
const ui::Color kGoldInk{0.86f, 0.64f, 0.32f, 1};

// The big figures `names` from (x, y); the x after them. A figure the packs lack is drawn in the
// engine's font in its cell.
float figures(const ClassicPen& pen, const std::vector<std::string>& names, float x, float y) {
  for (const std::string& name : names) {
    const ui::Image* img = pen.art.find(name);
    const bool narrow = name == "num_colon" || name == "num_slash";
    const float w = img != nullptr ? static_cast<float>(img->width) : narrow ? 16 : kFigureW;
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

// A number in a field of `digits` big figures from x.
void field(const ClassicPen& pen, int n, int digits, float x, float y) {
  int skip = 0;
  const std::vector<std::string> names = field_names(n, digits, &skip);
  figures(pen, names, x + kFigureW * static_cast<float>(skip), y);
}

void darken(ui::DrawList& dl) {
  dl.rect({0, 0, static_cast<float>(dl.width()), static_cast<float>(dl.height())},
          ui::hex(0x000000, 0.6f));
}

// The labels and the figures the intermission and the level's statistics share.
void level_figures(const ClassicPen& pen, const ClassicType& type, const LevelStats& s) {
  if (const ui::Image* inter = pen.art.find("inter")) {
    pen.picture(inter, 0, kInterY);
  } else {
    constexpr std::string_view kLabels[] = {"TIME", "SECRETS", "KILLS"};
    for (int i = 0; i < 3; i++) type.text(kLabels[i], 32, kRowY[i] + 8);
  }
  field(pen, s.minutes, 3, kFieldX, kRowY[0]);
  figures(pen, {"num_colon"}, kColonX, kRowY[0]);
  const int sec = std::clamp(s.seconds, 0, 59);
  figures(pen, {"num_" + std::to_string(sec / 10)}, kTensX, kRowY[0]);
  figures(pen, {"num_" + std::to_string(sec % 10)}, kUnitsX, kRowY[0]);
  field(pen, s.secrets, 3, kFieldX, kRowY[1]);
  figures(pen, {"num_slash"}, kSlashX, kRowY[1]);
  field(pen, s.total_secrets, 3, kTotalX, kRowY[1]);
  field(pen, s.kills, 3, kFieldX, kRowY[2]);
  figures(pen, {"num_slash"}, kSlashX, kRowY[2]);
  field(pen, s.total_kills, 3, kTotalX, kRowY[2]);
}

std::string two(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

}  // namespace

void ClassicType::text(std::string_view s, float x, float y, bool alt) const {
  if (glyphs) {
    pen.text(s, x, y, alt);
    return;
  }
  const ClassicLayout& l = pen.l;
  pen.dl.text(ui::ui_font(), 8 * l.scale, l.x + x * l.scale, l.y + (y + 7) * l.scale, s,
              alt ? kGoldInk : kInk);
}

void ClassicType::centred(std::string_view s, float y, float cx) const {
  text(s, std::floor(cx - 4 * static_cast<float>(s.size())), y);
}

void ClassicType::glyph(uint8_t code, float x, float y) const {
  if (glyphs) {
    pen.glyph(code, x, y);
    return;
  }
  // The engine's stand-ins: the brackets as brackets, a rule's pieces as a line.
  const ClassicLayout& l = pen.l;
  if (code == 16 || code == 17) {
    pen.dl.text(ui::ui_font(), 8 * l.scale, l.x + (x + 4) * l.scale, l.y + (y + 7) * l.scale,
                code == 16 ? "[" : "]", kGoldInk, ui::Align::center);
  } else if (code >= 29 && code <= 31) {
    pen.dl.rect(pen.at(x + (code == 29 ? 1.0f : 0.0f), y + 3, code == 30 ? 8.0f : 7.0f, 1),
                kGoldInk);
  }
}

std::string padded(int n, int width, int keep) {
  std::string d = std::to_string(n);
  if (d.size() < static_cast<size_t>(width))
    d.insert(0, static_cast<size_t>(width) - d.size(), ' ');
  if (keep > 0 && d.size() > static_cast<size_t>(keep)) d.resize(static_cast<size_t>(keep));
  return d;
}

std::vector<std::string> figure_names(int n, int digits) {
  std::vector<std::string> out;
  if (n < 0) out.emplace_back("num_minus");
  std::string d = std::to_string(n < 0 ? -static_cast<long>(n) : n);
  if (d.size() < static_cast<size_t>(digits))
    d.insert(0, static_cast<size_t>(digits) - d.size(), '0');
  for (char c : d) out.push_back(std::string("num_") + c);
  return out;
}

std::vector<std::string> field_names(int n, int digits, int* skip) {
  std::string d = std::to_string(n);
  if (d.size() > static_cast<size_t>(digits)) d = d.substr(d.size() - static_cast<size_t>(digits));
  *skip = digits - static_cast<int>(d.size());
  std::vector<std::string> out;
  for (char c : d) out.push_back(c == '-' ? std::string("num_minus") : std::string("num_") + c);
  return out;
}

void draw_intermission(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                       const LevelStats& s) {
  darken(dl);
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
  if (const ui::Image* complete = art.find("complete"))
    pen.picture(complete, kTitleX, kTitleY);
  else
    type.centred("LEVEL COMPLETE", kTitleY + 8);
  level_figures(pen, type, s);
}

void draw_level_stats(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                      std::string_view title, const LevelStats& s) {
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
  type.centred(title.substr(0, 40), kTitleY + 8);
  level_figures(pen, type, s);
}

void draw_solo_scorebar(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                        std::string_view title, const LevelStats& s) {
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
  if (const ui::Image* bar = art.find("scorebar"))
    pen.picture(bar, 0, kBarY);
  else
    dl.rect(pen.at(0, kBarY, kScreenW, 24), ui::hex(0x000000, 0.75f));
  type.text("Monsters:" + padded(s.kills, 3) + " /" + padded(s.total_kills, 3), 8, kBarY + 4);
  type.text("Secrets :" + padded(s.secrets, 3) + " /" + padded(s.total_secrets, 3), 8, kBarY + 12);
  type.text("Time :" + padded(s.minutes, 3) + ":" + two(std::clamp(s.seconds, 0, 59)), 184,
            kBarY + 4);
  // Centred on x 232; a title too long for the bar's right part keeps its first 22 characters.
  type.centred(title.substr(0, 22), kBarY + 12, 232);
}

void draw_level_text(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     std::string_view text, size_t typed) {
  darken(dl);
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
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

}  // namespace shooters_edge::menu
