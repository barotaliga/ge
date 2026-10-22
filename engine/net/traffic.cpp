// The fake-traffic session: the nodes, the clock, the streams and their tallies, the series and
// the digest. Each message is [kind u8][client u16][count u32][sent_us u64] then filler bytes, so
// the receiver checks its stream's order and measures its latency on the shared virtual clock.
//
// ge:summary The fake-traffic session's loop, its streams' tallies, series and digest
// ge:keywords net traffic session loop tally latency digest series
// ge:skill net
// ge:test tests/net_traffic_test.cpp
#include "net/traffic.hpp"

#include <cmath>
#include <memory>

#include "core/num_text.hpp"
#include "net/node.hpp"

namespace ge::net {

namespace {

enum Kind : uint8_t { kInput = 1, kSnapshot = 2, kEventUp = 3, kEventDown = 4 };
constexpr size_t kHeader = 1 + 2 + 4 + 8;

struct Fnv {
  uint64_t h = 0xcbf29ce484222325ull;
  void bytes(const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; i++) {
      h ^= b[i];
      h *= 0x100000001b3ull;
    }
  }
  template <class T>
  void value(T v) {
    bytes(&v, sizeof v);
  }
};

struct Stream {
  uint32_t next_send = 0;
  uint32_t expect = 0;
};

struct Side {
  Transport transport;
  Node node;
  int64_t connected_us = -1;
  bool gone = false;  // denied, failed or dropped: sends nothing more
  Stream inputs, events_up, snapshots, events_down;  // up streams sent here, down received here
};

void make_message(uint8_t* out, size_t size, Kind kind, int client, uint32_t count, int64_t now) {
  Writer w = make_writer(out, size);
  write_u8(w, kind);
  write_u16(w, static_cast<uint16_t>(client));
  write_u32(w, count);
  write_u64(w, static_cast<uint64_t>(now));
  for (size_t i = kHeader; i < size; i++) out[i] = static_cast<uint8_t>(count + i);
}

// How many of a stream are due `elapsed_us` after it started at `hz`.
uint32_t due(int64_t elapsed_us, double hz) {
  if (elapsed_us < 0 || hz <= 0) return 0;
  return static_cast<uint32_t>(std::floor(static_cast<double>(elapsed_us) * hz / 1e6)) + 1;
}

void tally(StreamTally& t, Stream& s, uint32_t count, int64_t sent_us, int64_t now_us,
           bool ordered) {
  if (ordered) {
    if (count < s.expect) {
      t.duplicated++;
      return;
    }
    if (count > s.expect) t.out_of_order++;
    s.expect = count + 1;
  }
  t.delivered++;
  t.latency_ms.push_back(static_cast<double>(now_us - sent_us) / 1000.0);
}

}  // namespace

