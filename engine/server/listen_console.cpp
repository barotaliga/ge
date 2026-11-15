// A hosting window's lobby words and settings: listen_console.hpp's header lists them.
//
// ge:summary A hosting window's lobby words and settings: ready, team, sv_lobby, sv_port, sv_login
// ge:keywords lobby console add_lobby_words add_listen_cvars sv_lobby sv_port sv_login hosting
// ge:skill net console
// ge:test tests/shooters_edge_lobby_test.cpp
#include "server/listen_console.hpp"

namespace ge::server {

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

void add_lobby_words(console::Console& con, const std::function<void(const std::string&)>& run) {
  for (const Word& w : kWords)
    con.add_command(w.name, w.help, [run](console::Console&, const console::Args& a) {
      std::string line = a[0];
      const std::string rest = a.rest(0);
      if (!rest.empty()) line += " " + rest;
      run(line);
    });
}

void add_listen_cvars(console::Cvars& cvars, const ListenTargets& t) {
  t.lobby = true;
  cvars.bind({.name = "sv_lobby",
              .fallback = "1",
              .help = "1: a match opens every map in the lobby's warmup (ready, teams, the "
                      "countdown), 0: the match starts with the map",
              .kind = console::CvarKind::boolean},
             &t.lobby);
  cvars.bind({.name = "sv_countdown",
              .fallback = "5",
              .help = "the lobby's countdown once everyone is ready, s",
              .kind = console::CvarKind::number},
             &t.lobby_settings.countdown_s);
  cvars.bind({.name = "sv_minplayers",
              .fallback = "2",
              .help = "the fewest seated (bots count) before the match may start",
              .kind = console::CvarKind::integer},
             &t.lobby_settings.min_players);
  cvars.bind({.name = "sv_port",
              .fallback = "0",
              .help = "the UDP port others join the next match on (0: nobody joins); "
                      "takes effect with the next map",
              .kind = console::CvarKind::integer},
             &t.port);
  cvars.bind({.name = "sv_maxclients",
              .fallback = "32",
              .help = "the match's slots: the player, the bots and who joins (1..32)",
              .kind = console::CvarKind::integer},
             &t.maxclients);
  // The logins on a match others join (login.hpp); with no master the window keeps the accounts
  // itself, in its own folder. Each takes effect with the next map.
  cvars.bind({.name = "sv_login",
              .fallback = "0",
              .help = "whether players joining the match must log in to an account (0 no, "
                      "1 players, 2 everyone); takes effect with the next map",
              .kind = console::CvarKind::integer},
             &t.login.login);
  cvars.bind({.name = "sv_logintime",
              .fallback = "60",
              .help = "how long a player who must log in has to do it, s",
              .kind = console::CvarKind::number},
             &t.login.time_s);
  cvars.bind({.name = "sv_loginwelcome",
              .fallback = "",
              .help = "a line shown before the login prompt, such as where to get a code",
              .kind = console::CvarKind::text},
             &t.login.welcome);
  cvars.bind({.name = "sv_accountmaster",
              .fallback = "",
              .help = "the account master logins are checked by (\"\": the window keeps "
                      "its own accounts); takes effect with the next map",
              .kind = console::CvarKind::text},
             &t.login.master);
  cvars.bind({.name = "sv_accountkey",
              .fallback = "",
              .help = "the account master's public key, 64 hex digits (\"\": learned at "
                      "the first answer)",
              .kind = console::CvarKind::text},
             &t.login.key);
}

}  // namespace ge::server
