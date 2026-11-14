// THE HUB'S BROWSE (docs/hub.md § The classic adapter): servers that speak only their game's own
// status protocol, found the way the game's server browser finds them (net/server_list.hpp, the
// game's `ServerQuery`: its masters' list and its status query), read into the hub's index beside
// the nodes of the engine's own master. A community's hub so lists the servers already out there,
// which never heard of the engine's master or status record.
//
// Every `refresh_us` the list is refreshed: the game's masters asked, every server they name (and
// every one named by hand) queried. Each answer becomes a node's record (`status_of`: the name,
// map, mode, slots, the roster with spectators flagged, the teams' frags added up, the phase from
// what the server says of its match, and its keys); a node is marked `browsed`, so the poller
// never asks it in the engine's protocol, and is dropped once no refresh has heard it for
// `drop_us`. The clock is the caller's, so a refresh over the simulated network replays to the bit.
//
//   browse_init(b, config, game_query, socket)   a socket of its own, not the poller's
//   every turn: browse_update(b, poller, now, unix_s)   the list stepped, answers merged
//
// A record that shows a match ending keeps the match as a result (results.hpp), the hub's one
// record of what was played on a server that reports to nobody.
//
// ge:summary The hub's browse: servers on a game's own status protocol read into the index
// ge:keywords hub browse classic adapter server-list status-query master foreign protocol
// ge:keywords status_of merge browsed refresh
// ge:skill net
// ge:test tests/hub_browse_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "hub/poller.hpp"
#include "hub/results.hpp"
#include "net/server_list.hpp"

namespace ge::hub {

struct BrowseConfig {
  std::vector<net::Address> masters;  // the game's own masters
  std::vector<net::Address> servers;  // servers named by hand
  int64_t refresh_us = 30000000;
  int64_t drop_us = 300000000;  // a server no refresh has heard for this long leaves the index
};

struct BrowseStats {
  uint64_t refreshes = 0, merged = 0, changes = 0, added = 0, dropped = 0;
};

struct Browse {
  BrowseConfig config;
  net::ServerList list;
  int64_t next_refresh_us = 0;
  BrowseStats stats;
  Results results;  // the matches its servers were seen to finish (results.hpp)
  bool on = false;
};

void browse_init(Browse& b, const BrowseConfig& config, const net::ServerQuery& query,
                 net::Transport& socket, int64_t now_us);
// `unix_s` dates the results.
void browse_update(Browse& b, Poller& p, int64_t now_us, int64_t unix_s = 0);

// A server's answer in the game's own protocol as the engine's status record.
net::Status status_of(const net::ServerInfo& info);

}  // namespace ge::hub
