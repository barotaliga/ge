// THE LIST PAGES IN THE CLASSIC LOOK: what DEMOS, JOIN A GAME and SELECT GAME share when the
// loaded game's menu is drawn in its own pictures (classic_view.hpp): the darkened view, the plaque
// down the left, a title (the page's picture, its word cut from the strips' letters, or the
// glyph sheet's darker set at twice its size), and rows of the glyph sheet 8 px apart in the
// column right of the plaque, cut to whole glyphs; the selected row marked by the blinking cursor,
// the keys written in the darker set at the foot. Nothing here reads the pages' models: each page
// lays out its own rows with these (demos_page.hpp, servers_page.hpp, games_page.hpp).
//
// ge:summary The classic look's list pages: plaque, title, glyph rows, cursor, footer, cutting
// ge:keywords menu classic list-page glyph-rows cursor footer fit-chars plaque title demos
// ge:keywords server-browser select-game
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "shooters_edge/menu/classic_view.hpp"

namespace shooters_edge::menu {

// The classic screen's columns for a list page, classic px.
inline constexpr float kListLeft = 64;  // the cursor's column; the text starts 8 px right of it
inline constexpr float kListRight = 312;
inline constexpr float kListTop = 32;
inline constexpr int kListChars = 30;  // glyphs from kListLeft + 8 to kListRight
inline constexpr float kFootLeft = 16;
inline constexpr int kFootChars = 37;  // glyphs from kFootLeft to kListRight, under the plaque

// `s` with every byte the glyph sheet's ASCII half cannot show written `?`.
std::string glyph_safe(std::string_view s);
// `s` cut to `n` glyphs, its end `..` when cut; `left`: its start cut instead (a path's tail).
std::string fit_chars(std::string_view s, size_t n, bool left = false);

// The darkened view, the plaque, and the page's title: the picture `title` when the art has it,
// else the strip word `carved` when it can be cut, else `words` in big glyphs, centred at the top.
// On the sequel's plates, the banner of `page` (`banner_of`) in place of `title`, the title above
// the 320 x 200 screen (classic y -16, the 240 px screen's top).
void classic_list_frame(const ClassicPen& pen, std::string_view page, std::string_view title,
                        std::string_view carved, std::string_view words);

// The blinking cursor before a row at classic y `y`.
void classic_cursor(const ClassicPen& pen, float y, double t);

}  // namespace shooters_edge::menu
