// The proxy trial (trial.hpp): the network, the pair, the game's two ends, the counts.
//
// ge:summary The proxy trial's run: network, near and far proxies, the game's ends, the counts
// ge:keywords proxy trial run count
// ge:skill net
// ge:test tests/proxy_test.cpp
#include "proxy/trial.hpp"

#include <algorithm>
#include <vector>

#include "net/sim_network.hpp"
#include "proxy/proxy.hpp"

namespace ge::proxy {

namespace {

// `n` bytes, most of them the same from one tick to the next: a few fields move each tick.
std::vector<uint8_t> datagram(int tick, size_t n) {
  std::vector<uint8_t> d(n);
  for (size_t i = 0; i < n; i++) d[i] = static_cast<uint8_t>(i * 7 + 3);
  for (size_t i = 0; i < n; i += 12) d[i] = static_cast<uint8_t>(tick + static_cast<int>(i));
  d[0] = static_cast<uint8_t>(tick);
  d[1] = static_cast<uint8_t>(tick >> 8);
  return d;
}

}  // namespace

double trial_lost(const TrialResult& r) {
  const int sent = r.sent_up + r.sent_down;
  return sent > 0 ? 1.0 - static_cast<double>(r.got_up + r.got_down) / sent : 1.0;
}

TrialResult run_proxy_trial(const TrialConfig& c) {
  net::LinkConditions clean;
  clean.latency_ms = 1;
  net::SimNetwork sim = net::make_sim_network(c.seed, clean);
  net::LinkConditions slow;
  slow.latency_ms = c.latency_ms;
  slow.loss = c.loss;
  slow.burst = c.burst;
  net::sim_set_path(sim, 0, 1, slow);
  const net::Address near_at = net::sim_address(0, 27000), far_at = net::sim_address(1, 31000);
  const net::Address server_at = net::sim_address(2, 27500);
  net::Transport client, near_client, near_link, far_link, server;
  net::open_sim_transport(client, sim, net::sim_address(0, 5000));
  net::open_sim_transport(near_client, sim, near_at);
  net::open_sim_transport(near_link, sim, net::sim_address(0, 27001));
  net::open_sim_transport(far_link, sim, far_at);
  net::open_sim_transport(server, sim, server_at);
  ProxyConfig pc;
  pc.link.repeat = c.repeat;
  pc.link.pack = c.pack;
  pc.seed = c.seed + 1;
  ProxyNear near;
  proxy_near_init(near, pc, near_client, near_link, far_at, server_at);
  pc.allow = {c.allowed ? server_at : net::sim_address(2, 1)};
  pc.seed = c.seed + 2;
  ProxyFar far;
  uint16_t next_up = 40000;
  proxy_far_init(far, pc, far_link, [&](net::Transport& t) {
    return net::open_sim_transport(t, sim, net::sim_address(1, next_up++));
  });
  TrialResult r;
  int64_t now = 0;
  net::Address client_seen;
  const int64_t start = 1000000, end = start + int64_t{c.seconds} * 1000000;
  uint8_t buf[net::kMaxDatagram];
  uint64_t up0 = 0, down0 = 0;
  for (int64_t t = 0; t < end + 500000; t += 1000) {
    const bool counted = t >= start && t < end;
    if (t == start) {
      for (const auto& k : near.clients) up0 += k->link.stats.bytes_out;
      for (const auto& s : far.sessions) down0 += s->link.stats.bytes_out;
    }
    if (t < end && t % 16000 == 0) {
      const auto cmd = datagram(static_cast<int>(t / 16000), 24);
      net::transport_send(client, near_at, cmd.data(), cmd.size(), now);
      if (counted) {
        r.sent_up++;
        r.payload_up += cmd.size();
      }
    }
    if (counted && t % 50000 == 0 && client_seen.port != 0) {
      const auto snap = datagram(static_cast<int>(t / 50000), 400);
      net::transport_send(server, client_seen, snap.data(), snap.size(), now);
      r.sent_down++;
      r.payload_down += snap.size();
    }
    proxy_near_update(near, now);
    proxy_far_update(far, now);
    now = t + 1000;
    net::sim_advance(sim, now);
    net::Address from;
    int n = 0;
    // Counted by the tick each datagram carries, so one sent before the window is not.
    const auto tick_of = [&] { return int64_t{buf[0]} | int64_t{buf[1]} << 8; };
    while ((n = net::transport_receive(server, buf, sizeof buf, from, now)) >= 0) {
      client_seen = from;
      const int64_t at = tick_of() * 16000;
      if (n >= 2 && at >= start && at < end) r.got_up++;
    }
    while ((n = net::transport_receive(client, buf, sizeof buf, from, now)) >= 0) {
      const int64_t at = tick_of() * 50000;
      if (n >= 2 && at >= start && at < end) r.got_down++;
    }
  }
  for (const auto& k : near.clients) r.link_up += k->link.stats.bytes_out;
  for (const auto& s : far.sessions) r.link_down += s->link.stats.bytes_out;
  r.link_up -= std::min(r.link_up, up0);
  r.link_down -= std::min(r.link_down, down0);
  r.refused = near.last_refusal;
  proxy_far_close(far);
  for (net::Transport* tr : {&client, &near_client, &near_link, &far_link, &server})
    net::close_transport(*tr);
  return r;
}

}  // namespace ge::proxy