TrafficReport run_traffic(const TrafficPlan& plan) {
  TrafficReport rep;
  const int n = plan.clients < 0 ? 0 : plan.clients;
  const uint32_t protocol = protocol_id("ge-net-traffic", 1);
  ChannelConfig channels;
  channels.count = 3;
  channels.delivery[kTrafficInputs] = Delivery::sequenced;
  channels.delivery[kTrafficSnapshots] = Delivery::unreliable;
  channels.delivery[kTrafficEvents] = Delivery::reliable;

  SimNetwork net = make_sim_network(plan.seed, plan.conditions);
  auto host = std::make_unique<Side>();
  std::vector<std::unique_ptr<Side>> clients;
  if (plan.capture) {
    rep.capture = make_capture();
    host->transport.capture = &rep.capture;
  }
  open_sim_transport(host->transport, net, sim_address(0, 1));
  NodeConfig hc;
  hc.protocol = protocol;
  hc.max_peers = plan.max_peers;
  hc.accept = true;
  hc.channels = channels;
  hc.seed = plan.seed * 2654435761ull + 1;
  node_init(host->node, hc, host->transport);
  for (int i = 0; i < n; i++) {
    clients.push_back(std::make_unique<Side>());
    Side& c = *clients.back();
    open_sim_transport(c.transport, net, sim_address(static_cast<uint32_t>(i + 1), 1));
    NodeConfig cc = hc;
    cc.max_peers = 1;
    cc.accept = false;
    cc.seed = hc.seed + static_cast<uint64_t>(i + 1) * 0x9e3779b97f4a7c15ull;
    node_init(c.node, cc, c.transport);
  }
  std::vector<int> client_of_peer(static_cast<size_t>(host->node.config.max_peers), -1);
  std::vector<Stream> host_up_inputs(static_cast<size_t>(n)),
      host_up_events(static_cast<size_t>(n));
  std::vector<Stream> host_down_snaps(static_cast<size_t>(n)),
      host_down_events(static_cast<size_t>(n));
  std::vector<int64_t> host_known_us(static_cast<size_t>(n), -1);

  const double tick_us = 1e6 / plan.tick_hz;
  const int64_t send_end_us = static_cast<int64_t>(std::floor(plan.seconds * 1e6));
  const int64_t end_us = send_end_us + static_cast<int64_t>(std::floor(plan.drain_seconds * 1e6));
  const int64_t sample_us = static_cast<int64_t>(std::floor(plan.sample_s * 1e6));
  int64_t next_sample = 0;
  bool summed = false;
  Fnv fnv;
  uint8_t msg[kMaxMessage];

  const auto host_means = [&](double& rtt, double& var, double& jit, double& loss) {
    rtt = var = jit = loss = 0;
    int k = 0;
    for (const Peer& p : host->node.peers) {
      if (p.state != PeerState::connected || p.conn.stats.rtt_samples == 0) continue;
      rtt += p.conn.stats.rtt_ms;
      var += p.conn.stats.rtt_var_ms;
      jit += p.conn.stats.jitter_ms;
      loss += p.conn.stats.loss;
      k++;
    }
    if (k) {
      rtt /= k;
      var /= k;
      jit /= k;
      loss /= k;
    }
  };

  for (int64_t tick = 0;; tick++) {
    const auto now = static_cast<int64_t>(std::floor(static_cast<double>(tick) * tick_us));
    if (now > end_us) break;
    const bool sending = now < send_end_us;
    sim_advance(net, now);
    if (tick < n) node_connect(clients[static_cast<size_t>(tick)]->node, sim_address(0, 1), now);

    // The host's events.
    node_update(host->node, now);
    for (const Event& e : host->node.events) {
      fnv.value(uint8_t{0});
      fnv.value(static_cast<uint8_t>(e.type));
      fnv.value(e.peer);
      if (e.type == EventType::connected || e.type == EventType::disconnected) {
        client_of_peer[static_cast<size_t>(e.peer)] = -1;
        continue;
      }
      if (e.type != EventType::message || e.size < kHeader) continue;
      const uint8_t* d = event_data(host->node, e);
      fnv.bytes(d, e.size);
      Reader r = make_reader(d, e.size);
      const uint8_t kind = read_u8(r);
      const int client = read_u16(r);
      const uint32_t count = read_u32(r);
      const auto sent = static_cast<int64_t>(read_u64(r));
      if (client >= n) continue;
      client_of_peer[static_cast<size_t>(e.peer)] = client;
      if (host_known_us[static_cast<size_t>(client)] < 0)
        host_known_us[static_cast<size_t>(client)] = now;
      if (kind == kInput) {
        tally(rep.inputs, host_up_inputs[static_cast<size_t>(client)], count, sent, now, false);
      } else if (kind == kEventUp) {
        tally(rep.events_up, host_up_events[static_cast<size_t>(client)], count, sent, now, true);
      }
    }

    // Each client's events.
    for (int i = 0; i < n; i++) {
      Side& c = *clients[static_cast<size_t>(i)];
      node_update(c.node, now);
      for (const Event& e : c.node.events) {
        fnv.value(static_cast<uint8_t>(i + 1));
        fnv.value(static_cast<uint8_t>(e.type));
        fnv.value(static_cast<uint8_t>(e.reason));
        switch (e.type) {
          case EventType::connected:
            c.connected_us = now;
            rep.connected++;
            break;
          case EventType::connect_failed:
            c.gone = true;
            rep.denied += e.reason == Reason::denied_full;
            break;
          case EventType::disconnected:
            c.gone = true;
            rep.dropped++;
            break;
          case EventType::message: {
            if (e.size < kHeader) break;
            const uint8_t* d = event_data(c.node, e);
            fnv.bytes(d, e.size);
            Reader r = make_reader(d, e.size);
            const uint8_t kind = read_u8(r);
            read_u16(r);
            const uint32_t count = read_u32(r);
            const auto sent = static_cast<int64_t>(read_u64(r));
            if (kind == kSnapshot) tally(rep.snapshots, c.snapshots, count, sent, now, false);
            if (kind == kEventDown) tally(rep.events_down, c.events_down, count, sent, now, true);
            break;
          }
        }
      }
      // What this client sends now.
      if (!sending || c.gone || c.connected_us < 0) continue;
      const int64_t elapsed = now - c.connected_us;
      for (uint32_t k = c.inputs.next_send; k < due(elapsed, plan.mix.input_hz); k++) {
        const size_t size = plan.mix.input_bytes < kHeader ? kHeader : plan.mix.input_bytes;
        make_message(msg, size, kInput, i, k, now);
        node_send(c.node, 0, kTrafficInputs, msg, size) ? rep.inputs.sent++ : rep.inputs.refused++;
        c.inputs.next_send = k + 1;
      }
      for (uint32_t k = c.events_up.next_send; k < due(elapsed, plan.mix.event_hz); k++) {
        const size_t size = plan.mix.event_bytes < kHeader ? kHeader : plan.mix.event_bytes;
        make_message(msg, size, kEventUp, i, c.events_up.next_send, now);
        if (!node_send(c.node, 0, kTrafficEvents, msg, size)) {
          rep.events_up.refused++;
          break;  // the window is full: the same count is tried again next tick
        }
        rep.events_up.sent++;
        c.events_up.next_send++;
      }
    }

    // What the host sends each client it knows.
    if (sending) {
      for (int peer = 0; peer < static_cast<int>(client_of_peer.size()); peer++) {
        const int client = client_of_peer[static_cast<size_t>(peer)];
        if (client < 0 || !peer_connected(host->node, peer)) continue;
        const int64_t elapsed = now - host_known_us[static_cast<size_t>(client)];
        Stream& snaps = host_down_snaps[static_cast<size_t>(client)];
        Stream& events = host_down_events[static_cast<size_t>(client)];
        for (uint32_t k = snaps.next_send; k < due(elapsed, plan.mix.snapshot_hz); k++) {
          const size_t size = plan.mix.snapshot_bytes < kHeader ? kHeader : plan.mix.snapshot_bytes;
          make_message(msg, size, kSnapshot, client, k, now);
          node_send(host->node, peer, kTrafficSnapshots, msg, size) ? rep.snapshots.sent++
                                                                    : rep.snapshots.refused++;
          snaps.next_send = k + 1;
        }
        for (uint32_t k = events.next_send; k < due(elapsed, plan.mix.event_hz); k++) {
          const size_t size = plan.mix.event_bytes < kHeader ? kHeader : plan.mix.event_bytes;
          make_message(msg, size, kEventDown, client, events.next_send, now);
          if (!node_send(host->node, peer, kTrafficEvents, msg, size)) {
            rep.events_down.refused++;
            break;
          }
          rep.events_down.sent++;
          events.next_send++;
        }
      }
    }

    node_flush(host->node, now);
    for (auto& c : clients) node_flush(c->node, now);

    if (now >= next_sample) {
      double rtt, var, jit, loss;
      host_means(rtt, var, jit, loss);
      rep.t_s.push_back(static_cast<double>(now) / 1e6);
      rep.series_rtt_ms.push_back(rtt);
      rep.series_loss.push_back(loss);
      next_sample += sample_us;
    }
    if (!sending && !summed) {
      summed = true;
      host_means(rep.rtt_ms, rep.rtt_var_ms, rep.jitter_ms, rep.loss);
      uint64_t sent = 0, recv = 0, lost = 0, acked = 0;
      for (const Peer& p : host->node.peers) {
        sent += p.conn.stats.bytes_sent;
        recv += p.conn.stats.bytes_received;
        lost += p.conn.stats.packets_lost;
        acked += p.conn.stats.packets_acked;
      }
      rep.loss_total =
          lost + acked ? static_cast<double>(lost) / static_cast<double>(lost + acked) : 0;
      rep.host_send_kbps = static_cast<double>(sent * 8) / (plan.seconds * 1000.0);
      rep.host_recv_kbps = static_cast<double>(recv * 8) / (plan.seconds * 1000.0);
    }
  }

  for (const Peer& p : host->node.peers) {
    rep.resends += p.conn.stats.resends;
    rep.dropped_unsent += p.conn.stats.dropped_unsent;
    rep.duplicates += p.conn.stats.duplicates;
    rep.stale += p.conn.stats.stale;
  }
  for (const auto& c : clients) {
    const LinkStats& s = c->node.peers[0].conn.stats;
    rep.resends += s.resends;
    rep.dropped_unsent += s.dropped_unsent;
    rep.duplicates += s.duplicates;
    rep.stale += s.stale;
  }
  rep.wire = net.stats;
  fnv.value(rep.wire.sent);
  fnv.value(rep.wire.delivered);
  fnv.value(rep.resends);
  rep.digest = fnv.h;
  host->transport.capture = nullptr;
  return rep;
}

