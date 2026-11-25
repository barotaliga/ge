// QUAKE II'S PLATE LETTERS, CUT OUT AS INK: Quake II's main menu writes its words on metal tags
// (`m_main_game` .. `m_main_video`, and their lit copies `_sel`), every letter embossed in a cell
// of an even 17 px pitch (16 or 17 classic px). When the pack loads, every capital the five tags
// hold (G, A, M, E, Q, U, I, T, V, D, O, P, N, S, L, Y, R) is cut out as its own see-through
// picture: each bright or shadowed pixel of a tag belongs to the one letter it touches, a letter
// keeps only its own, and a soft rim of the tag's recess (two px whole, then fading) is grown
// around it, so no piece of a neighbour and no box of tag comes along. The nine the tags lack are
// pieced from those cut-outs before the rim is grown (B: P's bowl over D's foot; C: an O open on
// the right; F: E without its lower bar; H: an N without its diagonal, E's bar across; J: U's right
// stem and curve; K: R's stem and leg with Y's right arm; W: two V's sharing a top serif; X: Y's
// arms over the same arms upside down; Z: T's bar, Y's right arm twice, L's foot). A serif's tip
// past its cell stays its own letter's. Each small letter is its capital shrunk
// (four rows and three columns dropped where they are most like their neighbour, never a stem's
// column), its foot on the capitals' foot.
//
// The tag behind them is any width: its left end (the hole, `[`), dirty metal (`_`: GAME's banner
// between its logo and its title, a strip and its mirror), its right end (`]`). Every piece is
// shaded to GAME's tag. Nothing ships: every piece is cut from the player's own pack at load, only
// when the tags are the ones the cuts were measured on.
//
// ge:summary Quake II's tag letters cut out as see-through ink, the missing ones pieced, on metal
// ge:keywords menu q2 plates tag carved-letters ink alpha alphabet small-capitals
// ge:keywords pieced-letters plate-print lit select-game banner page-title dirty-metal
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
// A letter's picture reaches this far past its cell on each side (its rim), classic px.
inline constexpr float kInkPad = 4;
// The tag's ends ('[' and ']') and the metal strip ('_'), classic px wide; a space's cell.
inline constexpr float kTagLeft = 19, kTagRight = 13, kMetalStrip = 120, kTagSpace = 6;

// Cuts every letter, both ends and the metal from `art`'s tags into `art->letters` (and from their
// lit copies into `art->letters_lit`) when the tags are the measured ones; leaves them otherwise.
void cut_plate_letters(ClassicArt* art);

// `text` on a tag: '[' + its letters' cells + ']'; each cell's left edge (classic px) into `xs`
// when it is given, `text.size() + 2` of them (the left end first); the tag's width, 0 when a
// letter is not cut.
float plate_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs);

// A part of a picture to draw: its columns `from` .. `from + w` (classic px), all its rows, at `x`
// (classic px from the word's left), its colour times `shade`.
struct TagPiece {
  const ge::ui::Image* image = nullptr;
  float x = 0, from = 0, w = 0, shade = 1;
};

// `text` on a tag as the pieces to draw in order: the ends, the metal, then the letters over it
// (from the lit set when `lit` and it has them); the tag's width into `width`. Empty when a letter
// is not cut.
std::vector<TagPiece> tag_pieces(const ClassicArt& art, std::string_view text, bool lit,
                                 float* width, Tone tone = Tone::full);

// A PAGE'S TITLE ON A BANNER, for a page the pack has no banner for: the frame of GAME's banner
// (its hole and logo left of x 40, its maker's mark from x 236), the dirty metal between, and the
// title in tag letters centred on it; the metal at least as wide as the pack's own (196 px), wider
// for a longer title.
inline constexpr std::string_view kBannerFrame = "m_banner_game";
inline constexpr float kBannerLeft = 40, kBannerRight = 236;
// The banner's width (classic px) and each letter's cell from the banner's left into `xs` (one a
// character of `text`, a space a cell too); 0 when the frame is missing or a letter is not cut.
float banner_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs);
// The frame's brightness times this meets GAME's tag, which the letters and the metal are shaded
// to; 1 when either is missing.
float banner_shade(const ClassicArt& art);
// The banner as the pieces to draw in order (the frame's ends, the metal, the letters); its width
// into `width`. Empty when `banner_layout` is 0.
std::vector<TagPiece> banner_pieces(const ClassicArt& art, std::string_view text, float* width);

}  // namespace shooters_edge::menu
