// The lobby's console: lobby_console.hpp's header lists the commands, settings and keys.
//
// ge:summary The lobby's console commands and settings: ready, break, team, start, addbot
// ge:keywords lobby console add_lobby_commands lobby_keys sv_lobby sv_countdown sv_minplayers
// ge:skill shooter console
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/client/lobby_console.hpp"

#include <string>

namespace shooters_edge::client {

namespace {

struct Word {
  const char* name;
  const char* help;
};

constexpr Word kWords[] = {
    {"ready", "ready: say you are ready; the countdown starts when everyone is"},
    {"break", "break: not ready after all; a running countdown stops"},
    {"notready", "notready: the same as break"},
    {"toggleready", "toggleready: ready or break, whichever you are not (F1)"},
    {"team", "team <red|blue|auto|switch>: change team (alone: say yours)"},
    {"start", "start: the leader starts the countdown whoever is ready"},
    {"lobby", "lobby: print the lobby's roster"},
    {"addbot", "addbot [novice|average|expert]: seat a bot in the warmup"},
    {"removebot", "removebot: free the highest bot seat"},
};

}  // namespace

void add_lobby_commands(ge::console::Console& con, Listen& l) {
  for (const Word& w : kWords)
    con.add_command(w.name, w.help, [&l](ge::console::Console&, const ge::console::Args& a) {
      std::string line = a[0];
      const std::string rest = a.rest(0);
      if (!rest.empty()) line += " " + rest;
      listen_lobby(l, line);
    });
  l.opt.lobby = true;
  con.cvars().bind({.name = "sv_lobby",
                    .fallback = "1",
                    .help = "1: a match opens every map in the lobby's warmup (ready, teams, the "
                            "countdown), 0: the match starts with the map",
                    .kind = ge::console::CvarKind::boolean},
                   &l.opt.lobby);
  con.cvars().bind({.name = "sv_countdown",
                    .fallback = "5",
                    .help = "the lobby's countdown once everyone is ready, s",
                    .kind = ge::console::CvarKind::number},
                   &l.opt.lobby_settings.countdown_s);
  con.cvars().bind({.name = "sv_minplayers",
                    .fallback = "1",
                    .help = "the fewest seated before the lobby's countdown starts",
                    .kind = ge::console::CvarKind::integer},
                   &l.opt.lobby_settings.min_players);
}

void lobby_keys(Listen& l, bool f1, bool f2, bool f3, bool& roster) {
  if (!l.room) return;
  if (f1) listen_lobby(l, "toggleready");
  if (f2) listen_lobby(l, "team switch");
  if (f3) roster = !roster;
}

}  // namespace shooters_edge::client
