// LOAD (stress.hpp): many sessions in one host process on one simulated network, each host's tick
// timed by the caller's clock.
//
// ge:summary Load: many sessions in one process, each host's update and flush timed a tick
// ge:keywords net stress load sessions host-process step-time run_load tick budget
// ge:skill net
// ge:test tests/net_stress_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>

#include "net/stress.hpp"
#include "net/transport.hpp"

namespace ge::net {

namespace {

struct Side {
  Transport transport;
  Node node;
};

struct Session {
  std::unique_ptr<Side> host;
  std::vector<std::unique_ptr<Side>> clients;
  LoadSession tally;
  std::vector<double> step_us;  // each tick's host update and flush (measured)
};

uint64_t fold(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) {
    h ^= (v >> (8 * i)) & 0xff;
    h *= 0x100000001b3ull;
  }
  return h;
}

// How many of a `hz` stream are due by `now_us` that `sent` has not covered.
uint64_t due(double hz, int64_t now_us, uint64_t sent) {
  const auto by_now = static_cast<uint64_t>(std::floor(static_cast<double>(now_us) * hz / 1e6));
  return by_now > sent ? by_now - sent : 0;
}

double percentile(std::vector<double> v, double q) {
  if (v.empty()) return 0;
  const size_t at = std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size())));
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(at), v.end());
  return v[at];
}

}  // namespace

