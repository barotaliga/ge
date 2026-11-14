// A GAME STARTED FROM SELECT GAME, AS THE CLASSIC GAMES START: the menu gone, the console down over
// the whole screen with the loading lines (the packs added, the first recording asked for), then,
// once the game is up (its recording playing behind, or its first map loaded), the console slides
// up at the game's own speed and the game is seen.
//
// - **The speed** (`scr_conspeed`, the loaded game's name and unit for it): the classic game's is
//   lines a second of its 200-line screen (300 by default: a screen and a half a second), the
//   sequel's screens a second (3 by default). `start_apply_speed` turns it into the engine
//   console's `con_speed` (screens a second) whenever it changes; a game switched to keeps the
//   player's own speed, and the new game's default when the player kept the old one's.
// - **The hold** (`start_begin`, `start_step`): the console covers the screen at once
//   (console/view.hpp's `set_full`) and is let go once the game is up and the lines have shown for
//   `kStartHold` seconds, or after `kStartMost` whatever happens, so a game that never comes up
//   leaves no screen covered.
//
// ge:summary A game started from SELECT GAME: the console down with the loading lines, then up
// ge:keywords game-start select-game loading console scr_conspeed conspeed con_speed full slide
// ge:skill console shooter
// ge:test tests/shooters_edge_game_start_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "console/view.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::client {

inline constexpr double kStartHold = 0.75;  // seconds the loading lines show at least
inline constexpr double kStartMost = 8;     // seconds before the console is let go regardless

struct GameStart {
  double conspeed = 300;    // scr_conspeed, bound: in the game's unit (`per_screen` a screen)
  double per_screen = 200;  // the loaded game's unit: 200 lines (classic), 1 screen (sequel)
  double applied = -1;      // the value con_speed was last set from (-1: none yet)
  bool loading = false;     // the console is held over the screen
  double since = 0;         // when it was put there
};

// Registers `scr_conspeed` on `c` with `game`'s default and unit.
void add_start_cvars(ge::console::Console& c, GameStart& s, const profiles::GameProfile& game);
// The game switched from `was` to `now`: its unit, and its default when the player kept `was`'s.
void start_switch_game(ge::console::Console& c, GameStart& s, const profiles::GameProfile& was,
                       const profiles::GameProfile& now);
// con_speed from scr_conspeed, when it changed since the last call.
void start_apply_speed(ge::console::Console& c, GameStart& s);
// The console down over the whole screen at `now_s`, with `lines` printed in it.
void start_begin(GameStart& s, ge::console::ConsoleView& v, ge::console::Console& c, double now_s,
                 const std::vector<std::string>& lines);
// Lets the console go once `up` (the game is seen) after the hold, or after kStartMost; true on
// the call that lets it go.
bool start_step(GameStart& s, ge::console::ConsoleView& v, double now_s, bool up);

}  // namespace shooters_edge::client
