// THE MENU IN THE CLASSIC LOOK: the main menu's model (main_menu.hpp) drawn the way the classic
// game drew its menu, in the pack's own pictures (classic_art.hpp), on a 320 x 200 screen scaled
// by a whole number and centred (an enhanced set, `ClassicArt::density` above 1, drawn at the same
// classic sizes from its finer pixels, filtered):
//
// - the plaque down the left at (16, 4) and the page's title picture centred at the top;
// - the root, SINGLE PLAYER and MULTIPLAYER as the pack's strips of 20 px rows at (72, 32), the
//   spinning cursor (six frames, ten a second) at (54, 32 + 20 x the row), a dimmed row darkened;
// - every other page as rows of the glyph sheet 8 px apart from y 32: the label in the sheet's
//   darker set ending at x 192, the blinking cursor at x 200, the value at x 220 (a toggle's on or
//   off, a choice's words, a slider's track and handle), a dimmed row faded;
// - HELP as the pack's help pages, one at a time (left and right turn them), and QUIT as its
//   question in the text box made of the box pieces.
//
// Pages with no picture of their own use the glyphs for their title; the classic-enhance pages
// keep the engine's modern view (game_menu.hpp), since their progress panel is drawn for it.
//
// ge:summary The main menu drawn in the classic look: the pack's plaque, titles, strips, cursor
// ge:keywords menu classic look draw layout 320x200 scale plaque title strip cursor glyphs slider
// ge:keywords help pages text-box dimmed pointer hit shooters-edge
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"
#include "ui/menu.hpp"

namespace shooters_edge::menu {

struct ClassicLayout {
  float scale = 1;                  // screen px a classic pixel
  float x = 0, y = 0;               // the 320 x 200 screen's top left, px
  bool strip = false;               // the page is drawn as the pack's strip of rows
  std::vector<ge::ui::Rect> items;  // index for index with the page's items; empty: not drawn
};

// Draws on the classic screen of a layout: pictures and the glyph sheet's text at its scale.
struct ClassicPen {
  ge::ui::DrawList& dl;
  const ClassicArt& art;
  const ClassicLayout& l;

  ge::ui::Rect at(float x, float y, float w, float h) const {
    return {l.x + x * l.scale, l.y + y * l.scale, w * l.scale, h * l.scale};
  }
  void picture(const ge::ui::Image* img, float x, float y, float opacity = 1) const {
    if (img == nullptr) return;
    ge::ui::Box b;
    b.rect = at(x, y, art.width_of(*img), art.height_of(*img));
    b.image = img;
    b.uv = {0, 0, static_cast<float>(img->width), static_cast<float>(img->height)};
    b.nearest = art.density <= 1;  // the pack's own pixels stay crisp; enhanced ones filter
    b.feather = 0;
    b.opacity = opacity;
    dl.box(b);
  }
  void glyph(uint8_t code, float x, float y, float opacity = 1) const {
    if (code == ' ' || code == 0) return;
    ge::ui::Box b;
    b.rect = at(x, y, 8, 8);
    b.image = &art.glyphs;
    const float cell = 8 * art.density;
    b.uv = {static_cast<float>(code & 15) * cell, static_cast<float>(code >> 4) * cell, cell, cell};
    b.nearest = art.density <= 1;
    b.feather = 0;
    b.opacity = opacity;
    dl.box(b);
  }
  // `alt`: the sheet's second set (codes + 128), the classic menu's labels.
  void text(std::string_view s, float x, float y, bool alt, float opacity = 1) const {
    for (size_t i = 0; i < s.size(); i++) {
      const auto c = static_cast<uint8_t>(s[i]);
      glyph(static_cast<uint8_t>(alt ? (c | 0x80) : c), x + 8 * static_cast<float>(i), y, opacity);
    }
  }
  // The text box: `cols` x `rows` cells of 8 px inside its pieces' frame, its top left at (x, y).
  void box(float x, float y, int cols, int rows) const {
    const auto piece = [&](const char* name) { return art.find(name); };
    const ge::ui::Image* tl = piece("box_tl");
    if (tl == nullptr) {
      ge::ui::Box b;
      b.rect = at(x, y, 8 * static_cast<float>(cols + 2), 8 * static_cast<float>(rows + 2));
      b.fill = ge::ui::solid(ge::ui::hex(0x000000, 0.8f));
      dl.box(b);
      return;
    }
    const char* top[3] = {"box_tl", "box_tm", "box_tr"};
    const char* mid[3] = {"box_ml", "box_mm", "box_mr"};
    const char* bot[3] = {"box_bl", "box_bm", "box_br"};
    for (int r = -1; r <= rows; r++) {
      const char** row = r < 0 ? top : r == rows ? bot : mid;
      const float py = y + 8 * static_cast<float>(r + 1);
      picture(piece(row[0]), x, py);
      for (int c = 0; c < cols; c += 2)
        picture(piece(row[1]), x + 8 + 8 * static_cast<float>(c), py);
      picture(piece(row[2]), x + 8 + 8 * static_cast<float>(cols + (cols & 1)), py);
    }
  }
};

// The bare classic screen (no page) on a `width` x `height` screen: scaled and centred.
ClassicLayout classic_screen(int width, int height);

// The page up laid out on a `width` x `height` screen.
ClassicLayout layout_classic(const ge::ui::Menu& menu, const ClassicArt& art, int width,
                             int height);

// What a reader of the picture would flag: a row off the screen, a label or a value wider than its
// column. Empty when it reads.
std::vector<std::string> classic_problems(const ge::ui::Menu& menu, const ClassicLayout& layout,
                                          int width, int height);

// The page drawn over a darkened game: `t` the seconds the cursor turns by, `help_page` the help
// picture shown.
void draw_classic(ge::ui::DrawList& list, const ge::ui::Menu& menu, const ClassicArt& art,
                  const ClassicLayout& layout, double t, int help_page);

// How many help pictures the art has (0: HELP is drawn as words).
int help_pages(const ClassicArt& art);

}  // namespace shooters_edge::menu
