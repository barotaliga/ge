// THE SERVER TRIAL: one game's server run three ways off one seed, for the tests and the
// `server-core` lab, on the simulated network (net/sim_network.hpp):
//
// - DEDICATED, offline: `server_frame` back to back, as `<game>_server --offline` runs.
// - LISTEN: the same server stepped by a client loop at `client_hz`, each client frame running the
//   server frames that have come due, as a game's window hosts one.
// - NETWORKED: the dedicated server on the game's port with the LAN's discovery port; a browser on
//   the LAN probes every second and records what it sees (the name, the seats, locked or not), and
//   `clients` players join the session one a second.
//
// The three give the same game digest (the network never touches the game), and the networked
// run's sightings, seats and times replay to the bit under any condition preset.
//
// ge:summary The server trial: dedicated, listen and networked runs of one server, a LAN browser
// ge:keywords server trial listen-server dedicated-server lan-browser sightings seats determinism
// ge:keywords run_server_trial simulated-network
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/conditions.hpp"
#include "server/server.hpp"

namespace ge::server {

struct TrialConfig {
  const ServerGame* game = nullptr;  // null: the sample game (sample.hpp)
  ServerConfig server;               // `offline` is ignored: each run picks its own
  net::LinkConditions link;
  uint32_t seed = 1;       // the network's
  int clients = 3;         // players joining the session, one a second from 1.5 s
  double client_hz = 144;  // the listen server's client loop
};

// One probe's answer, as a server browser would show it.
struct TrialSighting {
  double at_s = 0;
  std::string name;
  int seated = 0, max_players = 0;
  bool locked = false;
};

struct ServerTrial {
  bool ok = false;
  std::string error;
  int64_t frames = 0;
  uint64_t dedicated = 0, listen = 0, networked = 0;  // the game's digest each way
  int listen_client_frames = 0;
  std::vector<TrialSighting> sightings;
  std::vector<double> seats_t, seats_n;  // the server's seats (game and session) as a staircase
  std::vector<double> seated_at_s;       // each client's seat, from its join (-1: never seated)
  int clients_seated = 0;                // the session's players at the end
  std::string status;                    // the networked server's last status line
  std::string banner;
  uint64_t pings = 0, lan_answers = 0;
  uint64_t digest = 0;  // the game's, the sightings and the seats folded
};

ServerTrial run_server_trial(const TrialConfig& config);

}  // namespace ge::server
