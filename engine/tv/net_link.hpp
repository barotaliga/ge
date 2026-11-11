// TV OVER engine/net: a TV node that viewers connect to (`TvHost`), and a viewer's end that
// connects to one (`TvClient`), on any transport a net node runs on (UDP, TCP, the simulated
// network). A relay is both: a client of the node above it feeding a host of its own.
//
// One reliable channel carries the TV wire (node.hpp), its messages up to engine/net's biggest
// (a block in pieces under it); a viewer whose window is full is sent the rest on a later update.
// Every update: receive, hand the messages to the node or the watch, send what is due, flush.
//
// ge:summary TV over engine/net: a host viewers connect to, a viewer's end, and a relay of both
// ge:keywords tv net host client relay tree fan-out tv_host tv_client tv_relay channel
// ge:skill replay net
// ge:test tests/tv_net_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "net/node.hpp"
#include "tv/node.hpp"

namespace ge::tv {

// The net node's settings for a TV link: one reliable channel taking a block's piece.
net::NodeConfig tv_node_config(bool host, int max_peers, uint64_t seed);

struct TvHost {
  net::Node net;
  Node tv;
};

// A host on an open transport (`max_peers` up to engine/net's 32).
void tv_host_init(TvHost& h, const Config& config, net::Transport& transport, int max_peers,
                  uint64_t seed);
void tv_host_update(TvHost& h, int64_t now_us);

struct TvClient {
  net::Node net;
  Watch watch;
  std::string name;
  bool relay = false;
  int peer = -1;
  bool greeted = false;
  std::vector<std::vector<uint8_t>> outbox;  // messages up the link waits to take
  std::vector<std::vector<uint8_t>> fresh;   // stream bytes this update added (a relay's feed)
};

// A viewer's end connecting to `to`.
void tv_client_init(TvClient& c, net::Transport& transport, const net::Address& to,
                    std::string_view name, bool relay, uint64_t seed, int64_t now_us);
void tv_client_update(TvClient& c, int64_t now_us);
// A line said into the chat.
void tv_client_say(TvClient& c, std::string_view text);

// A relay: a client of the node above and a host of its own; what the client gets feeds the host.
struct TvRelay {
  TvClient up;
  TvHost down;
  int64_t reported = -1;  // the count under it last sent up
};
void tv_relay_init(TvRelay& r, const Config& config, net::Transport& up_transport,
                   const net::Address& to, net::Transport& down_transport, int max_peers,
                   uint64_t seed, int64_t now_us);
void tv_relay_update(TvRelay& r, int64_t now_us);

}  // namespace ge::tv
