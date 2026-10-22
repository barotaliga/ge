// The capture's analysis: two passes over the datagrams, the first to find the protocol most of
// them speak, the second to sum each connection's directions. Sequences are unwrapped per
// direction (each one placed relative to the newest seen), so a capture longer than 2^16 packets
// a way reads the same as a short one.
//
// ge:summary The capture's analysis: sequences unwrapped, acks matched, buckets for the series
// ge:keywords net analyze capture unwrap sequence ack match bucket median
// ge:skill net
// ge:test tests/net_analyze_test.cpp
#include "net/analyze.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>

#include "net/packet.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

// A reliable message seen again within this many packets of a direction is a resend; past it,
// its 16-bit id may have come round again.
constexpr int64_t kResendWindow = 1024;
// The ack bits cover the 32 sequences behind the ack: a sequence further behind the newest ack
// than this is never acked if it is not acked yet.
constexpr int64_t kAckReach = 32;

struct Sent {
  int64_t at_us = 0;
  int bucket = 0;
  bool acked = false;
};

struct DirWork {
  bool any = false;
  int64_t first = 0, newest = 0;  // unwrapped sequences
  bool any_ack = false;
  int64_t max_acked = 0;
  std::unordered_map<int64_t, Sent> sent;
  std::unordered_map<uint32_t, int64_t> reliable;  // channel << 16 | id -> the sequence it was in
};

int64_t unwrap(int64_t newest, uint16_t seq) {
  return newest + seq_diff(seq, static_cast<uint16_t>(newest));
}

int64_t floor_div(int64_t a, int64_t b) { return a / b - (a % b != 0 && (a < 0) != (b < 0)); }

double median_of(std::vector<double>& v) {
  if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

}  // namespace

const DirectionStats& sent_by(const ConnectionStats& c, const Address& local) {
  return c.ba.from == local ? c.ba : c.ab;
}

const DirectionStats& received_by(const ConnectionStats& c, const Address& local) {
  return c.ba.from == local ? c.ab : c.ba;
}

double percentile_of(std::vector<double> v, double p) {
  if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
  std::sort(v.begin(), v.end());
  const double rank = std::ceil(p / 100.0 * static_cast<double>(v.size()));
  const size_t i = rank < 1 ? 0 : static_cast<size_t>(rank) - 1;
  return v[std::min(i, v.size() - 1)];
}

