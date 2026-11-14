// THE MENU'S CARVED LETTERS, ONE PICTURE EACH: the classic menu's strips (`mainmenu`, `sp_menu`)
// set their rows' words in carved capitals and small capitals. A row or a name the strips have no
// words for (SELECT GAME, DEMOS, the games' names on SELECT GAME) is written in those letters by
// code: when the pack loads, every letter the table knows is cut out of the strips into its own
// picture, each piece of it holding only the letter's own pixels (a speck a piece's box caught of
// a neighbour, a part under `kSpeck` pixels, is left out), and set on the strips' common line; a
// letter the strips hold in one size only is made in the other by doubling or dropping its
// straightest rows and columns (the A from the a, the h from the H), the F from the E and the I;
// and the three in neither size are pieced from others (the B an I and two half O's, the X two
// leaned I's crossed, the Z a leaned I under the E's bars), their small capitals shrunk from them;
// `carved_layout` lays a word out from them, spaced as the strips space theirs.
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
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"

namespace shooters_edge::menu {

// A carved word's line: 24 classic px, measured on the strips: the small capitals on rows 3 to 15,
// a capital on rows 1 to 16, and below the line the t's stem (to 18), the G's tail (to 22) and the
// Q's (to 23). A row is 20 px; the tails hang into the next row's empty top.
inline constexpr float kCarvedLine = 24;

// Cuts every letter the table knows from `art`'s strips into `art->letters` (at the art's density)
// and makes the rest of A to Z and a to z from them when the strips are the ones it was
// measured on; clears them otherwise.
void cut_letters(ClassicArt* art);

// `text` set in the carved letters: each letter's left edge (classic px from the word's) into
// `xs` when it is given, by the strips' measured spacing (a pixel between letters, a space 5 px
// before a round capital and 10 before a stem); the word's width, 0 when a letter is not cut.
float carved_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs);
float carved_text_width(const ClassicArt& art, std::string_view text);

}  // namespace shooters_edge::menu
