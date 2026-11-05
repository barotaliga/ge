// THE LOBBY'S CONSOLE: the words a player types (or a key runs) to use the lobby on the listen
// server, and the settings that shape it, registered on the window's console.
//
// THE COMMANDS (each runs `listen_lobby` for the player):
//   ready                 say you are ready; the countdown starts when everyone is
//   break                 take it back (also `notready`); a running countdown stops
//   toggleready           ready or break, whichever you are not (F1)
//   team <red|blue|auto|switch>   change team (`team` alone says yours; `team switch` is F2)
//   start                 the leader starts the countdown whoever is ready (never with fewer
//                         than sv_minplayers seated)
//   warmup                leave the lobby's screen for the warmup on the map
//   sitout                leave the warmup for the lobby's screen
//   togglewarmup          whichever of the two you are not on (F4)
//   lobby                 print the roster
//   addbot [novice|average|expert]   seat a bot (in warmup)
//   removebot             free the highest bot seat
//
// THE SETTINGS (read when a map starts):
//   sv_lobby 1|0          a match opens every map in warmup (1, the default), or starts at once
//   sv_countdown <s>      the countdown's length (5)
//   sv_minplayers <n>     the fewest seated (bots count) before the match may start (2)
//
// THE KEYS (`lobby_keys`, outside the console and the menu): F1 toggleready, F2 team switch, F4
// togglewarmup; on the lobby's screen also F5 addbot, F6 removebot and ENTER start.
//
// ge:summary The lobby's console commands and settings: ready, break, team, start, addbot
// ge:keywords lobby console ready break notready toggleready team start addbot removebot
// ge:keywords sv_lobby sv_countdown sv_minplayers f1 f2 f4 f5 f6 enter lobby_keys
// ge:keywords add_lobby_commands
// ge:keywords warmup sitout togglewarmup
// ge:skill shooter console
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include "console/console.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

// The commands on `con` for `l`'s player, and the settings bound to `l.opt` (both must outlive
// the console).
void add_lobby_commands(ge::console::Console& con, Listen& l);

// The lobby's keys tapped this frame.
struct LobbyKeys {
  bool f1 = false, f2 = false, f4 = false, f5 = false, f6 = false, enter = false;
};
void lobby_keys(Listen& l, const LobbyKeys& keys);

}  // namespace shooters_edge::client
