// The node's handshake on both sides, its peers' timeouts and retries, the event list and the
// flush (the protocol is stated in node.hpp and packet.hpp).
//
// ge:summary The node's handshake, retries, timeouts, events and per-peer flush
// ge:keywords net node handshake cookie key retry timeout event flush keepalive
// ge:skill net
// ge:test tests/net_node_test.cpp
#include "net/node.hpp"

namespace ge::net {

namespace {

uint64_t mix64(uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

uint64_t draw64(core::Rng& rng) {
  const auto hi = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  const auto lo = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  return hi << 32 | lo;
}

uint64_t make_cookie(const Node& n, const Address& from, uint64_t salt) {
  return mix64(n.secret ^ mix64(address_hash(from) ^ mix64(salt)));
}

uint64_t make_key(const Node& n, uint64_t salt, uint64_t cookie) {
  const uint64_t k = mix64(cookie ^ mix64(salt + n.secret));
  return k ? k : 1;
}

void push(Node& n, EventType t, int peer, Reason reason) {
  n.events.push_back({t, peer, 0, reason, 0, 0});
}

void on_message(void* ctx, int channel, const uint8_t* data, size_t size) {
  auto* n = static_cast<Node*>(ctx);
  // The peer is filled in by node_update once the payload is read.
  const auto offset = static_cast<uint32_t>(n->event_bytes.size());
  n->event_bytes.insert(n->event_bytes.end(), data, data + size);
  n->events.push_back(
      {EventType::message, -1, channel, Reason::none, offset, static_cast<uint32_t>(size)});
}

void send_handshake(Node& n, Peer& p, PacketType type, const Handshake& h, int64_t now_us) {
  uint8_t out[kMaxPacket];
  Writer w = make_writer(out, sizeof out);
  write_handshake(w, n.config.protocol, type, h);
  transport_send(*n.transport, p.address, out, w.size, now_us);
  p.last_send_us = now_us;
}

void reply(Node& n, const Address& to, PacketType type, const Handshake& h, int64_t now_us) {
  uint8_t out[kMaxPacket];
  Writer w = make_writer(out, sizeof out);
  write_handshake(w, n.config.protocol, type, h);
  transport_send(*n.transport, to, out, w.size, now_us);
}

int find_peer(const Node& n, const Address& a) {
  for (size_t i = 0; i < n.peers.size(); i++) {
    if (n.peers[i].state != PeerState::free && n.peers[i].address == a) return static_cast<int>(i);
  }
  return -1;
}

int free_slot(const Node& n) {
  for (size_t i = 0; i < n.peers.size(); i++) {
    if (n.peers[i].state == PeerState::free) return static_cast<int>(i);
  }
  return -1;
}

void release(Peer& p) {
  p.state = PeerState::free;
  p.address = Address{};
  p.outgoing = false;
}

void connect_peer(Node& n, int slot, uint64_t key, int64_t now_us) {
  Peer& p = n.peers[slot];
  connection_init(p.conn, n.config.channels, key, now_us);
  p.state = PeerState::connected;
  p.last_recv_us = now_us;
  push(n, EventType::connected, slot, Reason::none);
}

// A packet that is not a payload.
void on_handshake(Node& n, const Address& from, PacketType type, const Handshake& h,
                  int64_t now_us) {
  const int at = find_peer(n, from);
  Peer* p = at >= 0 ? &n.peers[at] : nullptr;
  switch (type) {
    case PacketType::request: {
      if (p && p->state == PeerState::connected && !p->outgoing && p->salt == h.salt) {
        reply(n, from, PacketType::accept,
              {h.salt, p->conn.key, static_cast<uint8_t>(at),
               static_cast<uint8_t>(n.config.max_peers), 0},
              now_us);  // our accept was lost: say it again
        return;
      }
      if (!n.config.accept || n.closed) {
        n.stats.denied++;
        reply(n, from, PacketType::deny,
              {h.salt, 0, 0, 0, static_cast<uint8_t>(DenyReason::closed)}, now_us);
        return;
      }
      reply(n, from, PacketType::challenge, {h.salt, make_cookie(n, from, h.salt), 0, 0, 0},
            now_us);
      return;
    }
    case PacketType::challenge:
      if (p && p->outgoing && p->state == PeerState::requesting && p->salt == h.salt) {
        p->cookie = h.value;
        p->state = PeerState::responding;
        p->last_try_us = now_us;
        send_handshake(n, *p, PacketType::response, {p->salt, p->cookie, 0, 0, 0}, now_us);
      }
      return;
    case PacketType::response: {
      if (!n.config.accept) return;
      if (h.value != make_cookie(n, from, h.salt)) {
        n.stats.bad_cookies++;
        return;
      }
      if (p && p->state == PeerState::connected && !p->outgoing && p->salt == h.salt) {
        reply(n, from, PacketType::accept,
              {h.salt, p->conn.key, static_cast<uint8_t>(at),
               static_cast<uint8_t>(n.config.max_peers), 0},
              now_us);
        return;
      }
      if (p) return;  // this address is another peer of ours already
      const int slot = n.closed ? -1 : free_slot(n);
      if (slot < 0) {
        n.stats.denied++;
        reply(n, from, PacketType::deny,
              {h.salt, 0, 0, 0,
               static_cast<uint8_t>(n.closed ? DenyReason::closed : DenyReason::full)},
              now_us);
        return;
      }
      Peer& q = n.peers[slot];
      q.address = from;
      q.outgoing = false;
      q.salt = h.salt;
      q.cookie = h.value;
      q.started_us = now_us;
      connect_peer(n, slot, make_key(n, h.salt, h.value), now_us);
      reply(n, from, PacketType::accept,
            {h.salt, q.conn.key, static_cast<uint8_t>(slot),
             static_cast<uint8_t>(n.config.max_peers), 0},
            now_us);
      return;
    }
    case PacketType::accept:
      if (p && p->outgoing && p->state != PeerState::connected && p->salt == h.salt) {
        connect_peer(n, at, h.value, now_us);
      }
      return;
    case PacketType::deny:
      if (p && p->outgoing && p->state != PeerState::connected && p->salt == h.salt) {
        push(n, EventType::connect_failed, at,
             h.reason == static_cast<uint8_t>(DenyReason::full) ? Reason::denied_full
                                                                : Reason::denied_closed);
        release(*p);
      }
      return;
    case PacketType::disconnect:
      if (p && p->state == PeerState::connected) {
        if (h.value != p->conn.key) {
          n.stats.bad_keys++;
          return;
        }
        push(n, EventType::disconnected, at, Reason::remote_closed);
        release(*p);
      }
      return;
    case PacketType::payload:
      return;
  }
}

}  // namespace

const char* event_name(EventType t) {
  switch (t) {
    case EventType::connected:
      return "connected";
    case EventType::disconnected:
      return "disconnected";
    case EventType::message:
      return "message";
    case EventType::connect_failed:
      return "connect_failed";
  }
  return "unknown";
}

const char* reason_name(Reason r) {
  switch (r) {
    case Reason::none:
      return "none";
    case Reason::timed_out:
      return "timed_out";
    case Reason::remote_closed:
      return "remote_closed";
    case Reason::denied_full:
      return "denied_full";
    case Reason::denied_closed:
      return "denied_closed";
    case Reason::connect_timed_out:
      return "connect_timed_out";
  }
  return "unknown";
}

void node_init(Node& node, const NodeConfig& config, Transport& transport) {
  node.config = config;
  if (node.config.max_peers < 1) node.config.max_peers = 1;
  if (node.config.max_peers > kMaxPeers) node.config.max_peers = kMaxPeers;
  if (node.config.channels.count < 1) node.config.channels.count = 1;
  if (node.config.channels.count > kMaxChannels) node.config.channels.count = kMaxChannels;
  node.transport = &transport;
  node.rng = core::make_rng(static_cast<uint32_t>(config.seed ^ (config.seed >> 32)));
  node.secret = draw64(node.rng);
  node.closed = false;
  node.peers.assign(static_cast<size_t>(node.config.max_peers), Peer{});
  for (Peer& p : node.peers) connection_init(p.conn, node.config.channels, 0, 0);
  node.events.clear();
  node.events.reserve(1024);
  node.event_bytes.clear();
  node.event_bytes.reserve(size_t{64} * 1024);
  node.stats = NodeStats{};
}

int node_connect(Node& node, const Address& to, int64_t now_us) {
  const int slot = free_slot(node);
  if (slot < 0) return -1;
  Peer& p = node.peers[slot];
  p.state = PeerState::requesting;
  p.address = to;
  p.outgoing = true;
  p.salt = draw64(node.rng);
  p.started_us = now_us;
  p.last_try_us = now_us;
  send_handshake(node, p, PacketType::request, {p.salt, 0, 0, 0, 0}, now_us);
  return slot;
}

void node_update(Node& node, int64_t now_us) {
  node.events.clear();
  node.event_bytes.clear();
  Address from;
  while (true) {
    const int n = transport_receive(*node.transport, node.buffer, sizeof node.buffer, from, now_us);
    if (n < 0) break;
    Reader r = make_reader(node.buffer, static_cast<size_t>(n));
    uint32_t protocol = 0;
    PacketType type{};
    if (!read_packet_start(r, protocol, type) || protocol != node.config.protocol) {
      node.stats.foreign++;
      continue;
    }
    if (type != PacketType::payload) {
      Handshake h;
      if (!read_handshake(r, type, h)) {
        node.stats.foreign++;
        continue;
      }
      if (type != PacketType::request && type != PacketType::response &&
          find_peer(node, from) < 0) {
        node.stats.strangers++;
        continue;
      }
      on_handshake(node, from, type, h, now_us);
      continue;
    }
    const int at = find_peer(node, from);
    if (at < 0 || node.peers[at].state != PeerState::connected) {
      node.stats.strangers++;
      continue;
    }
    PayloadHeader h;
    Peer& p = node.peers[at];
    if (!read_payload_header(r, h) || h.key != p.conn.key) {
      node.stats.bad_keys++;
      continue;
    }
    const size_t first = node.events.size();
    if (connection_read(p.conn, h, r, now_us, {on_message, &node})) p.last_recv_us = now_us;
    for (size_t i = first; i < node.events.size(); i++) node.events[i].peer = at;
  }

  for (size_t i = 0; i < node.peers.size(); i++) {
    Peer& p = node.peers[i];
    const int slot = static_cast<int>(i);
    switch (p.state) {
      case PeerState::free:
        break;
      case PeerState::requesting:
      case PeerState::responding:
        if (now_us - p.started_us >= node.config.connect_timeout_us) {
          push(node, EventType::connect_failed, slot, Reason::connect_timed_out);
          release(p);
        } else if (now_us - p.last_try_us >= node.config.retry_us) {
          p.last_try_us = now_us;
          if (p.state == PeerState::requesting) {
            send_handshake(node, p, PacketType::request, {p.salt, 0, 0, 0, 0}, now_us);
          } else {
            send_handshake(node, p, PacketType::response, {p.salt, p.cookie, 0, 0, 0}, now_us);
          }
        }
        break;
      case PeerState::connected:
        if (now_us - p.last_recv_us >= node.config.timeout_us) {
          push(node, EventType::disconnected, slot, Reason::timed_out);
          release(p);
        } else {
          connection_audit(p.conn, now_us);
        }
        break;
    }
  }
}

bool node_send(Node& node, int peer, int channel, const uint8_t* data, size_t size) {
  if (!peer_connected(node, peer)) return false;
  return connection_queue(node.peers[peer].conn, channel, data, size);
}

int node_broadcast(Node& node, int channel, const uint8_t* data, size_t size, int except) {
  int sent = 0;
  for (int i = 0; i < static_cast<int>(node.peers.size()); i++) {
    if (i != except && node_send(node, i, channel, data, size)) sent++;
  }
  return sent;
}

void node_flush(Node& node, int64_t now_us) {
  uint8_t out[kMaxPacket];
  for (Peer& p : node.peers) {
    if (p.state != PeerState::connected) continue;
    // A few packets a peer a flush at most: a backlog past that waits for the next flush.
    for (int k = 0; k < 8; k++) {
      const bool idle = now_us - p.last_send_us >= node.config.keepalive_us;
      const size_t n = connection_write(p.conn, node.config.protocol, out, now_us, k == 0 && idle);
      if (n == 0) break;
      transport_send(*node.transport, p.address, out, n, now_us);
      p.last_send_us = now_us;
    }
    connection_end_flush(p.conn);
  }
}

void node_disconnect(Node& node, int peer, int64_t now_us) {
  if (peer < 0 || peer >= static_cast<int>(node.peers.size())) return;
  Peer& p = node.peers[peer];
  if (p.state == PeerState::connected) {
    for (int i = 0; i < 3; i++) {
      send_handshake(node, p, PacketType::disconnect, {0, p.conn.key, 0, 0, 0}, now_us);
    }
  }
  release(p);
}

const uint8_t* event_data(const Node& node, const Event& e) {
  return e.type == EventType::message ? node.event_bytes.data() + e.offset : nullptr;
}

int node_connected_count(const Node& node) {
  int n = 0;
  for (const Peer& p : node.peers) n += p.state == PeerState::connected;
  return n;
}

}  // namespace ge::net