CaptureAnalysis analyze_capture(const std::vector<CapturedDatagram>& datagrams, double bucket_s) {
  CaptureAnalysis a;
  a.bucket_s = bucket_s > 0 ? bucket_s : 0.25;
  a.datagrams = datagrams.size();
  if (datagrams.empty()) return a;

  // The protocol most datagrams speak is the capture's.
  std::map<uint32_t, uint64_t> protocols;
  for (const CapturedDatagram& d : datagrams) {
    Reader r = make_reader(d.data.data(), d.data.size());
    uint32_t protocol = 0;
    PacketType type{};
    if (read_packet_start(r, protocol, type)) protocols[protocol]++;
  }
  uint64_t most = 0;
  for (const auto& [protocol, n] : protocols) {
    if (n > most) {
      most = n;
      a.protocol = protocol;
    }
  }

  const int64_t t0 = datagrams.front().at_us;
  const auto bucket_of = [&](int64_t at_us) {
    const double s = static_cast<double>(at_us - t0) / 1e6;
    return s <= 0 ? 0 : static_cast<int>(s / a.bucket_s);
  };
  const int buckets = bucket_of(datagrams.back().at_us) + 1;

  std::vector<std::array<DirWork, 2>> work;
  std::vector<std::pair<Address, uint64_t>> seen;  // every address and how many datagrams it is in
  const auto count_address = [&](const Address& x) {
    for (auto& [addr, n] : seen) {
      if (addr == x) {
        n++;
        return;
      }
    }
    seen.emplace_back(x, 1);
  };
  // Bytes each way per bucket for every address, picked for the local one at the end.
  std::vector<std::vector<double>> out_bytes, in_bytes;
  const auto address_index = [&](const Address& x) {
    for (size_t i = 0; i < seen.size(); i++) {
      if (seen[i].first == x) return i;
    }
    return seen.size();
  };

  // Ack delays by the bucket the ack arrived in, per sending address.
  std::vector<std::vector<std::vector<double>>> rtt_by_bucket;

  for (const CapturedDatagram& d : datagrams) {
    count_address(d.from);
    count_address(d.to);
    const size_t from_i = address_index(d.from), to_i = address_index(d.to);
    while (out_bytes.size() < seen.size()) {
      out_bytes.emplace_back(static_cast<size_t>(buckets), 0.0);
      in_bytes.emplace_back(static_cast<size_t>(buckets), 0.0);
      rtt_by_bucket.emplace_back(static_cast<size_t>(buckets));
    }
    const int b = bucket_of(d.at_us);
    out_bytes[from_i][static_cast<size_t>(b)] += static_cast<double>(d.data.size());
    in_bytes[to_i][static_cast<size_t>(b)] += static_cast<double>(d.data.size());

    Reader r = make_reader(d.data.data(), d.data.size());
    uint32_t protocol = 0;
    PacketType type{};
    if (!read_packet_start(r, protocol, type) || protocol != a.protocol) {
      a.foreign++;
      continue;
    }

    // The connection and the direction.
    size_t ci = a.connections.size();
    int dir = 0;
    for (size_t i = 0; i < a.connections.size(); i++) {
      const ConnectionStats& c = a.connections[i];
      if (c.ab.from == d.from && c.ab.to == d.to) {
        ci = i;
        dir = 0;
        break;
      }
      if (c.ba.from == d.from && c.ba.to == d.to) {
        ci = i;
        dir = 1;
        break;
      }
    }
    if (ci == a.connections.size()) {
      ConnectionStats c;
      c.ab.from = c.ba.to = d.from;
      c.ab.to = c.ba.from = d.to;
      a.connections.push_back(c);
      work.emplace_back();
    }
    ConnectionStats& conn = a.connections[ci];
    DirectionStats& s = dir == 0 ? conn.ab : conn.ba;
    DirWork& w = work[ci][static_cast<size_t>(dir)];
    DirWork& back = work[ci][static_cast<size_t>(1 - dir)];
    DirectionStats& back_s = dir == 0 ? conn.ba : conn.ab;

    s.packets++;
    s.bytes += d.data.size();
    s.by_type[static_cast<size_t>(type) & 7]++;
    if (s.first_us < 0) s.first_us = d.at_us;
    s.last_us = d.at_us;
    if (type != PacketType::payload) continue;

    PayloadHeader h;
    if (!read_payload_header(r, h)) continue;

    // This packet's own sequence.
    const int64_t ext = w.any ? unwrap(w.newest, h.seq) : h.seq;
    bool fresh = true;
    if (!w.any) {
      w.any = true;
      w.first = w.newest = ext;
    } else if (w.sent.count(ext)) {
      s.duplicated++;
      fresh = false;
    } else if (ext < w.newest) {
      s.reordered++;
    }
    if (ext > w.newest) w.newest = ext;
    if (fresh) w.sent[ext] = Sent{d.at_us, b, false};

    // Its acks settle the other direction's sends. Sequence 0 is never sent: ack 0 acks nothing.
    if (h.ack != 0 && back.any) {
      const int64_t acked = unwrap(back.newest, h.ack);
      for (int i = -1; i < 32; i++) {
        if (i >= 0 && !(h.ack_bits >> i & 1u)) continue;
        const int64_t q = acked - 1 - i;
        auto it = back.sent.find(q);
        if (it == back.sent.end() || it->second.acked) continue;
        it->second.acked = true;
        back_s.acked++;
        const double delay = static_cast<double>(d.at_us - it->second.at_us) / 1000.0;
        back_s.ack_delay_ms.push_back(delay);
        rtt_by_bucket[to_i][static_cast<size_t>(b)].push_back(delay);
      }
      if (!back.any_ack || acked > back.max_acked) back.max_acked = acked;
      back.any_ack = true;
    }

    // Its messages.
    MessageView m;
    bool any_message = false;
    while (read_message(r, m)) {
      any_message = true;
      s.messages++;
      s.message_bytes += m.size;
      if (!fresh || !m.has_id) continue;
      const uint32_t key = static_cast<uint32_t>(m.channel) << 16 | m.id;
      auto it = w.reliable.find(key);
      if (it != w.reliable.end() && ext - it->second < kResendWindow && ext != it->second) {
        s.resends++;
      }
      w.reliable[key] = ext;
    }
    if (!any_message) s.empty++;
  }

  // Which address took the capture: the one in every datagram.
  std::vector<Address> everywhere;
  for (const auto& [addr, n] : seen) {
    if (n == a.datagrams) everywhere.push_back(addr);
  }
  if (everywhere.size() == 1) {
    a.local = everywhere[0];
    a.local_known = true;
  } else if (everywhere.size() == 2 && a.connections.size() == 1) {
    // One connection: the capturing side waits a round trip for its acks, the other a turnaround.
    std::vector<double> ab = a.connections[0].ab.ack_delay_ms;
    std::vector<double> ba = a.connections[0].ba.ack_delay_ms;
    if (!ab.empty() && !ba.empty()) {
      a.local =
          median_of(ab) >= median_of(ba) ? a.connections[0].ab.from : a.connections[0].ba.from;
      a.local_known = true;
    }
  }

  // Loss: what the acks settled.
  std::vector<double> settled(static_cast<size_t>(buckets), 0.0),
      lost(static_cast<size_t>(buckets), 0.0);
  for (size_t ci = 0; ci < a.connections.size(); ci++) {
    for (int dir = 0; dir < 2; dir++) {
      DirectionStats& s = dir == 0 ? a.connections[ci].ab : a.connections[ci].ba;
      const DirWork& w = work[ci][static_cast<size_t>(dir)];
      if (w.any) {
        // Sequence 0 is never sent, so each wrap the span crosses is one number short.
        const int64_t zeros = floor_div(w.newest, 65536) - floor_div(w.first - 1, 65536);
        const int64_t span = w.newest - w.first + 1 - zeros;
        const auto unique = static_cast<int64_t>(w.sent.size());
        s.missing = span > unique ? static_cast<uint64_t>(span - unique) : 0;
      }
      const bool local_sends = a.local_known && s.from == a.local;
      uint64_t lost_n = 0;
      for (const auto& [ext, sent] : w.sent) {
        const bool gone = !sent.acked && w.any_ack && ext < w.max_acked - kAckReach;
        if (!sent.acked && !gone) continue;
        s.settled++;
        if (gone) lost_n++;
        if (local_sends) {
          settled[static_cast<size_t>(sent.bucket)] += 1;
          if (gone) lost[static_cast<size_t>(sent.bucket)] += 1;
        }
      }
      s.loss = s.settled ? static_cast<double>(lost_n) / static_cast<double>(s.settled) : 0;
      // Toward the capturing node a packet lost on the way never reaches the capture at all:
      // the gaps in its sequences are the loss.
      if (a.local_known && s.to == a.local && w.any) {
        s.settled = w.sent.size() + s.missing;
        s.loss = static_cast<double>(s.missing) / static_cast<double>(s.settled);
      }
    }
  }

  if (a.local_known) {
    const size_t li = address_index(a.local);
    for (int b = 0; b < buckets; b++) {
      const auto i = static_cast<size_t>(b);
      a.t_s.push_back(static_cast<double>(b) * a.bucket_s);
      a.out_kbps.push_back(out_bytes[li][i] * 8.0 / 1000.0 / a.bucket_s);
      a.in_kbps.push_back(in_bytes[li][i] * 8.0 / 1000.0 / a.bucket_s);
      a.rtt_ms.push_back(median_of(rtt_by_bucket[li][i]));
      a.loss_pct.push_back(settled[i] > 0 ? 100.0 * lost[i] / settled[i]
                                          : std::numeric_limits<double>::quiet_NaN());
    }
  }
  return a;
}

}  // namespace ge::net
