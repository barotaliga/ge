// THE MENU IN THE CLASSIC LOOK: the main menu's model (main_menu.hpp) drawn the way the classic
// game drew its menu, in the pack's own pictures (classic_art.hpp), on a 320 x 200 screen scaled
// by a whole number and centred:
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

#include <string>
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
