// FAKE TRAFFIC: a whole session run on the simulated network with no game behind it: a host and
// up to 32 clients connect, every client sends inputs (sequenced, 60 Hz) and events (reliable),
// the host sends each client snapshots (unreliable, 20 Hz) and events back, for a while, then
// everyone stops sending new messages and the links drain. Every message carries its stream's
// count and the time it was sent, so the report says exactly what arrived, how late, in what
// order and twice or not, beside what each link measured (round trip, loss, bandwidth) and what
// the wire did (`SimStats`). The `net-link` lab sweeps it over the condition presets, the tests
// hold it to its guarantees, and `ge net sim` (M17b) runs it from the command line.
//
// The run is a pure function of its plan: the same plan is the same report, bit for bit, and the
// `digest` says so in one number.
//
// ge:summary Fake traffic: a host and clients over the simulated network, measured end to end
// ge:keywords net traffic fake-traffic generator load session simulate bots latency loss
// ge:keywords reliable-delivery in-order goodput bandwidth report digest stress
// ge:skill net
// ge:test tests/net_traffic_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/capture.hpp"
#include "net/conditions.hpp"
#include "net/connection.hpp"
#include "net/sim_network.hpp"

namespace ge::net {

// The traffic's channels: 0 sequenced (inputs up), 1 unreliable (snapshots down), 2 reliable
// (events both ways).
constexpr int kTrafficInputs = 0;
constexpr int kTrafficSnapshots = 1;
constexpr int kTrafficEvents = 2;

struct TrafficMix {
  double input_hz = 60;  // each client to the host
  size_t input_bytes = 24;
  double snapshot_hz = 20;  // the host to each client
  size_t snapshot_bytes = 320;
  double event_hz = 4;  // each way, on every link
  size_t event_bytes = 48;
};

struct TrafficPlan {
  int clients = 8;
  int max_peers = 32;        // the host's slots; clients past it are denied
  double seconds = 10;       // sending
  double drain_seconds = 4;  // then nothing new, while the reliable messages finish
  double tick_hz = 60;       // how often every node updates and flushes
  uint32_t seed = 1;
  LinkConditions conditions;  // every link, both ways
  TrafficMix mix;
  bool capture = false;    // capture the host's transport
  double sample_s = 0.25;  // the series' step
};

// One kind of message, summed over its streams.
struct StreamTally {
  uint64_t sent = 0, refused = 0, delivered = 0, out_of_order = 0, duplicated = 0;
  std::vector<double> latency_ms;  // each delivery, one way
};

struct TrafficReport {
  int connected = 0;  // clients that reached connected
  int denied = 0;     // clients refused (the host was full)
  int dropped = 0;    // connections lost before the end
  StreamTally inputs, snapshots, events_up, events_down;
  // What the host's links measured at the end of the sending, averaged over its peers.
  double rtt_ms = 0, rtt_var_ms = 0, jitter_ms = 0, loss = 0;
  double loss_total = 0;  // the host's packets lost over the whole sending, of those settled
  double host_send_kbps = 0, host_recv_kbps = 0;  // mean over the sending, the host's total
  uint64_t resends = 0, dropped_unsent = 0, duplicates = 0, stale = 0;
  SimStats wire;
  std::vector<double> t_s, series_rtt_ms, series_loss;  // the host's mean over its peers
  Capture capture;                                      // empty unless the plan asked for it
  uint64_t digest = 0;
};

TrafficReport run_traffic(const TrafficPlan& plan);

}  // namespace ge::net
