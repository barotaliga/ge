// THE SEQUEL'S PLATE LETTERS, ONE CELL EACH: the sequel's main menu writes its words on metal tags
// (`m_main_game` .. `m_main_video`, and their lit copies `_sel`), every letter embossed in a cell
// of an even 17 px pitch (16 or 17 classic px) the tag's whole 32 px tall. When the pack loads,
// every capital the five tags hold is cut out as its cell (G, A, M, E, Q, U, I, T, V, D, O, P, N,
// S, L, Y, R), and the nine they lack are pieced from them over plain tag (B: P's bowl over D's
// foot; C: an O with its right side hidden; F: E over P's foot; H: an N with its diagonal hidden
// and T's bar across; J: a U without its left stem; K: an R with Y's arm; W: two narrowed V's; X:
// Y's arms over A's legs; Z: T's bar, A's leg slanted, L's foot). Each small letter is its capital
// shrunk (four of its rows and three of its columns dropped where they are most like their
// neighbour, plain tag added above), its foot on the capitals' foot, its cell 3 px narrower. The
// tags' surfaces differ by a tenth in brightness, so every piece is shaded to the darkest tag's
// surface and no cell shows its edge.
//
// Besides the letters: '[' the tag's left end (its hole), ']' its right end, ' ' a space (6 px of
// plain tag: the bare column after OPTIONS' S, which every pieced cell is laid over too). A word is
// drawn '[' + its letters + ']'. Nothing ships: every cell is cut from the player's own pack at
// load, only when the tags are the ones the cells were measured on.
//
// ge:summary The sequel's tag letters cut a cell each, the missing ones pieced, small ones shrunk
// ge:keywords menu sequel plates tag carved-letters cells alphabet small-capitals pieced-letters
// ge:keywords plate-print lit select-game
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"

namespace shooters_edge::menu {

// The five tags the cells were measured on (`ClassicArt::strip_print` over their palette indices).
inline constexpr uint64_t kPlatePrint = 0xfd50f20825e19f40ULL;
// A tag's height, classic px.
inline constexpr float kPlateLine = 32;

// Cuts every letter, both ends and the space from `art`'s tags into `art->letters` (and from their
// lit copies into `art->letters_lit`) when the tags are the measured ones; leaves them otherwise.
void cut_plate_letters(ClassicArt* art);

// `text` on a tag: '[' + its letters + ']' laid out cell after cell; each piece's left edge
// (classic px) into `xs` when it is given, `text.size() + 2` of them (the left end first); the
// tag's width, 0 when a letter is not cut.
float plate_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs);

}  // namespace shooters_edge::menu