namespace {

struct NamedMix {
  const char* name;
  TrafficMix mix;
};

const NamedMix kMixes[] = {
    {"racing", {60, 24, 20, 320, 4, 48}},  {"action", {60, 32, 30, 700, 8, 32}},
    {"rpg", {20, 16, 10, 400, 10, 64}},    {"world", {10, 24, 10, 1000, 20, 96}},
    {"lockstep", {10, 16, 0, 320, 2, 64}},
};

}  // namespace

const std::vector<std::string>& mix_names() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> n;
    for (const NamedMix& m : kMixes) n.emplace_back(m.name);
    return n;
  }();
  return names;
}

bool traffic_mix(std::string_view name, TrafficMix& out) {
  for (const NamedMix& m : kMixes) {
    if (name == m.name) {
      out = m.mix;
      return true;
    }
  }
  return false;
}

std::string describe_mix(const TrafficMix& m) {
  const auto one = [](const char* what, double hz, size_t bytes) {
    if (hz <= 0) return std::string("no ") + what;
    return std::string(what) + " " + core::num::to_string(hz) + " Hz x " + std::to_string(bytes) +
           " B";
  };
  return one("inputs", m.input_hz, m.input_bytes) + ", " +
         one("snapshots", m.snapshot_hz, m.snapshot_bytes) + ", " +
         one("events", m.event_hz, m.event_bytes);
}

}  // namespace ge::net
