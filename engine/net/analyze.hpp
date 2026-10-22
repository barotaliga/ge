// A CAPTURE READ BACK AS NUMBERS: what `ge net stats` answers and the tests hold. Every datagram
// of a capture (capture.hpp) is parsed with the engine's own wire format (packet.hpp) and summed
// per CONNECTION (an unordered pair of addresses) and per DIRECTION of it:
//
// - packets, bytes and messages, each packet type counted (the handshake is in them);
// - from the payload headers: sequences seen twice (duplicated), older than the newest seen
//   (reordered), missing between the first and the newest (lost before the capture point);
// - from the other direction's acks: each payload ACKED, and the time from its capture to the
//   first packet back that acks it (the ack delay). A sequence 32 or more behind the newest ack
//   that never was acked is lost: the ack bits will never cover it again;
// - a reliable message seen again in a later packet: a resend.
//
// LOSS each way: of the capturing node's sends, those the acks never settled; of what it
// receives, the gaps in the sequences (a packet lost on the way never reaches the capture).
//
// WHERE THE CAPTURE WAS TAKEN decides what the ack delay is. At the capturing node (`local`) the
// ack delay of its own sends is its round trip, the tick it polls on included; of the remote's
// sends, the local node's own turnaround. `local` is the address in every datagram; with one
// connection it is the side whose sends wait longer for their acks.
//
// The series (one row per `bucket_s`) is the local node's: kilobits a second each way, the
// median round trip and the share of its sends lost, for `ge graph`.
//
// ge:summary A capture analyzed: per connection and direction packets, bytes, rtt, loss, resends
// ge:keywords net analyze capture pcap stats rtt round-trip loss packet-loss bandwidth kbps
// ge:keywords resend duplicate reorder ack-delay connection direction series inspect
// ge:skill net
// ge:test tests/net_analyze_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/address.hpp"
#include "net/capture.hpp"

namespace ge::net {

struct DirectionStats {
  Address from, to;
  uint64_t packets = 0, bytes = 0;  // bytes: the datagrams' own, no IP or UDP header
  uint64_t by_type[8] = {};         // indexed by PacketType
  uint64_t messages = 0, message_bytes = 0;
  uint64_t empty = 0;  // payloads with no message: keepalives
  uint64_t duplicated = 0, reordered = 0, missing = 0;
  uint64_t resends = 0;
  uint64_t acked = 0, settled = 0;  // of the payloads sent this way, by the other way's acks
  double loss = 0;  // from the local node: unacked of settled; toward it: missing of the span
  std::vector<double> ack_delay_ms;  // one a payload acked, in the order the acks came
  int64_t first_us = -1, last_us = -1;
};

struct ConnectionStats {
  DirectionStats ab, ba;  // a is the first address seen in it
};

struct CaptureAnalysis {
  uint64_t datagrams = 0;
  uint64_t foreign = 0;  // not a packet of the capture's protocol (the most common one)
  uint32_t protocol = 0;
  bool local_known = false;
  Address local;
  std::vector<ConnectionStats> connections;  // in the order they first appear
  double bucket_s = 0.25;
  // The local node's, one row a bucket from the first datagram: t_s is the bucket's start.
  std::vector<double> t_s, out_kbps, in_kbps, rtt_ms, loss_pct;
};

CaptureAnalysis analyze_capture(const std::vector<CapturedDatagram>& datagrams,
                                double bucket_s = 0.25);

// The direction of `c` that `local` sends on, or `ab` when it is in neither.
const DirectionStats& sent_by(const ConnectionStats& c, const Address& local);
const DirectionStats& received_by(const ConnectionStats& c, const Address& local);

// The p-th percentile (0..100) of `v`, by nearest rank; NaN when empty.
double percentile_of(std::vector<double> v, double p);

}  // namespace ge::net
