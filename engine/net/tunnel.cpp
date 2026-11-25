// The tunnel (tunnel.hpp): a game's datagrams in pieces on the node's one channel, and the
// node's foreign datagrams handed on beside them.
//
// ge:summary The tunnel: a game's datagrams in pieces through the node, foreign ones beside them
// ge:keywords net tunnel pieces reassembly keep_foreign pipe native-protocol
// ge:skill net
// ge:test tests/net_tunnel_test.cpp
#include "net/tunnel.hpp"

#include <cstring>

namespace ge::net {

namespace {

constexpr int kChannel = 0;

void queue_came(Tunnel& t, const Address& from, const uint8_t* data, size_t size) {
  if (t.next > 0 && t.next == t.came.size()) {  // all handed on: start the buffer again
    t.came.clear();
    t.came_bytes.clear();
    t.next = 0;
  }
  Tunnel::Came c;
  c.from = from;
  c.offset = static_cast<uint32_t>(t.came_bytes.size());
  c.size = static_cast<uint32_t>(size);
  t.came_bytes.insert(t.came_bytes.end(), data, data + size);
  t.came.push_back(c);
}

int peer_at(const Tunnel& t, const Address& address) {
  if (!t.node) return -1;
  const Node& n = *t.node;
  if (!t.host) return peer_connected(n, t.server_peer) ? t.server_peer : -1;
  for (size_t i = 0; i < n.peers.size(); i++)
    if (n.peers[i].state == PeerState::connected && n.peers[i].address == address)
      return static_cast<int>(i);
  return -1;
}

bool carry(Tunnel& t, int peer, const uint8_t* data, size_t size) {
  const size_t count = (size + kTunnelPiece - 1) / kTunnelPiece;
  if (size == 0 || count > static_cast<size_t>(kTunnelPieces)) {
    t.stats.refused++;
    return false;
  }
  const uint16_t number = t.next_number[static_cast<size_t>(peer)]++;
  uint8_t piece[kMaxMessage];
  for (size_t i = 0; i < count; i++) {
    const size_t at = i * kTunnelPiece;
    const size_t n = size - at < kTunnelPiece ? size - at : kTunnelPiece;
    piece[0] = static_cast<uint8_t>(number & 0xff);
    piece[1] = static_cast<uint8_t>(number >> 8);
    piece[2] = static_cast<uint8_t>(i);
    piece[3] = static_cast<uint8_t>(count);
    std::memcpy(piece + kTunnelHeader, data + at, n);
    if (!node_send(*t.node, peer, kChannel, piece, kTunnelHeader + n)) {
      t.stats.refused++;
      return false;
    }
  }
  t.stats.carried_out++;
  return true;
}

bool inner_send(void* ctx, const Address& to, const uint8_t* data, size_t size) {
  Tunnel& t = *static_cast<Tunnel*>(ctx);
  if (!t.host && !t.failed) {  // a client: everything to its host goes through the node
    if (to == t.server) {
      if (tunnel_joined(t)) return carry(t, t.server_peer, data, size);
      if (static_cast<int>(t.held.size()) >= kTunnelHeld) {
        t.stats.refused++;
        return false;
      }
      t.held.emplace_back(data, data + size);
      return true;
    }
  } else if (t.host) {
    if (const int peer = peer_at(t, to); peer >= 0) return carry(t, peer, data, size);
  }
  t.stats.plain_out++;
  return transport_send(*t.outer, to, data, size, t.now_us);
}

int inner_receive(void* ctx, uint8_t* buffer, size_t capacity, Address& from) {
  Tunnel& t = *static_cast<Tunnel*>(ctx);
  while (t.next < t.came.size()) {
    const Tunnel::Came& c = t.came[t.next++];
    if (c.size > capacity) continue;
    std::memcpy(buffer, t.came_bytes.data() + c.offset, c.size);
    from = c.from;
    return static_cast<int>(c.size);
  }
  return -1;
}

// A piece from `peer`: put into its datagram, which is handed on once whole.
void piece_came(Tunnel& t, int peer, const uint8_t* data, size_t size) {
  if (size <= kTunnelHeader) return;
  const auto number = static_cast<uint16_t>(data[0] | (data[1] << 8));
  const int index = data[2], count = data[3];
  if (count < 1 || count > kTunnelPieces || index >= count) return;
  const size_t n = size - kTunnelHeader;
  const Address& from = t.host ? t.node->peers[static_cast<size_t>(peer)].address : t.server;
  if (count == 1) {
    t.stats.carried_in++;
    queue_came(t, from, data + kTunnelHeader, n);
    return;
  }
  Tunnel::Partial& p = t.partial[static_cast<size_t>(peer)];
  if (p.count == 0 || p.number != number) {
    if (p.count != 0) t.stats.lost++;  // the one being put together never finished
    p.number = number;
    p.count = static_cast<uint8_t>(count);
    p.have = 0;
    p.size = 0;
  }
  // Every piece but the last is whole, so a piece's place in the datagram is its index's.
  if (index < count - 1 && n != kTunnelPiece) return;
  std::memcpy(p.data + static_cast<size_t>(index) * kTunnelPiece, data + kTunnelHeader, n);
  p.have = static_cast<uint8_t>(p.have | (1u << index));
  if (index == count - 1)
    p.size = static_cast<uint32_t>(static_cast<size_t>(index) * kTunnelPiece + n);
  if (p.have == (1u << count) - 1) {
    t.stats.carried_in++;
    queue_came(t, from, p.data, p.size);
    p.count = 0;
  }
}

bool start(Tunnel& t, Transport& outer, const TunnelConfig& config, bool host) {
  t.host = host;
  t.outer = &outer;
  t.failed = false;
  t.came.clear();
  t.came_bytes.clear();
  t.next = 0;
  t.held.clear();
  NodeConfig nc;
  nc.protocol = config.protocol;
  nc.max_peers = host ? config.max_peers : 1;
  nc.accept = host;
  nc.channels.count = 1;
  nc.channels.delivery[0] = Delivery::unreliable;
  nc.timeout_us = config.timeout_us;
  nc.connect_timeout_us = config.connect_timeout_us;
  nc.seed = config.seed;
  nc.keep_foreign = true;
  if (host) {
    nc.security = config.security;
    nc.guards = config.guards;
  }
  if (nc.max_peers < 1 || nc.max_peers > kMaxPeers) return false;
  t.node = std::make_unique<Node>();
  node_init(*t.node, nc, outer);
  t.partial.assign(static_cast<size_t>(nc.max_peers), Tunnel::Partial{});
  t.next_number.assign(static_cast<size_t>(nc.max_peers), 0);
  DatagramPipe pipe;
  pipe.send = inner_send;
  pipe.receive = inner_receive;
  pipe.ctx = &t;
  if (!open_pipe_transport(t.inner, pipe)) return false;
  t.inner.local = outer.local;
  return true;
}

}  // namespace

bool tunnel_host(Tunnel& t, Transport& outer, const TunnelConfig& config) {
  return start(t, outer, config, true);
}

bool tunnel_connect(Tunnel& t, Transport& outer, const Address& host, const TunnelConfig& config,
                    int64_t now_us, const ConnectToken* token) {
  if (!start(t, outer, config, false)) return false;
  t.server = host;
  t.server_peer = token != nullptr ? node_connect_secure(*t.node, host, *token, now_us)
                                   : node_connect(*t.node, host, now_us);
  t.failed = t.server_peer < 0;
  return !t.failed;
}

void tunnel_update(Tunnel& t, int64_t now_us) {
  if (!t.node) return;
  t.now_us = now_us;
  transport_pump(*t.outer, now_us);
  node_update(*t.node, now_us);
  Node& n = *t.node;
  for (const Event& e : n.events) {
    if (e.type == EventType::message && e.channel == kChannel) {
      piece_came(t, e.peer, event_data(n, e), e.size);
    } else if (e.type == EventType::connected && !t.host && e.peer == t.server_peer) {
      for (const std::vector<uint8_t>& d : t.held) carry(t, t.server_peer, d.data(), d.size());
      t.held.clear();
    } else if ((e.type == EventType::disconnected || e.type == EventType::connect_failed) &&
               !t.host && e.peer == t.server_peer) {
      t.failed = true;
      t.held.clear();
    } else if (e.type == EventType::disconnected && t.host) {
      t.partial[static_cast<size_t>(e.peer)] = Tunnel::Partial{};
      t.next_number[static_cast<size_t>(e.peer)] = 0;
    }
  }
  for (const Foreign& f : n.foreign) {
    t.stats.plain_in++;
    queue_came(t, f.from, n.foreign_bytes.data() + f.offset, f.size);
  }
}

void tunnel_flush(Tunnel& t, int64_t now_us) {
  t.now_us = now_us;
  if (t.node) node_flush(*t.node, now_us);
}

void tunnel_close(Tunnel& t, int64_t now_us) {
  if (!t.node) return;
  for (size_t i = 0; i < t.node->peers.size(); i++)
    if (t.node->peers[i].state != PeerState::free)
      node_disconnect(*t.node, static_cast<int>(i), now_us);
}

bool tunnel_carried(const Tunnel& t, const Address& address) { return peer_at(t, address) >= 0; }

bool tunnel_joined(const Tunnel& t) {
  return !t.host && t.node && peer_connected(*t.node, t.server_peer);
}

}  // namespace ge::net
