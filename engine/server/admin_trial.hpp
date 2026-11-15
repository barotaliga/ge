// THE ADMINISTRATOR'S TRIAL: the administrator's features (admin.hpp) on a dedicated server over
// the simulated network, for the tests and the `server-core` lab. Three players join (Ana, Bo,
// Cy); Ana asks the server's status, tries rcon with a wrong password and is refused, kicks Bo
// with the right one (Bo comes back), bans Cy (Cy's return is refused), lists the bans, uses a
// command no feature knows and the trial's own feature (`hello`, a feature the trial adds to the
// list: no edit in the core), then tries wrong passwords until her tries wait. Every line a player
// is sent, every log line and the ban file come back, and a fold of them all replays to the bit
// under any condition preset.
//
// ge:summary The admin trial: status, rcon refused and run, kick, ban, a feature of its own
// ge:keywords server admin trial rcon kick ban status run_admin_trial simulated-network
// ge:skill net
// ge:test tests/server_feature_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/conditions.hpp"
#include "server/server.hpp"

namespace ge::server {

struct AdminTrialConfig {
  net::LinkConditions link;
  uint32_t seed = 1;
  std::string rcon_password = "secret";
};

// A line one player was sent, or one they sent (`sent`).
struct AdminLine {
  double at_s = 0;
  std::string player;
  std::string text;
  bool sent = false;
};

struct AdminTrial {
  bool ok = false;
  std::string error;
  std::vector<AdminLine> lines;                // in time order
  std::vector<std::string> log;                // the server log (admin.hpp)
  std::string ban_file;                        // what the server wrote to sv_banfile
  bool bo_kicked = false, bo_back = false;     // Bo removed, and seated again
  bool cy_banned = false, cy_refused = false;  // Cy removed, and refused coming back
  bool hello_answered = false, unknown_told = false;
  bool wrong_refused = false, locked = false;  // a wrong password refused; the tries made wait
  uint64_t rcon_runs = 0, rcon_refused = 0, commands = 0, refused_joins = 0;
  int seated_end = 0;
  uint64_t digest = 0;
};

AdminTrial run_admin_trial(const AdminTrialConfig& config);

}  // namespace ge::server
