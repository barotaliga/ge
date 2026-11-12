// The route finder (routes.hpp).
#include "forward/routes.hpp"

#include <algorithm>
#include <climits>
#include <utility>

namespace ge::forward {

namespace {

net::Address host_of(net::Address a) {
  a.port = 0;
  return a;
}

}  // namespace

Route best_route(const RouteTable& t, const net::Address& server) {
  // Nodes: 0 the player, then every host the table names, in its order.
  std::vector<net::Address> hosts;
  std::vector<int> via;  // per node: the hop of `t.hops` it is (-1: none)
  const auto node = [&](const net::Address& a) {
    const net::Address h = host_of(a);
    for (size_t i = 0; i < hosts.size(); i++)
      if (hosts[i] == h) return static_cast<int>(i) + 1;
    hosts.push_back(h);
    via.push_back(-1);
    return static_cast<int>(hosts.size());
  };
  struct Edge {
    int from, to, ms;
  };
  std::vector<Edge> edges;
  for (const auto& [a, ms] : t.direct)
    if (ms >= 0) edges.push_back({0, node(a), ms});
  for (size_t h = 0; h < t.hops.size(); h++) {
    const int n = node(t.hops[h].forwarder);
    if (via[static_cast<size_t>(n - 1)] < 0) via[static_cast<size_t>(n - 1)] = static_cast<int>(h);
    for (const auto& [a, ms] : t.hops[h].reach)
      if (ms >= 0 && ms < 65535) edges.push_back({n, node(a), ms});
  }
  Route r;
  const int target = node(server);
  for (const auto& [a, ms] : t.direct)
    if (host_of(a) == host_of(server) && ms >= 0) r.direct_ms = ms;
  const size_t count = hosts.size() + 1;
  std::vector<int64_t> dist(count, INT64_MAX);
  std::vector<int> steps(count, INT_MAX), prev(count, -1);
  std::vector<bool> done(count, false);
  dist[0] = 0;
  steps[0] = 0;
  for (;;) {
    int u = -1;
    for (size_t i = 0; i < count; i++) {
      if (done[i] || dist[i] == INT64_MAX) continue;
      if (u < 0 || dist[i] < dist[static_cast<size_t>(u)] ||
          (dist[i] == dist[static_cast<size_t>(u)] && steps[i] < steps[static_cast<size_t>(u)]))
        u = static_cast<int>(i);
    }
    if (u < 0 || u == target) break;
    done[static_cast<size_t>(u)] = true;
    // Only the player and forwarders lead on.
    if (u != 0 && via[static_cast<size_t>(u - 1)] < 0) continue;
    for (const Edge& e : edges) {
      if (e.from != u) continue;
      const auto v = static_cast<size_t>(e.to);
      const int64_t d = dist[static_cast<size_t>(u)] + e.ms;
      const int s = steps[static_cast<size_t>(u)] + 1;
      if (d < dist[v] || (d == dist[v] && s < steps[v])) {
        dist[v] = d;
        steps[v] = s;
        prev[v] = u;
      }
    }
  }
  if (dist[static_cast<size_t>(target)] == INT64_MAX) return r;
  r.found = true;
  r.ping_ms = static_cast<int>(dist[static_cast<size_t>(target)]);
  for (int n = prev[static_cast<size_t>(target)]; n > 0; n = prev[static_cast<size_t>(n)])
    r.forwarders.push_back(t.hops[static_cast<size_t>(via[static_cast<size_t>(n - 1)])].forwarder);
  std::reverse(r.forwarders.begin(), r.forwarders.end());
  return r;
}

std::string route_chain(const Route& r, const net::Address& server) {
  std::string s;
  for (const net::Address& a : r.forwarders) {
    s += net::to_string(a);
    s += '@';
  }
  s += net::to_string(server);
  return s;
}

void route_finder_start(RouteFinder& rf, const RouteQuery& query, net::Transport& transport,
                        const net::ServerList& list, int64_t now_us) {
  rf.query = query;
  rf.transport = &transport;
  rf.asked.clear();
  rf.next_us = now_us;
  if (query.is_forwarder == nullptr) return;
  for (const net::ServerEntry& e : list.entries)
    if (e.answered && query.is_forwarder(e.info)) {
      RouteFinder::Asked a;
      a.forwarder = e.address;
      rf.asked.push_back(std::move(a));
    }
}

void route_finder_update(RouteFinder& rf, int64_t now_us) {
  if (rf.transport == nullptr) return;
  uint8_t buf[net::kMaxDatagram];
  net::Address from;
  int got = 0;
  while ((got = net::transport_receive(*rf.transport, buf, sizeof buf, from, now_us)) >= 0) {
    bool taken = false;
    for (RouteFinder::Asked& a : rf.asked) {
      if (a.answered || host_of(a.forwarder) != host_of(from)) continue;
      Reach reach;
      if (rf.query.read == nullptr || !rf.query.read(buf, static_cast<size_t>(got), reach)) break;
      a.reach = std::move(reach);
      a.answered = true;
      rf.replies++;
      taken = true;
      break;
    }
    if (!taken) rf.foreign++;
  }
  if (now_us < rf.next_us || rf.query.ask == nullptr) return;
  for (RouteFinder::Asked& a : rf.asked) {
    if (a.answered || a.tries >= rf.tries) continue;
    if (a.asked_us >= 0 && now_us - a.asked_us < rf.timeout_us) continue;
    const size_t n = rf.query.ask(buf, sizeof buf);
    if (n == 0) return;
    net::transport_send(*rf.transport, a.forwarder, buf, n, now_us);
    a.tries++;
    a.asked_us = now_us;
    rf.asks++;
    rf.next_us = now_us + rf.gap_us;
    return;  // one ask a gap
  }
}

bool route_finder_done(const RouteFinder& rf, int64_t now_us) {
  for (const RouteFinder::Asked& a : rf.asked) {
    if (a.answered) continue;
    if (a.tries < rf.tries || now_us - a.asked_us < rf.timeout_us) return false;
  }
  return true;
}

RouteTable route_finder_table(const RouteFinder& rf, const net::ServerList& list) {
  RouteTable t;
  for (const net::ServerEntry& e : list.entries)
    if (e.answered && e.ping_ms >= 0) t.direct.emplace_back(e.address, e.ping_ms);
  for (const RouteFinder::Asked& a : rf.asked)
    if (a.answered) t.hops.push_back({a.forwarder, a.reach});
  return t;
}

}  // namespace ge::forward
