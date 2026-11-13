// The lobby's console: lobby_console.hpp's header lists the commands, settings and keys.
//
// ge:summary The lobby's console commands and settings: ready, break, team, start, addbot
// ge:keywords lobby console add_lobby_commands lobby_keys sv_lobby sv_countdown sv_minplayers
// ge:keywords warmup sv_port sv_maxclients listen-server hosting sv_login sv_accountmaster
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
    {"start", "start: the leader starts the countdown whoever is ready (ENTER on the lobby)"},
    {"warmup", "warmup: leave the lobby's screen for the warmup on the map (F4)"},
    {"sitout", "sitout: leave the warmup for the lobby's screen (F4 on the map)"},
    {"togglewarmup",
     "togglewarmup: the warmup or the lobby's screen, whichever you are not on (F4)"},
    {"lobby", "lobby: print the lobby's roster"},
    {"addbot", "addbot [novice|average|expert]: seat a bot in the warmup (F5)"},
    {"removebot", "removebot: free the highest bot seat (F6)"},
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
                    .fallback = "2",
                    .help = "the fewest seated (bots count) before the match may start",
                    .kind = ge::console::CvarKind::integer},
                   &l.opt.lobby_settings.min_players);
  con.cvars().bind({.name = "sv_port",
                    .fallback = "0",
                    .help = "the UDP port others join the next match on (0: nobody joins); "
                            "takes effect with the next map",
                    .kind = ge::console::CvarKind::integer},
                   &l.opt.port);
  con.cvars().bind({.name = "sv_maxclients",
                    .fallback = "32",
                    .help = "the match's slots: the player, the bots and who joins (1..32)",
                    .kind = ge::console::CvarKind::integer},
                   &l.opt.maxclients);
  // The logins on a match others join (server/login.hpp); with no master the window keeps the
  // accounts itself, in its own folder. Each takes effect with the next map.
  con.cvars().bind({.name = "sv_login",
                    .fallback = "0",
                    .help = "whether players joining the match must log in to an account (0 no, "
                            "1 players, 2 everyone); takes effect with the next map",
                    .kind = ge::console::CvarKind::integer},
                   &l.opt.login.login);
  con.cvars().bind({.name = "sv_logintime",
                    .fallback = "60",
                    .help = "how long a player who must log in has to do it, s",
                    .kind = ge::console::CvarKind::number},
                   &l.opt.login.time_s);
  con.cvars().bind({.name = "sv_loginwelcome",
                    .fallback = "",
                    .help = "a line shown before the login prompt, such as where to get a code",
                    .kind = ge::console::CvarKind::text},
                   &l.opt.login.welcome);
  con.cvars().bind({.name = "sv_accountmaster",
                    .fallback = "",
                    .help = "the account master logins are checked by (\"\": the window keeps "
                            "its own accounts); takes effect with the next map",
                    .kind = ge::console::CvarKind::text},
                   &l.opt.login.master);
  con.cvars().bind({.name = "sv_accountkey",
                    .fallback = "",
                    .help = "the account master's public key, 64 hex digits (\"\": learned at "
                            "the first answer)",
                    .kind = ge::console::CvarKind::text},
                   &l.opt.login.key);
}

void lobby_keys(Listen& l, const LobbyKeys& k) {
  if (!l.room) return;
  const bool page = on_lobby_screen(l);
  if (k.f1) listen_lobby(l, "toggleready");
  if (k.f2) listen_lobby(l, "team switch");
  if (k.f4) listen_lobby(l, "togglewarmup");
  if (!page) return;  // the rest are the lobby's screen's: on the map they are the game's keys
  if (k.f5) listen_lobby(l, "addbot");
  if (k.f6) listen_lobby(l, "removebot");
  if (k.enter) listen_lobby(l, "start");
}

}  // namespace shooters_edge::client
