// A LINK'S CONDITIONS and every packet's fate on it: how late it arrives, whether it is lost,
// doubled or overtaken, and how long it queues behind the ones before it. The simulated network
// (sim_network.hpp) asks `decide` once a packet, in the order packets are sent, and every draw
// comes off the caller's seeded stream, so the same seed and the same sends are the same fates:
// a session over a bad link is as reproducible as one over a perfect one.
//
// The model, stated once:
// - delay = the queue's wait + the time to clock the packet out at the link's rate + latency +
//   jitter (uniform in [0, jitter]); a link is a queue, so with no reorder no packet overtakes
//   another, whatever its jitter;
// - loss: independent drops at `loss` when `burst` is 1; otherwise a Gilbert-Elliott chain whose
//   bad state drops every packet, with mean run of drops `burst` and long-run share `loss`;
// - a packet that would wait in the queue past `queue_ms` is dropped at the tail (congestion);
// - `duplicate` delivers a second copy just after the first; `reorder` holds a packet back
//   `reorder_ms` so the ones behind it overtake it.
// Each packet draws exactly five numbers whatever happens to it, so changing one condition moves
// no other condition's draws.
//
// ge:summary A link's conditions (latency, jitter, loss, bursts, reorder, bandwidth) and fates
// ge:keywords net network conditions conditioner latency jitter loss packet-loss burst
// ge:keywords gilbert-elliott duplicate reorder bandwidth queue congestion preset lan wifi
// ge:keywords mobile bad-network simulate fate
// ge:skill net
// ge:test tests/net_conditions_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/prng.hpp"

namespace ge::net {

struct LinkConditions {
  double latency_ms = 0;      // one way
  double jitter_ms = 0;       // added uniform in [0, jitter]
  double loss = 0;            // long-run share dropped, 0..1
  double burst = 1;           // mean run of drops, packets (1: independent)
  double duplicate = 0;       // share delivered twice
  double reorder = 0;         // share held back reorder_ms
  double reorder_ms = 0;      // how far a held packet falls behind
  double bandwidth_kbps = 0;  // the link's rate, kbit/s (0: unlimited)
  double queue_ms = 250;      // the longest a packet may queue before it is dropped
};

// What `decide` keeps between packets on one directed link.
struct LinkState {
  bool bad = false;            // the loss chain's state
  int64_t busy_until_us = 0;   // when the link has clocked out every packet before
  int64_t last_arrive_us = 0;  // the latest arrival of an in-order packet
};

// Bytes of IPv4 and UDP header a datagram carries on the wire: counted against the bandwidth.
constexpr size_t kWireOverhead = 28;

struct Fate {
  int copies = 0;             // 0: lost; 1; 2: duplicated
  bool queue_drop = false;    // lost to the queue rather than the line
  int64_t arrive_us[2] = {};  // when each copy arrives
};

Fate decide(const LinkConditions& c, LinkState& s, core::Rng& rng, int64_t now_us, size_t bytes);

// Named conditions the labs and `ge net` sweep: lan, broadband, wifi, mobile, bad, awful.
const std::vector<std::string>& preset_names();
bool conditions_preset(std::string_view name, LinkConditions& out);
// "40 ms ±10, 1% loss (bursts of 2), 0.5% reorder, 2000 kbit/s": the conditions in words.
std::string describe_conditions(const LinkConditions& c);

}  // namespace ge::net
