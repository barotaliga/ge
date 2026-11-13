// THE MENU'S CARVED LETTERS, ONE PICTURE EACH: the classic menu's strips (`mainmenu`, `sp_menu`)
// set their rows' words in carved capitals and small capitals. A row or a name the strips have no
// words for (SELECT GAME, DEMOS, the games' names on SELECT GAME) is written in those letters by
// code: when the pack loads, every letter the table knows is cut out of the strips into its own
// picture, each piece of it holding only the letter's own pixels (a speck a piece's box caught of
// a neighbour, a part under `kSpeck` pixels, is left out), and set on the strips' common line;
// `carved_text` lays a word out from them, a pixel between letters as the strips space them.
//
// The table was measured on one set of strips, named by their print (`ClassicArt::strip_print`);
// on any other set no letter is cut and the caller writes the word in the glyph sheet instead.
//
// ge:summary The menu's carved letters cut one by one from the pack's strips, and words set in them
// ge:keywords menu classic carved-letters letter-pictures strips word layout baseline select-game
// ge:keywords demos strip-print speck
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <string_view>

#include "shooters_edge/menu/classic_art.hpp"

namespace shooters_edge::menu {

// A carved word's line: 20 classic px, the small capitals from row 3 to row 15, a capital from
// row 1 (row 0 for the tall S and G), the Q's tail below.
inline constexpr float kCarvedLine = 20;

// Cuts every letter the table knows from `art`'s strips into `art->letters` (at the art's density)
// when the strips are the ones it was measured on; clears them otherwise.
void cut_letters(ClassicArt* art);

// `text`'s width in classic px set in the carved letters, a space 6 px; 0 when a letter is not cut.
float carved_text_width(const ClassicArt& art, std::string_view text);

}  // namespace shooters_edge::menu
