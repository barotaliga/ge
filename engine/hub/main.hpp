// A HUB'S PROCESS: what every game's `<game>_hub` binary runs (cmake/Hub.cmake's `ge_add_hub`
// links a three-line main that calls `hub_main` with the game's HubSpec). One UDP socket for the
// masters and the status asks (poller.hpp), one TCP port for HTTP (http.hpp), paced by the wall
// clock in small sleeps; a status line every `--status` seconds.
//
// THE MASTER BESIDE IT: `--serve-master[=port]` runs the master (net/master.hpp) in the same
// process on its own UDP port (27900 by default), so a community runs one binary for both; with
// no `--master` the hub then reads that one.
//
// THE BROWSE BESIDE IT: a game whose spec gives its own status query (spec.hpp's `browse`) reads
// `--browse-master` (the game's masters) and `--browse-server` (servers by hand) on a second UDP
// socket (browse.hpp): the servers already out there, which never heard of the engine's master.
//
// THE FLAGS: --master=<host:port,...> (the masters to read), --serve-master[=port], --ip (where to
// bind, both sockets), --port (the UDP socket's; 0 any), --http (the web port; the game's default),
// --list-every / --status-every / --busy-every (seconds), --for (seconds; 0 runs until killed),
// --status (seconds between status lines), --browse-master / --browse-server (host:port,...),
// --browse-every (seconds), --help.
//
// ge:summary A hub's process: flags, the poller's socket, the HTTP port, the loop, status lines
// ge:keywords hub hub_main process flags master http port loop status ge_add_hub
// ge:skill net
// ge:test tests/hub_http_test.cpp
#pragma once

#include <string>
#include <vector>

#include "hub/spec.hpp"

namespace ge::hub {

struct HubOptions {
  std::string masters;
  std::string ip = "0.0.0.0";
  int port = 0, http = 0;  // http 0: the spec's
  int serve_master = 0;    // the master's own port (0: none run here)
  double list_s = 60, status_s = 30, busy_s = 4;
  double for_s = 0, status_line_s = 60;
  std::string browse_masters, browse_servers;  // the game's own masters and servers (browse.hpp)
  double browse_s = 30;                        // the browse's refresh
  std::string archive;                         // the demo archive's directory ("" none)
  double keep_gb = 0, keep_days = 0;           // the archive's retention (0: no limit)
  bool help = false;
};

// The flags (without the program's name) read into `out`; false with `error` (the flag and the
// next action).
bool hub_options(const std::vector<std::string>& args, HubOptions& out, std::string& error);
std::string hub_help(const HubSpec& spec);

// The whole process: 0 done, 1 refused (a flag, a socket).
int hub_main(int argc, char** argv, const HubSpec& spec);

}  // namespace ge::hub
