// THE LOBBY IN THE CLASSIC LOOK: the lobby over the game (hud/lobby_panel.hpp's `LobbyView`) drawn
// in the player's own pack's menu pictures (classic_art.hpp) when the packs carry them, on the
// classic 320 x 200 screen scaled by a whole number and centred, as the main menu is:
//
// - THE BANNER, a text box of the pack's box pieces at the top centre (y 4, 26 cells wide): the
//   phase in the sheet's darker set (WARMUP or COUNTDOWN) with how many
//   of the seated are ready, and under it the keys (F1 ready, F2 team, F3 roster);
// - THE COUNT, during the countdown: the seconds in the pack's big figures (`num_0` ..) at twice
//   their size, centred under the banner (y 36), fading once a second;
// - THE ROSTER (unless hidden), one text box across the screen (y 88): the red team in the left
//   half and the blue team in the right, each headed by its name in the darker set over a strip
//   of the team's colour; a row is the player's name (nine cells, a bot's skill dropped first), a
//   tag (H the host, B a bot)
//   and READY in the darker set or a dash. The player's own row carries the menu's arrow. The
//   rows stop above the status bar (y 176): seven a team.
//
// The client draws this when the packs' menu art is loaded and the modern panel otherwise.
//
// ge:summary The lobby drawn in the classic look: the pack's text box, glyphs and big figures
// ge:keywords lobby classic look warmup countdown roster text-box big-figures glyphs 320x200
// ge:keywords draw_lobby_classic team red blue ready host bot pack menu-art
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::menu {

// The banner, the count and the roster of `v` over a `width` x `height` frame, in `art`.
void draw_lobby_classic(ge::ui::DrawList& dl, const ClassicArt& art,
                        const shooters_edge::hud::LobbyView& v, int width, int height);

// How many rows a team's half of the roster shows before it stops above the status bar.
int lobby_classic_rows();

}  // namespace shooters_edge::menu
