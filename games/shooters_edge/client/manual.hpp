// THE WINDOW'S PART OF THE REFERENCE MANUAL (console/manual.hpp, `se -manual <file>`): once every
// command and setting is registered (just before the first frame), each game's own layer is loaded
// in turn as a game switch loads it (`switch_game_cvars`, `start_switch_game`) and the console
// described in that game's pass, then the loaded game's put back. So the manual lists each game's
// own settings, its spellings of ours and its defaults as the window really has them. The Quake
// pass is QuakeWorld's too: the same game plays QuakeWorld's servers and demos.
//
// ge:summary The window's part of the reference manual: every game's layer described in turn
// ge:keywords manual reference-manual user-manual manual-flag pass per-game describe write_manual
// ge:skill console shooter
// ge:test tests/shooters_edge_manual_test.cpp
#pragma once

#include <string>

#include "console/manual.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/game_start.hpp"

namespace shooters_edge::client {

// The program's name and its games, and the pass's game ids for each game ("q1,qw" for Quake).
ge::console::ManualInfo manual_info();
std::string manual_pass(const profiles::GameProfile& game);

// Every game's pass of `con` (its layer loaded in turn, `loaded`'s back after) as one source.
ge::console::ManualSource describe_games(ge::console::Console& con, GameCvarValues& v,
                                         const GameBindings& b, GameStart& start,
                                         const profiles::GameProfile& loaded);
// `-manual <file>`: describe_games written to `path`; the exit code (0 written, 1 not, said why).
int write_manual(ge::console::Console& con, GameCvarValues& v, const GameBindings& b,
                 GameStart& start, const profiles::GameProfile& loaded, const std::string& path);

}  // namespace shooters_edge::client
