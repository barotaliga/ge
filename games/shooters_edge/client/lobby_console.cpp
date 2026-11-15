// The lobby's console: the engine's lobby words and hosting settings (server/listen_console.hpp)
// run on the window's listen server, and the lobby's keys.
//
// ge:summary The lobby's console commands and settings: ready, break, team, start, addbot
// ge:keywords lobby console add_lobby_commands lobby_keys sv_lobby sv_countdown sv_minplayers
// ge:keywords warmup sv_port sv_maxclients listen-server hosting sv_login sv_accountmaster
// ge:skill shooter console
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/client/lobby_console.hpp"

#include <string>

#include "server/listen_console.hpp"

namespace shooters_edge::client {

void add_lobby_commands(ge::console::Console& con, Listen& l) {
  ge::server::add_lobby_words(con, [&l](const std::string& line) { listen_lobby(l, line); });
  ge::server::add_listen_cvars(con.cvars(), {.lobby = l.opt.lobby,
                                             .lobby_settings = l.opt.lobby_settings,
                                             .port = l.opt.port,
                                             .maxclients = l.opt.maxclients,
                                             .login = l.opt.login});
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
