// A REPLICATION TRIAL: one server-authoritative world over the simulated network, measured.
// Every client owns a mover it steers with a held direction that changes every half second, and
// the server moves a crowd of others in straight lines that bounce off the field's edge. Each
// client predicts its own and draws the rest interpolated; each tick the server rewinds a share
// of the crowd to the moment each client drew it. The trial answers what a choice of rates and a
// link cost and give: the bytes each way, the corrections, the delay the others are drawn at, the
// snapshots lost and the rows starved, how far a rewind lands from what was drawn. The
// `net-replicate` lab runs one a preset; `ge net sweep` runs one a value of a knob, to find where
// more packets stop buying anything. A pure function of its config: the same trial, the same
// hash.
//
// ge:summary A replication trial: a 32-client world over a link, bandwidth, corrections, rewind
// ge:keywords net replicate trial experiment sweep rate update-rate cmd-rate bandwidth
// ge:keywords corrections interpolation delay rewind multiplayer measure
// ge:skill net
// ge:test tests/net_replicate_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "net/conditions.hpp"
#include "net/replicate.hpp"

namespace ge::net {

struct ReplicateTrialConfig {
  int clients = 32;  // 1..32, each steering its own mover
  int npcs = 64;     // movers the server moves
  double seconds = 20;
  uint32_t seed = 1;  // the network's, the hands' and the crowd's stream
  LinkConditions link;
  // The rates, the interest radius, the budget and the rest; the trial sets the entity count,
  // the history ring and which fields interest reads. Every client asks the same rates.
  ReplicateConfig replicate;
  // Each client's clock runs this much fast (the even ones) or slow (the odd ones): a cheap
  // crystal, a busy machine. TIME SYNC is what keeps their inputs arriving on time anyway.
  double skew = 0;
};

struct ReplicateTrial {
  int owned = 0;                                // clients that heard of their own entity
  double down_kbps = 0, down_max_kbps = 0;      // snapshot bytes a client, mean and worst
  double wire_down_kbps = 0, wire_up_kbps = 0;  // every packet each way a client, headers too
  double corrections_per_min = 0;               // per client
  double worst_correction = 0;                  // metres
  double view_delay_ms = 0;  // how far behind the server's clock the others are drawn, mean
  uint64_t dropped = 0, decoded = 0, full = 0, starved = 0, repeated = 0, inputs = 0;
  uint64_t rewinds = 0, missed = 0;
  uint64_t trimmed = 0;            // inputs the server dropped for a queue past its most
  std::vector<double> input_ms;    // each input, made on its client to applied on the server
  double underrun = 0;             // share of drawn ticks past the newest snapshot (held)
  std::vector<double> t_q, queue;  // client 1's inputs waiting on the server, ten times a second
  std::vector<double> delay_ms;    // client 1's view delay at the same moments
  std::vector<double> dilation;    // client 1's clock speed at the same moments
  std::vector<double> gaps;        // a rewind against what was drawn, metres
  std::vector<double> t_s, kbps;   // snapshot kbit/s a client, each second
  uint64_t hash = 0;               // every end's final state
};

ReplicateTrial run_replicate_trial(const ReplicateTrialConfig& config);

}  // namespace ge::net
