// THE LOBBY OVER THE GAME: what the player sees of the lobby (server/lobby.hpp) while he warms up
// on the map. The lobby itself is a screen of its own (lobby_page.hpp); on the map only a banner
// is left, on the HUD's 2D layer, over the world and under the console:
//
// - THE BANNER, top centre: the phase (WARMUP, or the countdown's seconds), how many of the
//   seated players are ready, and the keys (F1 ready or break, F4 back to the lobby);
// - THE COUNT, during the countdown: MATCH STARTS IN n, large, in the middle of the screen's upper
//   half, pulsing once a second.
//
// Nothing is drawn once the match is on. The panel reads a plain view of the lobby (`LobbyView`),
// so the HUD library knows nothing of the server; the lobby's screen reads the same view.
//
// ge:summary The lobby over the game: the warmup banner and the countdown; the lobby's view
// ge:keywords lobby panel warmup banner countdown ready break bot leader lobby-view frags
// ge:keywords draw_lobby f1 f4 hud
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
  bool warm = false;  // in the warmup, on the map (else on the lobby's screen)
  int frags = 0;      // scored on the map so far (the warmup's are reset when the match starts)
};

enum class LobbyShown : uint8_t { warmup, countdown, match };

struct LobbyView {
  LobbyShown phase = LobbyShown::warmup;
  int seconds_left = 0;  // the countdown's, rounded up
  double fraction = 0;   // of the second now running down, 0..1 (the count's pulse)
  std::string map;       // the map the warmup plays
  int min_players = 1;   // seats needed before the match may start
  std::vector<LobbyRow> rows;
};

// The banner and the count for `v` on a `w` x `h` screen.
void draw_lobby(ge::ui::DrawList& dl, const LobbyView& v, int w, int h);

}  // namespace shooters_edge::hud
