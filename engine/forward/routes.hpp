// THE ROUTE FINDER (docs/tv.md § Forward): the path to a server with the lowest summed round
// trip, straight or through forwarders, worked out from what a server browser already measured.
// Any game's browser runs it: the game hands it its forwarders' protocol as a table of functions
// (`RouteQuery`), as it hands the server list its status query (net/server_list.hpp).
//
// THE TABLE is a graph of hosts. The player links to every server and forwarder the browser
// pinged, at that ping; each forwarder links to every server it reports with its own ping (a
// forwarder asked for its list answers with each server it knows and its round trip to it). A
// route's cost is the sum of its legs' round trips. Hosts are matched by address without the port,
// so a forwarder that reports a server reaches the browser's entry for it whatever port it names,
// and a forwarder beside a server is the same host. The shortest path (Dijkstra's, ties broken by
// the fewer hops and then the table's order, so the same table gives the same route) is the route.
//
// THE FINDER asks every forwarder the list found (an entry the game's `is_forwarder` says is one)
// for its list, from the browser's own socket, `tries` times at most, `timeout_us` apart, paced
// `gap_us`; a reply is matched by the host it came from.
//
//   route_finder_start(rf, query, transport, list, now)   once the list's refresh is done
//   every frame: route_finder_update(rf, now)            asks sent, replies read
//   best_route(route_finder_table(rf, list), server)     the route to one server
//
// ge:summary The route finder: the lowest summed ping to a server, direct or through forwarders
// ge:keywords forward route-finder shortest-path dijkstra pingstatus forwarders browser ping-tree
// ge:keywords best_route route_finder_start route_table
// ge:skill net
// ge:test tests/forward_routes_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "net/address.hpp"
#include "net/server_list.hpp"
#include "net/transport.hpp"

namespace ge::forward {

using Reach = std::vector<std::pair<net::Address, int>>;  // a host and its round trip, ms

struct RouteTable {
  Reach direct;  // the player's own: every server and forwarder the browser pinged
  struct Hop {
    net::Address forwarder;  // as the player reaches it (with its port)
    Reach reach;             // what it reports
  };
  std::vector<Hop> hops;
};

struct Route {
  bool found = false;
  std::vector<net::Address> forwarders;  // nearest first; empty: straight to the server
  int ping_ms = -1;                      // the summed round trip
  int direct_ms = -1;                    // the player's own to the server (-1: none measured)
};

// The best route to `server` (its port kept for the last leg).
Route best_route(const RouteTable& t, const net::Address& server);
// "forwarder@forwarder@server": the route as a chain names it.
std::string route_chain(const Route& r, const net::Address& server);

// THE GAME'S PROTOCOL for its forwarders. Each writer returns the bytes written (at most `cap`).
struct RouteQuery {
  bool (*is_forwarder)(const net::ServerInfo& info) = nullptr;
  size_t (*ask)(uint8_t* out, size_t cap) = nullptr;
  bool (*read)(const uint8_t* data, size_t size, Reach& out) = nullptr;
};

struct RouteFinder {
  RouteQuery query;
  net::Transport* transport = nullptr;
  int tries = 3;
  int64_t timeout_us = 1000000, gap_us = 100000;
  struct Asked {
    net::Address forwarder;
    int tries = 0;
    int64_t asked_us = -1;
    bool answered = false;
    Reach reach;
  };
  std::vector<Asked> asked;
  int64_t next_us = 0;
  uint64_t asks = 0, replies = 0, foreign = 0;
};

// Every forwarder `list` holds asked for its list, from `transport` (a socket of its own, or the
// browser's when the list is done with it).
void route_finder_start(RouteFinder& rf, const RouteQuery& query, net::Transport& transport,
                        const net::ServerList& list, int64_t now_us);
void route_finder_update(RouteFinder& rf, int64_t now_us);
// Every forwarder answered or given up (its last ask's wait passed).
bool route_finder_done(const RouteFinder& rf, int64_t now_us);
// The table from the list's pings and the forwarders' answers.
RouteTable route_finder_table(const RouteFinder& rf, const net::ServerList& list);

}  // namespace ge::forward
