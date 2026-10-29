// The simulated network's queue: datagrams in flight in a min-heap on (arrival, send order),
// held in a slab of slots reused through a free list, delivered into each socket's inbox.
//
// ge:summary The simulated network's in-flight heap, slot slab and socket inboxes
// ge:keywords net sim-network heap deliver inbox slab
// ge:skill net
// ge:test tests/net_sim_network_test.cpp
#include "net/sim_network.hpp"

#include <algorithm>
#include <cstring>

namespace ge::net {

namespace {

bool later(const SimNetwork::Pending& a, const SimNetwork::Pending& b) {
  return a.at_us != b.at_us ? a.at_us > b.at_us : a.order > b.order;
}

uint64_t link_key(uint32_t from, uint32_t to) { return static_cast<uint64_t>(from) << 32 | to; }

SimNetwork::Socket* find_socket(SimNetwork& net, const Address& a) {
  for (SimNetwork::Socket& s : net.sockets) {
    if (s.address == a) return &s;
  }
  return nullptr;
}

uint32_t take_slot(SimNetwork& net) {
  if (!net.free_slots.empty()) {
    const uint32_t s = net.free_slots.back();
    net.free_slots.pop_back();
    return s;
  }
  net.store.emplace_back();
  return static_cast<uint32_t>(net.store.size() - 1);
}

bool has(const std::vector<uint32_t>& nodes, uint32_t n) {
  return std::find(nodes.begin(), nodes.end(), n) != nodes.end();
}

bool alive(const SimNat& nat, const SimNat::Mapping& m, int64_t now_us) {
  return now_us - m.last_out_us <= nat.config.mapping_us;
}

bool port_taken(const SimNat& nat, uint16_t port, int64_t now_us) {
  for (const SimNat::Mapping& m : nat.maps) {
    if (m.port == port && alive(nat, m, now_us)) return true;
  }
  for (const SimNat::Forward& f : nat.forwards) {
    if (f.port == port && f.until_us > now_us) return true;
  }
  return false;
}

// The public address `from` leaves by toward `to`, the mapping made or refreshed.
Address map_out(SimNat& nat, const Address& from, const Address& to, int64_t now_us) {
  for (const SimNat::Forward& f : nat.forwards) {
    if (f.inside == from && f.until_us > now_us) return sim_address(nat.config.public_node, f.port);
  }
  const bool symmetric = nat.config.kind == NatKind::symmetric;
  SimNat::Mapping* m = nullptr;
  for (SimNat::Mapping& k : nat.maps) {
    if (k.inside == from && (!symmetric || k.remote == to) && alive(nat, k, now_us)) {
      m = &k;
      break;
    }
  }
  if (!m) {
    // Drop the dead ones first, so the list stays as long as the live mappings.
    nat.maps.erase(std::remove_if(nat.maps.begin(), nat.maps.end(),
                                  [&](const SimNat::Mapping& k) { return !alive(nat, k, now_us); }),
                   nat.maps.end());
    SimNat::Mapping fresh;
    fresh.inside = from;
    fresh.remote = to;
    do {
      fresh.port = nat.next_port++;
      if (nat.next_port < nat.config.first_port) nat.next_port = nat.config.first_port;
    } while (port_taken(nat, fresh.port, now_us));
    nat.maps.push_back(fresh);
    m = &nat.maps.back();
  }
  m->last_out_us = now_us;
  if (std::find(m->sent_to.begin(), m->sent_to.end(), to) == m->sent_to.end())
    m->sent_to.push_back(to);
  return sim_address(nat.config.public_node, m->port);
}

// Where a datagram from `from` to the NAT's public port goes inside, or none (refused).
bool map_in(const SimNat& nat, const Address& from, uint16_t port, int64_t now_us, Address& to) {
  for (const SimNat::Forward& f : nat.forwards) {
    if (f.port == port && f.until_us > now_us) {
      to = f.inside;
      return true;
    }
  }
  for (const SimNat::Mapping& m : nat.maps) {
    if (m.port != port || !alive(nat, m, now_us)) continue;
    bool pass = false;
    switch (nat.config.kind) {
      case NatKind::full_cone:
        pass = true;
        break;
      case NatKind::restricted:
        for (const Address& a : m.sent_to) pass |= sim_node(a) == sim_node(from);
        break;
      case NatKind::port_restricted:
        for (const Address& a : m.sent_to) pass |= a == from;
        break;
      case NatKind::symmetric:
        pass = m.remote == from;
        break;
    }
    if (!pass) return false;
    to = m.inside;
    return true;
  }
  return false;
}

}  // namespace

const char* nat_kind_name(NatKind k) {
  switch (k) {
    case NatKind::full_cone:
      return "full-cone";
    case NatKind::restricted:
      return "restricted";
    case NatKind::port_restricted:
      return "port-restricted";
    case NatKind::symmetric:
      return "symmetric";
  }
  return "?";
}

bool nat_kind(std::string_view name, NatKind& out) {
  for (NatKind k :
       {NatKind::full_cone, NatKind::restricted, NatKind::port_restricted, NatKind::symmetric}) {
    if (name == nat_kind_name(k)) {
      out = k;
      return true;
    }
  }
  return false;
}

int sim_add_nat(SimNetwork& net, const NatConfig& config) {
  SimNat nat;
  nat.config = config;
  nat.next_port = config.first_port;
  net.nats.push_back(std::move(nat));
  return static_cast<int>(net.nats.size()) - 1;
}

bool sim_nat_forward(SimNetwork& net, int nat, const Address& inside, uint16_t public_port,
                     int64_t until_us) {
  if (nat < 0 || nat >= static_cast<int>(net.nats.size())) return false;
  SimNat& n = net.nats[static_cast<size_t>(nat)];
  for (SimNat::Forward& f : n.forwards) {
    if (f.port == public_port && f.inside == inside) {
      f.until_us = until_us;
      return true;
    }
  }
  if (port_taken(n, public_port, net.now_us)) return false;
  n.forwards.push_back({public_port, inside, until_us});
  return true;
}

bool sim_nat_unforward(SimNetwork& net, int nat, uint16_t public_port) {
  if (nat < 0 || nat >= static_cast<int>(net.nats.size())) return false;
  auto& fs = net.nats[static_cast<size_t>(nat)].forwards;
  for (size_t i = 0; i < fs.size(); i++) {
    if (fs[i].port == public_port) {
      fs.erase(fs.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

int sim_nat_of(const SimNetwork& net, uint32_t node) {
  for (size_t i = 0; i < net.nats.size(); i++) {
    if (has(net.nats[i].config.inside, node)) return static_cast<int>(i);
  }
  return -1;
}

namespace {

// The LAN a node is on: its NAT's (the router's own LAN face counted in it), or -1 for the open
// network, where every node without a NAT sits.
int lan_of(const SimNetwork& net, uint32_t node) {
  for (size_t i = 0; i < net.nats.size(); i++) {
    if (has(net.nats[i].config.inside, node) || net.nats[i].config.gateway == node)
      return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

SimNetwork make_sim_network(uint32_t seed, const LinkConditions& defaults) {
  SimNetwork net;
  net.rng = core::make_rng(seed);
  net.defaults = defaults;
  net.store.reserve(1024);
  net.heap.reserve(1024);
  return net;
}

bool sim_bind(SimNetwork& net, const Address& address) {
  if (address.family != Family::sim || find_socket(net, address)) return false;
  net.sockets.push_back({address, {}, 0});
  net.sockets.back().inbox.reserve(256);
  return true;
}

void sim_unbind(SimNetwork& net, const Address& address) {
  for (size_t i = 0; i < net.sockets.size(); i++) {
    if (net.sockets[i].address == address) {
      const auto& s = net.sockets[i];
      for (size_t k = s.head; k < s.inbox.size(); k++) net.free_slots.push_back(s.inbox[k]);
      net.sockets.erase(net.sockets.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }
}

void sim_set_link(SimNetwork& net, uint32_t from_node, uint32_t to_node, const LinkConditions& c) {
  net.links[link_key(from_node, to_node)].conditions = c;
}

void sim_set_path(SimNetwork& net, uint32_t a, uint32_t b, const LinkConditions& c) {
  sim_set_link(net, a, b, c);
  sim_set_link(net, b, a, c);
}

const LinkConditions& sim_link(const SimNetwork& net, uint32_t from_node, uint32_t to_node) {
  const auto it = net.links.find(link_key(from_node, to_node));
  return it == net.links.end() ? net.defaults : it->second.conditions;
}

bool sim_send(SimNetwork& net, const Address& from, const Address& to, const uint8_t* data,
              size_t size) {
  if (size > kMaxDatagram) return false;
  if (sim_node(to) == kSimBroadcast) {
    // Every socket on the port but the sender's, on the sender's own LAN (a router does not
    // forward a broadcast); the list cannot change while it is walked.
    const int lan = net.nats.empty() ? -1 : lan_of(net, sim_node(from));
    for (size_t i = 0; i < net.sockets.size(); i++) {
      const Address at = net.sockets[i].address;
      if (at.port != to.port || at == from) continue;
      if (!net.nats.empty() && lan_of(net, sim_node(at)) != lan) continue;
      sim_send(net, from, at, data, size);
    }
    return true;
  }
  // Behind a NAT and leaving it: the datagram goes out from the NAT's public address. The link
  // is still the inside node's own.
  Address source = from;
  if (!net.nats.empty()) {
    const int n = sim_nat_of(net, sim_node(from));
    if (n >= 0) {
      SimNat& nat = net.nats[static_cast<size_t>(n)];
      if (!has(nat.config.inside, sim_node(to)) && sim_node(to) != nat.config.gateway)
        source = map_out(nat, from, to, net.now_us);
    }
  }
  auto it = net.links.find(link_key(sim_node(from), sim_node(to)));
  if (it == net.links.end()) {
    it = net.links
             .emplace(link_key(sim_node(from), sim_node(to)), SimNetwork::Link{net.defaults, {}})
             .first;
  }
  net.stats.sent++;
  net.stats.bytes_sent += size;
  const Fate f = decide(it->second.conditions, it->second.state, net.rng, net.now_us, size);
  if (f.copies == 0) {
    (f.queue_drop ? net.stats.queue_dropped : net.stats.lost)++;
    return true;
  }
  if (f.copies == 2) net.stats.duplicated++;
  for (int c = 0; c < f.copies; c++) {
    const uint32_t slot = take_slot(net);
    Datagram& d = net.store[slot];
    d.from = source;
    d.to = to;
    d.at_us = f.arrive_us[c];
    d.size = static_cast<uint16_t>(size);
    if (size) std::memcpy(d.data, data, size);
    net.heap.push_back({f.arrive_us[c], net.next_order++, slot});
    std::push_heap(net.heap.begin(), net.heap.end(), later);
  }
  return true;
}

void sim_advance(SimNetwork& net, int64_t now_us) {
  if (now_us > net.now_us) net.now_us = now_us;
  while (!net.heap.empty() && net.heap.front().at_us <= net.now_us) {
    std::pop_heap(net.heap.begin(), net.heap.end(), later);
    const SimNetwork::Pending p = net.heap.back();
    net.heap.pop_back();
    Datagram& d = net.store[p.slot];
    // Arriving at a NAT's public node: through to the inside, or refused.
    bool refused = false;
    for (const SimNat& nat : net.nats) {
      if (sim_node(d.to) != nat.config.public_node) continue;
      Address inside;
      if (map_in(nat, d.from, d.to.port, net.now_us, inside)) {
        d.to = inside;
      } else {
        refused = true;
      }
      break;
    }
    if (refused) {
      net.stats.nat_dropped++;
      net.free_slots.push_back(p.slot);
      continue;
    }
    SimNetwork::Socket* s = find_socket(net, d.to);
    if (!s) {
      net.stats.unbound++;
      net.free_slots.push_back(p.slot);
      continue;
    }
    net.stats.delivered++;
    net.stats.bytes_delivered += net.store[p.slot].size;
    s->inbox.push_back(p.slot);
  }
}

namespace {

// The next slot delivered to `at`, taken off its inbox; the caller frees it.
bool next_delivered(SimNetwork& net, const Address& at, uint32_t& slot) {
  SimNetwork::Socket* s = find_socket(net, at);
  if (!s || s->head == s->inbox.size()) return false;
  slot = s->inbox[s->head++];
  if (s->head == s->inbox.size()) {
    s->inbox.clear();
    s->head = 0;
  }
  return true;
}

}  // namespace

bool sim_receive(SimNetwork& net, const Address& at, Datagram& out) {
  uint32_t slot = 0;
  if (!next_delivered(net, at, slot)) return false;
  const Datagram& d = net.store[slot];
  out.from = d.from;
  out.to = d.to;
  out.at_us = d.at_us;
  out.size = d.size;
  std::memcpy(out.data, d.data, d.size);
  net.free_slots.push_back(slot);
  return true;
}

int sim_receive_bytes(SimNetwork& net, const Address& at, uint8_t* buffer, size_t capacity,
                      Address& from) {
  uint32_t slot = 0;
  while (next_delivered(net, at, slot)) {
    const Datagram& d = net.store[slot];
    net.free_slots.push_back(slot);
    if (d.size > capacity) continue;
    std::memcpy(buffer, d.data, d.size);
    from = d.from;
    return d.size;
  }
  return -1;
}

}  // namespace ge::net
