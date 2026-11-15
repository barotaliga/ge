// THE ADMINISTRATOR'S FEATURES: the first features on the seam (feature.hpp), the same for every
// game on the server core (docs/servers.md § Open for extension). `admin_features` gives them in
// their order; a server's `main` puts them in its list (the dedicated process does, main.cpp).
//
// - STATUS. `status` on the server's console: the server's name, map and players, a row a seat
//   (seat, ping, team, name, address). A player may ask it too, as a command: the same rows
//   without the addresses.
// - KICK. `kick <seat|name>` on the console: the player is removed and may come back.
// - BANS. The address list (bans.hpp) kept in a file: `addip`, `removeip`, `listip`, `writeip` on
//   the console, and `ban <seat|name> [minutes]` (the player's address added, the player removed).
//   Each change is written to `sv_banfile` (default listip.cfg, in the game's directory, as
//   `addip` lines), read back when the server starts; `sv_banmode` 1 refuses the list's
//   addresses, 0 lets in only them. A player whose address the list refuses is refused a seat,
//   and one already seated is removed when an entry comes to match them.
// - RCON. A player's command `rcon <password> <line>` runs the line on the server's console and
//   sends back what it printed, over the session's authenticated link (net/session.hpp). No
//   `rcon_password`: "This server has no remote console."; a wrong one: "Bad rcon password.", and
//   after kRconTries wrong ones in a row the seat's tries wait kRconLockSeconds of server time.
// - THE LOG. Who joined from where and left why, chat, commands (an rcon line without its
//   password), kicks, bans and phases, a line each stamped with the server's time: the last
//   kLogKept kept (`AdminFeatures::log`) and, with `sv_logfile` set, appended to that file in the
//   game's directory.
//
// The clock bans expire by is the server's `ServerConfig::unix_us` (none: 0, so timed entries
// never expire), so a run over the simulated network replays to the bit.
//
// ge:summary The administrator's features: status, kick, bans in a file, rcon, the server log
// ge:keywords server admin administrator status kick ban bans banfile sv_banfile sv_banmode addip
// ge:keywords removeip listip writeip rcon remote-console rcon_password rcon-lockout server-log
// ge:keywords sv_logfile log admin_features
// ge:skill net
// ge:test tests/server_feature_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "server/bans.hpp"
#include "server/feature.hpp"
#include "server/server.hpp"

namespace ge::server {

constexpr int kRconTries = 3;              // wrong passwords in a row before the seat waits
constexpr double kRconLockSeconds = 30.0;  // how long it waits (server time)
constexpr size_t kLogKept = 256;           // log lines kept in memory

struct AdminFeatures {
  // Settings (cvars `sv_banfile`, `sv_banmode`, `sv_logfile`).
  std::string ban_file = "listip.cfg";
  int ban_mode = 1;
  std::string log_file;
  // State.
  BanList bans;
  bool bans_read = false;
  int rcon_wrong[kMaxClients] = {};
  int64_t rcon_until_us[kMaxClients] = {};
  uint64_t rcon_runs = 0, rcon_refused = 0, kicks = 0;
  std::deque<std::string> log;
};

// The log, status, kick, bans, rcon: in that order (the log first sees every line), each over `a`
// (which must outlive the list).
std::vector<Feature> admin_features(AdminFeatures& a);

// One line in the server log (`AdminFeatures::log` and `sv_logfile`), stamped with the time.
void admin_log(AdminFeatures& a, const Features& f, std::string_view line);

}  // namespace ge::server
