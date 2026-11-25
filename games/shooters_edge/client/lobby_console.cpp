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

void add_lobby_commands(ge::console::Console& con, Listen& l, const LobbyElsewhere& elsewhere) {
  ge::server::add_lobby_words(con, [&l, elsewhere](const std::string& line) {
    if (!elsewhere || !elsewhere(line)) listen_lobby(l, line);
  });
  ge::server::add_listen_cvars(con.cvars(), {.lobby = l.opt.lobby,
                                             .lobby_settings = l.opt.lobby_settings,
                                             .port = l.opt.port,
                                             .maxclients = l.opt.maxclients,
                                             .login = l.opt.login,
                                             .hostname = l.opt.hostname});
}

std::vector<uint16_t> lobby_taken_keys(const Listen& l) {
  std::vector<uint16_t> out;
  if (!l.room || l.room->lobby.phase == sv::LobbyPhase::match) return out;
  // Their HID usages (platform/window.hpp's Key): F1, F2, F4; F5, F6, ENTER.
  for (const uint16_t k : {uint16_t{58}, uint16_t{59}, uint16_t{61}}) out.push_back(k);
  if (on_lobby_screen(l))
    for (const uint16_t k : {uint16_t{62}, uint16_t{63}, uint16_t{40}}) out.push_back(k);
  return out;
}

void lobby_keys(Listen& l, const LobbyKeys& k, const LobbyElsewhere& elsewhere) {
  const auto word = [&](const char* line) {
    if ((!elsewhere || !elsewhere(line)) && l.room) listen_lobby(l, line);
  };
  if (k.f1) word("toggleready");
  if (k.f2) word("team switch");
  if (k.f4) word("togglewarmup");
  if (!l.room || !on_lobby_screen(l))
    return;  // the rest are the lobby's screen's: on the map they are the game's keys
  if (k.f5) listen_lobby(l, "addbot");
  if (k.f6) listen_lobby(l, "removebot");
  if (k.enter) listen_lobby(l, "start");
}

}  // namespace shooters_edge::client
