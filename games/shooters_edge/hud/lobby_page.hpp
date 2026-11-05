// THE LOBBY'S SCREEN: the multiplayer lobby as a screen of its own, over the main menu's backdrop
// (the title's night, client/title.hpp), never over the game. The player is on it from the moment
// a match's map opens until he joins the WARMUP (F4), and again when he leaves it (F4 on the map)
// or the match ends; the bots and whoever joined the warmup fight on meanwhile.
//
// What is drawn, on a `w` x `h` screen (laid out for 1280 x 720 and scaled; on a narrow screen,
// a phone held upright, the teams stack):
//
// - THE HEAD: MULTIPLAYER LOBBY, the map, and the phase: WARMUP with how many are ready, or
//   MATCH STARTS IN n, the seconds large and pulsing once a second;
// - THE TEAMS, red and blue side by side: each player's name, a HOST tag on the leader and a BOT
//   tag on a bot, where he is (IN WARMUP, lit, or IN LOBBY), his frags so far and READY or a dash;
//   the player's own row lit. The team's header sums its frags;
// - THE NOTES: how many seats the match needs when too few are seated, and that the warmup's frags
//   are reset when the match starts;
// - THE KEYS along the foot: F4 join the warmup, F1 ready or break, F2 switch team, F5 add a bot,
//   F6 remove one, ENTER start the match (the leader's).
//
// The page reads the lobby's plain view (lobby_panel.hpp's `LobbyView`), as the banner on the map
// does, so the HUD library knows nothing of the server. The classic look's page is
// menu/lobby_screen.hpp's.
//
// ge:summary The lobby's screen: the teams, where each player is, frags and ready, the keys
// ge:keywords lobby lobby-screen lobby-page full-screen warmup countdown teams roster frags ready
// ge:keywords in-warmup in-lobby host bot f1 f2 f4 f5 f6 enter draw_lobby_page main-menu-backdrop
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include "shooters_edge/hud/lobby_panel.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::hud {

// The lobby's screen for `v` on a `w` x `h` screen, over whatever is drawn under it (the
// backdrop). Nothing once the match is on.
void draw_lobby_page(ge::ui::DrawList& dl, const LobbyView& v, int w, int h);

}  // namespace shooters_edge::hud
