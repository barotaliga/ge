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
#include <string_view>
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
  int game = 1;         // Quake's number (2: Quake II, `+set fs_basegame q2`)
  int port = 27500;
};

// What starting a server for `map` with the page's `settings` asks (the settings as cfg lines).
ge::server::SpawnAsk dedicated_ask(
    const DedicatedHost& host, const std::string& map,
    const std::vector<std::pair<std::string, std::string>>& settings);

// `hostdedicated` and `stopdedicated` on the window's console (`host` copied into them). A server
// started or found running is joined with `connect` on this machine's port.
void add_dedicated_commands(ge::console::Console& console, const DedicatedHost& host);

// The window's: `sesv` beside it, the install `root`, the state and logs under the player's
// `prefs` folder (`<prefs>/cli`, `<prefs>/logs`), `+set fs_basegame q2` for Quake II's `game_id`;
// nothing when there is no prefs folder.
void add_window_dedicated(ge::console::Console& console, const std::string& prefs,
                          const std::string& root, std::string_view game_id);

}  // namespace shooters_edge::client
