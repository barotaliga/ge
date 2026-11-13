// THE HUB'S JSON API (docs/hub.md § The JSON API): read-only, CORS open, every answer cacheable
// for its refresh time. The pages will use nothing else, so another community's tools can too.
// Pure: an answer is a function of the poller's index, the game's spec, the path and the time,
// so labs and tests call it without a socket and the HTTP server (http.hpp) only carries it.
//
// THE PATHS:
// - `/v1`: the game, the hub's counts, every path below.
// - `/v1/servers?mode=&min_players=&kind=&has_players=`: game nodes' records, by interest.
// - `/v1/servers/{address}`: one node's record; 404 when unknown.
// - `/v1/servers/{address}/events`: server-sent events (http.hpp), the record as it changes.
// - `/v1/streams`: every live broadcast a record names: node, stream, server, delay, viewers.
// - `/v1/nodes?kind=`: every node the masters list, answered or not (TV nodes, forwarders too).
// - `/v1/masters`: the masters and when their list last came.
//
// ge:summary The hub's JSON API: servers by interest, one server, streams, nodes, masters
// ge:keywords hub api json v1 servers streams nodes masters filter mode interest cors cache 404
// ge:skill net
// ge:test tests/hub_api_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hub/poller.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

struct Answer {
  int code = 200;
  std::string type = "application/json";
  std::string body;
  int max_age_s = 0;     // Cache-Control's max-age
  std::string encoding;  // Content-Encoding ("gzip": the body is packed; "" plain)
  bool events = false;   // the path asks for an event stream (http.hpp keeps it open)
  net::Address node;     // ... of this node
};

// The answer to GET `target` (a path with its query). `now_us` dates the ages in it.
Answer api_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us);

// One node's record as the API writes it (the events stream sends the same).
std::string node_json(const HubSpec& spec, const HubNode& n, int64_t now_us);

// A query parameter's value ("" when absent), percent-decoding it.
std::string query_param(std::string_view target, std::string_view key);
// The path of a target without its query, percent-decoded.
std::string target_path(std::string_view target);

}  // namespace ge::hub
