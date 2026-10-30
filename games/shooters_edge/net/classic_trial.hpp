// ONE MEASURED CLASSIC SESSION: the test mod on the test arena served by the classic server
// (classic_server.hpp) to scripted clients (classic_client.hpp) over engine/net's simulated
// network under one condition preset, for a stretch of server time. What the server's tests and
// the `se-net` lab run.
//
// THE CLIENTS (`TrialOptions::clients`, 1..16): client i asks for its extensions by
// `ClientExtMode` i mod 3 (lines, pext, none: a base-protocol client gets every base form), wears
// the skin `kTrialSkins[i mod 3]` (the server offers two of them for download; every client holds
// only the base skin), and plays the default script. One server frame of 13 ms is one tick of
// every client; the network moves at the same clock.
//
// THE DIGEST folds every server frame's loop hash and, at the end, every client's stage, counts,
// stats and the downloads' bytes: a session replays to the bit from its options.
//
// ge:summary One classic session over the simulated network, measured: what tests and se-net run
// ge:keywords classic-trial session simulated-network preset clients signon bytes-per-second
// ge:keywords loss digest run_classic_trial trial-files
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/capture.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

extern const char* const kTrialSkins[3];

struct TrialOptions {
  std::string preset = "lan";
  int clients = 4;
  double seconds = 10;
  uint32_t seed = 1;
  bool antilag = false;
  bool capture = false;  // keep a pcap of the server's datagrams
  int maxrate = 0;
};

struct TrialClient {
  std::string name, mode, stage, refused;
  double connect_s = -1, active_s = -1;  // network time when connected and when playing
  int64_t bytes_in = 0, bytes_out = 0;
  int64_t full_frames = 0, delta_frames = 0, delta_misses = 0, entities = 0, players_seen = 0;
  int64_t prints = 0, chunks_oob = 0;
  int downloads = 0;  // files fetched and equal to the server's
  int files = 0;      // files fetched
  int stats_health = 0;
  std::string ext;
};

struct TrialReport {
  bool ok = false;
  std::string error;
  std::string preset;
  double seconds = 0;
  int frames = 0;
  std::vector<TrialClient> clients;
  ServerCounts server;
  int64_t moves = 0, moves_unchecked = 0, moves_bad_checksum = 0, lag_rewinds = 0;
  int64_t net_sent = 0, net_lost = 0;
  std::vector<double> server_kbps;  // a second a point: the server's bytes out, kbit/s
  std::vector<std::string> log;     // the server's connects, rejects and drops
  uint64_t digest = 0;
  std::vector<uint8_t> pcap;  // with `capture`
};

// The files the trial's server offers (deterministic bytes).
std::vector<ServedFile> trial_files();

TrialReport run_classic_trial(const TrialOptions& opt);

}  // namespace shooters_edge::net
