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
    d.from = from;
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
    SimNetwork::Socket* s = find_socket(net, net.store[p.slot].to);
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
