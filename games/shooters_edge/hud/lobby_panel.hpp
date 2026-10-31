// THE LOBBY OVER THE GAME: what the player sees of the lobby (server/lobby.hpp) while he warms up
// on the map. Drawn on the HUD's 2D layer, over the world and under the console:
//
// - THE BANNER, top centre: the phase (WARMUP, or the countdown's seconds), how many of the
//   seated players are ready, and the keys (F1 ready or break, F2 the other team, F3 the roster);
// - THE COUNT, during the countdown: MATCH STARTS IN n, large, in the middle of the screen's upper
//   half, pulsing once a second;
// - THE ROSTER (unless hidden): the two teams side by side, each player's name, a READY mark or a
//   dash, a BOT tag, a HOST tag on the leader and the player's own row lit. On a narrow screen
//   (a phone held upright) the teams stack under the banner instead.
//
// Nothing is drawn once the match is on. The panel reads a plain view of the lobby (`LobbyView`),
// so the HUD library knows nothing of the server.
//
// ge:summary The lobby over the game: the warmup banner, the countdown, the two teams' roster
// ge:keywords lobby panel warmup banner countdown roster teams red blue ready break bot leader
// ge:keywords draw_lobby lobby-view f1 f2 f3 hud
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include <string>
#include <vector>

#include "ui/draw.hpp"

namespace shooters_edge::hud {

struct LobbyRow {
  std::string name;
  int team = 0;  // 1 red, 2 blue, 0 none
  bool ready = false, bot = false, leader = false, me = false;
};

enum class LobbyShown : uint8_t { warmup, countdown, match };

struct LobbyView {
  LobbyShown phase = LobbyShown::warmup;
  int seconds_left = 0;  // the countdown's, rounded up
  double fraction = 0;   // of the second now running down, 0..1 (the count's pulse)
  bool roster = true;    // the teams shown (F3 hides them)
  std::vector<LobbyRow> rows;
};

// The banner, the count and the roster for `v` on a `w` x `h` screen.
void draw_lobby(ge::ui::DrawList& dl, const LobbyView& v, int w, int h);

}  // namespace shooters_edge::hud