LoadReport run_load(const LoadPlan& plan) {
  LoadReport rep;
  const int sessions = std::max(1, plan.sessions);
  const int clients = std::clamp(plan.clients, 1, kMaxPeers);
  SimNetwork net = make_sim_network(plan.seed, plan.conditions);
  ChannelConfig channels;
  channels.count = 3;
  channels.delivery[kTrafficInputs] = Delivery::sequenced;
  channels.delivery[kTrafficSnapshots] = Delivery::unreliable;
  channels.delivery[kTrafficEvents] = Delivery::reliable;
  const uint32_t protocol = traffic_protocol();
  // Node numbers: a session's host, then its clients, sessions one after another.
  const auto node_of = [&](int s, int c) {
    return static_cast<uint32_t>(s * (clients + 1) + c + 1);
  };
  std::vector<Session> all(static_cast<size_t>(sessions));
  for (int s = 0; s < sessions; s++) {
    Session& ses = all[static_cast<size_t>(s)];
    ses.host = std::make_unique<Side>();
    open_sim_transport(ses.host->transport, net, sim_address(node_of(s, -1), 1));
    NodeConfig hc;
    hc.protocol = protocol;
    hc.max_peers = clients;
    hc.accept = true;
    hc.channels = channels;
    hc.seed = (plan.seed * 2654435761ull + 1) ^ (static_cast<uint64_t>(s) << 32);
    node_init(ses.host->node, hc, ses.host->transport);
    for (int c = 0; c < clients; c++) {
      ses.clients.push_back(std::make_unique<Side>());
      Side& cl = *ses.clients.back();
      open_sim_transport(cl.transport, net, sim_address(node_of(s, c), 1));
      NodeConfig cc = hc;
      cc.max_peers = 1;
      cc.accept = false;
      cc.seed = hc.seed + static_cast<uint64_t>(c + 1) * 0x9e3779b97f4a7c15ull;
      node_init(cl.node, cc, cl.transport);
    }
  }

  const TrafficMix& mix = plan.mix;
  const double tick_us = 1e6 / plan.tick_hz;
  const int64_t connect_us = 2000000;  // everyone connects in the first two seconds
  const int64_t send_end_us = connect_us + static_cast<int64_t>(std::floor(plan.seconds * 1e6));
  const int64_t end_us = send_end_us + 3000000;  // the drain: every reliable event settles
  uint8_t msg[kMaxMessage] = {};
  const size_t input_bytes = std::clamp<size_t>(mix.input_bytes, 1, kMaxMessage);
  const size_t snapshot_bytes = std::clamp<size_t>(mix.snapshot_bytes, 1, kMaxMessage);
  const size_t event_bytes = std::clamp<size_t>(mix.event_bytes, 1, kMaxMessage);
  std::vector<uint64_t> up_inputs(static_cast<size_t>(sessions * clients)),
      up_events(up_inputs.size()), down_snaps(up_inputs.size()), down_events(up_inputs.size());
  double tick_sum_us = 0;
  uint64_t ticks = 0;

  for (uint64_t tick = 1;; tick++) {
    const auto now = static_cast<int64_t>(std::floor(static_cast<double>(tick) * tick_us));
    if (now > end_us) break;
    sim_advance(net, now);
    const bool sending = now >= connect_us && now < send_end_us;
    const int64_t since = now - connect_us;
    double tick_hosts_us = 0;
    for (int s = 0; s < sessions; s++) {
      Session& ses = all[static_cast<size_t>(s)];
      // Clients: connect once, then send inputs and events.
      for (int c = 0; c < clients; c++) {
        Side& cl = *ses.clients[static_cast<size_t>(c)];
        if (tick == static_cast<uint64_t>(1 + c % 60) && cl.node.peers[0].state == PeerState::free)
          node_connect(cl.node, sim_address(node_of(s, -1), 1), now);
        node_update(cl.node, now);
        for (const Event& e : cl.node.events) {
          if (e.type != EventType::message) continue;
          if (e.channel == kTrafficSnapshots) ses.tally.snapshots_delivered++;
          if (e.channel == kTrafficEvents) ses.tally.events_delivered++;
        }
        const size_t k =
            static_cast<size_t>(s) * static_cast<size_t>(clients) + static_cast<size_t>(c);
        if (sending && peer_connected(cl.node, 0)) {
          for (uint64_t n = due(mix.input_hz, since, up_inputs[k]); n > 0; n--, up_inputs[k]++) {
            if (node_send(cl.node, 0, kTrafficInputs, msg, input_bytes)) ses.tally.inputs_sent++;
          }
          for (uint64_t n = due(mix.event_hz, since, up_events[k]); n > 0; n--, up_events[k]++)
            if (node_send(cl.node, 0, kTrafficEvents, msg, event_bytes)) ses.tally.events_sent++;
        }
        node_flush(cl.node, now);
      }
      // The host: timed from its update to its flush.
      Node& host = ses.host->node;
      const int64_t t0 = plan.clock_ns ? plan.clock_ns() : 0;
      node_update(host, now);
      for (const Event& e : host.events) {
        if (e.type != EventType::message) continue;
        if (e.channel == kTrafficInputs) ses.tally.inputs_delivered++;
        if (e.channel == kTrafficEvents) ses.tally.events_delivered++;
      }
      if (sending) {
        for (int p = 0; p < host.config.max_peers; p++) {
          if (!peer_connected(host, p)) continue;
          const size_t k =
              static_cast<size_t>(s) * static_cast<size_t>(clients) + static_cast<size_t>(p);
          for (uint64_t n = due(mix.snapshot_hz, since, down_snaps[k]); n > 0; n--, down_snaps[k]++)
            if (node_send(host, p, kTrafficSnapshots, msg, snapshot_bytes))
              ses.tally.snapshots_sent++;
          for (uint64_t n = due(mix.event_hz, since, down_events[k]); n > 0; n--, down_events[k]++)
            if (node_send(host, p, kTrafficEvents, msg, event_bytes)) ses.tally.events_sent++;
        }
      }
      node_flush(host, now);
      if (plan.clock_ns && sending) {
        const double us = static_cast<double>(plan.clock_ns() - t0) / 1000.0;
        ses.step_us.push_back(us);
        tick_hosts_us += us;
      }
    }
    if (plan.clock_ns && sending) {
      tick_sum_us += tick_hosts_us;
      ticks++;
    }
  }

  uint64_t h = 0xcbf29ce484222325ull;
  double step_sum = 0;
  size_t step_count = 0;
  bool whole = true;
  size_t held = sim_held_bytes(net);
  for (Session& ses : all) {
    ses.tally.connected = node_connected_count(ses.host->node);
    rep.connected += ses.tally.connected;
    whole = whole && ses.tally.events_delivered == ses.tally.events_sent;
    double sum = 0;
    for (const double us : ses.step_us) sum += us;
    step_sum += sum;
    step_count += ses.step_us.size();
    ses.tally.step_us_mean =
        ses.step_us.empty() ? 0 : sum / static_cast<double>(ses.step_us.size());
    ses.tally.step_us_p99 = percentile(ses.step_us, 0.99);
    ses.tally.step_us_max =
        ses.step_us.empty() ? 0 : *std::max_element(ses.step_us.begin(), ses.step_us.end());
    rep.step_us_p99 = std::max(rep.step_us_p99, ses.tally.step_us_p99);
    held += node_held_bytes(ses.host->node);
    for (const auto& cl : ses.clients) held += node_held_bytes(cl->node);
    for (const uint64_t v :
         {static_cast<uint64_t>(ses.tally.connected), ses.tally.inputs_sent,
          ses.tally.inputs_delivered, ses.tally.snapshots_sent, ses.tally.snapshots_delivered,
          ses.tally.events_sent, ses.tally.events_delivered})
      h = fold(h, v);
    rep.sessions.push_back(ses.tally);
  }
  rep.expected = sessions * clients;
  rep.reliable_whole = whole;
  rep.step_us_mean = step_count ? step_sum / static_cast<double>(step_count) : 0;
  rep.tick_us_mean = ticks ? tick_sum_us / static_cast<double>(ticks) : 0;
  rep.tick_budget_us = tick_us;
  rep.held_kb = static_cast<double>(held) / 1024.0;
  rep.digest = h;
  return rep;
}

}  // namespace ge::net
