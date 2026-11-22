// A WINDOW THAT HOSTS, for any game whose player can open his own match to others: the lobby's
// words a player types (or a key runs) and the settings that shape the hosted match, on the
// window's console. A dedicated server binds its own (server.hpp: `net_port`, `sv_lobby` 0 by
// default); a window's match opens in the lobby and nobody joins until `sv_port` is set.
//
// THE WORDS (each hands its whole line to the game's lobby): ready, break (notready), toggleready,
// team <red|blue|auto|switch>, start, warmup, sitout, togglewarmup, lobby, addbot [skill],
// removebot. The keys their help names (F1, F2, F4, F5, F6, ENTER) are the game's to bind.
//
// THE SETTINGS (read when a map starts): `sv_lobby`, `sv_countdown`, `sv_minplayers`, `sv_port`,
// `sv_maxclients`, and the logins (`sv_login`, `sv_logintime`, `sv_loginwelcome`,
// `sv_accountmaster`, `sv_accountkey`; login.hpp).
//
// ge:summary A hosting window's lobby words and settings: ready, team, sv_lobby, sv_port, sv_login
// ge:keywords lobby console ready break notready toggleready team start addbot removebot warmup
// ge:keywords sitout togglewarmup sv_lobby sv_countdown sv_minplayers sv_port sv_maxclients
// ge:keywords sv_login sv_logintime sv_loginwelcome sv_accountmaster sv_accountkey listen-server
// ge:keywords hosting add_lobby_words add_listen_cvars
// ge:skill net console
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include <functional>
#include <string>

#include "console/console.hpp"
#include "server/login.hpp"
#include "server/match.hpp"

namespace ge::server {

// The lobby's words on `con`, each running `run` with the line typed ("team red").
void add_lobby_words(console::Console& con, const std::function<void(const std::string&)>& run);

// What the hosting settings write (each must outlive the cvars).
struct ListenTargets {
  bool& lobby;
  LobbySettings& lobby_settings;
  int& port;
  int& maxclients;
  LoginSettings& login;
  std::string& hostname;  // sv_hostname: the match's name in the browser and the lobby
};

// The settings over `t`; `lobby` is set on (a window's match opens in the lobby).
void add_listen_cvars(console::Cvars& cvars, const ListenTargets& t);

}  // namespace ge::server
