// THE LOBBY IN THE CLASSIC LOOK: the lobby (hud/lobby_panel.hpp's `LobbyView`) drawn in the
// player's own pack's menu pictures (classic_art.hpp) when the packs carry them, on the classic
// 320 x 200 screen scaled by a whole number and centred, as the main menu is. Two pictures:
//
// THE BANNER ON THE MAP (`draw_lobby_classic`), while the player warms up:
// - a text box of the pack's box pieces at the top centre (y 4, 26 cells wide): the phase in the
//   sheet's darker set (WARMUP or COUNTDOWN) with how many of the seated are ready, and under it
//   the keys (F1 ready, F4 the lobby);
// - THE COUNT, during the countdown: the seconds in the pack's big figures (`num_0` ..) at twice
//   their size, centred under the banner (y 36), fading once a second.
//
// THE LOBBY'S SCREEN (`draw_lobby_page_classic`), over the main menu's backdrop, as
// hud/lobby_page.hpp's in the modern look:
// - the head box (y 4): MULTIPLAYER LOBBY and the map in the darker set; under it WARMUP and the
//   ready count, or MATCH STARTS IN n;
// - the roster box (y 40, as tall as its rows): the red team, then the blue, each headed by its
// name in the darker set
//   over a strip of the team's colour with its frags summed; a row is the player's name (twelve
//   cells, a bot's skill dropped first), a tag (H the host, B a bot), WARM in the darker set or
//   LOBBY, the frags, and READY or a dash; the player's own row carries the menu's arrow. Five rows
//   a team fit;
// - the keys' box under the roster (at most y 168): F4 warmup, F1 ready, F2 team; F5 and F6 a bot
// more or less,
//   ENTER start; or how many more players the match needs.
//
// The client draws these when the packs' menu art is loaded and the modern ones otherwise.
//
// ge:summary The lobby drawn in the classic look: the banner on the map and the lobby's screen
// ge:keywords lobby classic look warmup countdown roster text-box big-figures glyphs 320x200
// ge:keywords draw_lobby_classic draw_lobby_page_classic lobby-screen team red blue ready host bot
// ge:keywords pack menu-art frags
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::menu {

// The banner and the count of `v` over a `width` x `height` frame, in `art`.
void draw_lobby_classic(ge::ui::DrawList& dl, const ClassicArt& art,
                        const shooters_edge::hud::LobbyView& v, int width, int height);

// The lobby's screen of `v` over a `width` x `height` frame, in `art`.
void draw_lobby_page_classic(ge::ui::DrawList& dl, const ClassicArt& art,
                             const shooters_edge::hud::LobbyView& v, int width, int height);

// How many rows each team's part of the screen's roster shows.
int lobby_classic_rows();

}  // namespace shooters_edge::menu
