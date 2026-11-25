// THE MENU'S AND THE LETTERS' GAME UNDER CLASSIC REMIX: `cr_menu` names the game whose menu is
// drawn (its pictures: the plaque, the titles, the strips or plates, the cursor, the console's
// background) and `cr_charset` the game whose glyph sheet writes every letter (the menu's rows,
// the console, the HUD's counts, Quake II's status bar's words), each apart from the other. Both
// are read from the remix's files (client/remix.hpp) through that game's adapter, so each game's
// pictures are read as that game reads them; with either 0, or the remix off, the played game's
// own. The menu's pages stay the played game's: only how they are drawn changes.
//
// A glyph sheet taken from a set at another density (one enhanced, `ce_pictures`, the other not)
// is scaled to the set's when it divides it, else the set keeps its own letters.
//
// ge:summary Classic remix's cr_menu and cr_charset: another game's menu pictures and letters
// ge:keywords remix classic-remix cr_menu cr_charset menu-style glyph-sheet conchars pictures
// ge:skill ui classicenhance
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/q2_bar.hpp"
#include "shooters_edge/client/remix.hpp"

namespace shooters_edge::client {

// The window's pictures: the played game's from `files` through its adapter `played`, with the
// menu's pictures and the glyph sheets taken from `remix`'s files where `remix.menu` and
// `remix.charset` name another game.
ClassicPictures remixed_pictures(const SearchPath& files, const RemixFiles& remix, bool enhance,
                                 const GameAdapter& played);

// Quake II's status bar read again from the packs now open (`pics`' cache), its words written in
// `pics`' glyph sheet while that is another game's (`letters_swapped`), else in its own `conchars`.
void bar_pictures(Q2Bar& bar, const ClassicPictures& pics);

// `into`'s glyph sheet (a set at `density`) as `sheet` (from a set at `from`), scaled to the
// density; false (and `into` kept) when the sheet is empty or its density does not divide it.
bool take_glyphs(ge::ui::Image& into, float density, const ge::ui::Image& sheet, float from);

}  // namespace shooters_edge::client
