// CONFIGS OVER THE SESSION (M21c): a server's settings followed by its clients, a client's own
// settings read by the server, and console text a server stuffs into a client, all through a
// session (session.hpp § CONFIGS) and a console's cvars (console/cvar.hpp's tiers).
//
// - THE SERVER INFO. Each frame the host sends the info string of its `kServerInfo` cvars when it
//   changed (`sv_maxrate 20000` in its console, a cfg, an admin's rcon), and each player gets it as
//   they are welcomed. A client applies it from `Source::server`: the server's cvars take its
//   values (a client's own `set` of one is refused), unknown names are created, and `sv_cheats`
//   among them decides whether the client's `kCheat` settings move (off puts them back). A change
//   reaches every client within a round trip of the host's frame.
// - THE USER INFO. Each frame the client sends the info string of its `kUserInfo` cvars when it
//   changed (`rate`, `cl_updaterate`, the name); the host keeps each seat's and reads a key with
//   `session_user_value`.
// - STUFFED TEXT runs in the client's console from `Source::server`: only commands registered
//   `kStuffable` run, and only the server's cvars are set (a stuffed `quit`, `bind` or `rate` is
//   refused and counted, the console's problem naming it).
//
// ge:summary Configs over a session: the server info followed, user info read, stuffed text ruled
// ge:keywords net session config server-info user-info stuffed-text stuffcmd follow apply_info
// ge:keywords info-string sv_cheats cheat rate cl_updaterate refused quit session_config
// ge:skill net console
// ge:test tests/net_session_config_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "net/session.hpp"

namespace ge::net {

// Host, each frame after the game's console has run: the server info sent when it changed.
// True when it was sent this frame.
bool session_host_follow_cvars(SessionHost& host, const console::Cvars& cvars);

// One key of a seat's user info ("" when it is not there).
std::string session_user_value(const SessionHost& host, int player, std::string_view key);

struct ClientConfig {
  std::string sent_user;  // the user info last sent
  uint64_t infos = 0;     // server infos applied
  uint64_t moved = 0;     // values they moved
  uint64_t stuffed = 0;   // stuffed lines run
  uint64_t refused = 0;   // stuffed lines the console refused (a problem each)
};

// Client, each frame after session_client_update: the server info and stuffed text in the
// session's events applied to `console`, and the user info sent when it changed.
void session_client_follow_cvars(SessionClient& client, console::Console& console,
                                 ClientConfig& state);

}  // namespace ge::net
