// `ge net`: the network's tools (docs/networking.md § The tools), over the engine's own wire
// format (engine/net). Each answers from a capture (a .pcap a lab attaches, `ge net sim` writes
// or a live node saves: net/capture.hpp) or from a session it runs itself:
//
//   ge net inspect <pcap>   every datagram decoded to one line: time, addresses, type,
//                           sequence, acks, each message's channel, id and size
//   ge net stats <pcap>     per connection, from the capturing node's side: packets and
//                           kilobits each way, round trip, loss each way, duplicates, reorders,
//                           resends; the local node's kbps, round trip and loss over time as a
//                           series file and its charts (`ge graph`)
//   ge net sim              fake-traffic clients and a host over chosen conditions (presets, a
//                           traffic mix, --link overrides): the same numbers the net-link lab
//                           prints, a digest a run, and a capture with --capture
//
// No game and no build are needed: everything runs inside ge.
//
// ge:summary ge net inspect, stats and sim: captures decoded and measured, fake traffic run
// ge:keywords cli ge net network inspect stats sim pcap capture rtt loss bandwidth kbps
// ge:keywords fake-traffic conditions preset mix link wireshark multiplayer
// ge:skill agent-interface net
// ge:test tests/cli_net_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

struct NetInspectOptions {
  std::string file;
  std::string peer;  // only datagrams to or from this address ("" for all)
  std::string type;  // only this packet type ("" for all)
  long long skip = 0;
  long long limit = 200;  // 0: every one
};

struct NetStatsOptions {
  std::string file;
  double bucket_ms = 250;
  std::string out;  // the series and charts' directory (default: <stem>-net/ beside the file)
  bool charts = true;
};

struct NetSimOptions {
  std::vector<std::string> presets;  // empty: every preset
  std::string mix = "racing";
  std::string link;        // overrides on every preset: "latency=40,jitter=10,loss=2%"
  bool stream = false;     // every link a stream (TCP): --transport=tcp
  double budget_kbps = 0;  // every connection's send budget
  int clients = 8;
  int max_peers = 32;
  double seconds = 10;
  double drain = 4;
  double tick_hz = 60;
  unsigned seed = 1;
  std::string capture;  // a .pcap path: the host's capture (one a preset when several)
};

Outcome run_net_inspect(const NetInspectOptions& opts);
Outcome run_net_stats(const NetStatsOptions& opts);
Outcome run_net_sim(const NetSimOptions& opts);
// A parsed `ge net <inspect|stats|sim>` line's flags read into the options, and run.
Outcome run_net(const Args& a);

}  // namespace ge::cli
