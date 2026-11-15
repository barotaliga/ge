// THE HOST PAGE'S DEDICATED SERVER (docs/servers.md § The shape; engine/server/spawn.hpp): NEW
// GAME with SERVER set to DEDICATED starts the game's own server program (`sesv`, beside the
// window) as a process of its own instead of hosting in the window, hands it the page's rules as
// a cfg, and joins it. It keeps running when the window quits; the record in the game's state
// folder (`<prefs>/cli`, the player CLI's too, so `secli host list` sees it) lets the next window,
// or the CLI, find it again and stop it.
//
//   hostdedicated <map> [<setting> <value>]...  started (or, already running, joined) and joined
//   stopdedicated                                the one this machine started on the port, stopped
//
// The words printed are a player's: what happened and what to do, never a command's syntax.
//
// ge:summary The host page's dedicated server: sesv started with the page's rules, joined, stopped
// ge:keywords hostdedicated stopdedicated host-page dedicated-server spawn sesv new-game join
// ge:keywords state-folder prefs-cli
// ge:skill net ui
// ge:test tests/shooters_edge_host_dedicated_test.cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "console/console.hpp"
#include "server/spawn.hpp"

namespace shooters_edge::client {

struct DedicatedHost {
  std::string program;  // the server program's path (sesv beside the window)
  std::string root;     // the install folder the server reads (its --root)
  std::string state;    // where its record goes (`<prefs>/cli`)
  std::string logs;     // where its log goes (`<prefs>/logs`)
  int game = 1;         // the classic game's number (`-game 2`: the sequel)
  int port = 27500;
};

// What starting a server for `map` with the page's `settings` asks (the settings as cfg lines).
ge::server::SpawnAsk dedicated_ask(
    const DedicatedHost& host, const std::string& map,
    const std::vector<std::pair<std::string, std::string>>& settings);

// `hostdedicated` and `stopdedicated` on the window's console; `host` must outlive it. A server
// started or found running is joined with `connect` on this machine's port.
void add_dedicated_commands(ge::console::Console& console, const DedicatedHost& host);

}  // namespace shooters_edge::client
