// The node's handshake on both sides, its peers' timeouts and retries, the event list and the
// flush (the protocol is stated in node.hpp and packet.hpp).
//
// ge:summary The node's handshake, retries, timeouts, events and per-peer flush
// ge:keywords net node handshake cookie key retry timeout event flush keepalive
// ge:skill net
// ge:test tests/net_node_test.cpp
#include "net/node.hpp"

#include <cstring>

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

// A security-on host binds the cookie to the token too, so a response must carry the token its
// request did.
uint64_t token_bits(const OpenedToken& t) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(t.tag[i]) << (8 * i);
  return v;
}

// Take one from `b` (filled at `rate` a second up to `burst`); false when it is empty. A rate of
// 0 is no guard.
bool take(Bucket& b, double rate, double burst, int64_t now_us) {
  if (rate <= 0) return true;
  if (b.at_us == 0 && b.tokens == 0) b.tokens = burst;  // a new bucket starts full
  if (now_us > b.at_us) {
    b.tokens += static_cast<double>(now_us - b.at_us) * rate * 1e-6;
    if (b.tokens > burst) b.tokens = burst;
  }
  b.at_us = now_us;
  if (b.tokens < 1) return false;
  b.tokens -= 1;
  return true;
}

// A stranger's handshake against the guards: the address's bucket, then everyone's.
bool admit_handshake(Node& n, const Address& from, int64_t now_us) {
  const NodeGuards& g = n.config.guards;
  if (g.address_per_s > 0) {
    const uint64_t who = address_hash(from);
    Bucket& b = n.addresses[who % kAddressBuckets];
    if (b.who != who) b = Bucket{who, 0, 0};  // a bucket another address had: start it afresh
    if (!take(b, g.address_per_s, g.address_burst, now_us)) return false;
  }
  return take(n.handshakes, g.handshakes_per_s, g.handshake_burst, now_us);
}

// SEALED PACKETS (packet.hpp). The nonce's kind is the packet's type, so an accept's salt and a
// payload's counter never meet on one key.
void seal_after_header(uint8_t* p, size_t n, const Key& key, uint64_t counter) {
  uint8_t nonce[kNonceBytes];
  counter_nonce(nonce, counter, p[4]);
  seal(p + kSealedHeader, n - kSealedHeader, p + n, key, nonce, p, kSealedHeader);
}
bool open_after_header(uint8_t* p, size_t n, const Key& key, uint64_t counter) {
  if (n < kSealedHeader + kTagBytes) return false;
  uint8_t nonce[kNonceBytes];
  counter_nonce(nonce, counter, p[4]);
  return open(p + kSealedHeader, n - kSealedHeader - kTagBytes, p + n - kTagBytes, key, nonce, p,
              kSealedHeader);
}

uint64_t get_u64(const uint8_t* p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(p[i]) << (8 * i);
  return v;
}
void put_u64(uint8_t* p, uint64_t v) {
  for (int i = 0; i < 8; i++) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

// The replay window: whether `counter` may still be opened (newer than the newest, or one of the
// 256 before it not yet seen), and marking it once it opened.
bool window_allows(const Peer& p, uint64_t counter) {
  if (counter == 0) return false;
  if (counter > p.recv_newest) return true;
  const uint64_t back = p.recv_newest - counter;
  if (back >= 256) return false;
  return !(p.recv_seen[back / 64] >> (back % 64) & 1);
}
void window_mark(Peer& p, uint64_t counter) {
  if (counter > p.recv_newest) {
    const uint64_t shift = counter - p.recv_newest;
    if (shift >= 256) {
      p.recv_seen[0] = p.recv_seen[1] = p.recv_seen[2] = p.recv_seen[3] = 0;
    } else {
      for (uint64_t k = 0; k < shift; k++) {  // a shift of the 256-bit window by one, `shift` times
        p.recv_seen[3] = p.recv_seen[3] << 1 | p.recv_seen[2] >> 63;
        p.recv_seen[2] = p.recv_seen[2] << 1 | p.recv_seen[1] >> 63;
        p.recv_seen[1] = p.recv_seen[1] << 1 | p.recv_seen[0] >> 63;
        p.recv_seen[0] <<= 1;
      }
    }
    p.recv_newest = counter;
  }
  const uint64_t back = p.recv_newest - counter;
  p.recv_seen[back / 64] |= uint64_t{1} << (back % 64);
}

// The host's table of tokens seen: false when this token was seen from another address (or its
// client is connected from another address); otherwise it is kept, replacing an expired or the
// oldest entry.
bool token_fresh(Node& n, const OpenedToken& t, const Address& from, int64_t token_now_us) {
  for (const Peer& p : n.peers) {
    if (p.state == PeerState::connected && p.secure && !p.outgoing && p.client_id == t.client_id &&
        !(p.address == from)) {
      return false;
    }
  }
  int slot = -1;
  for (int i = 0; i < kSeenTokens; i++) {
    SeenToken& s = n.seen[i];
    if (s.used && s.tag == t.tag) {
      if (!(s.from == from)) return false;
      return true;
    }
    if (slot < 0 && (!s.used || s.expire_us <= token_now_us)) slot = i;
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < kSeenTokens; i++) {
      if (n.seen[i].expire_us < n.seen[slot].expire_us) slot = i;
    }
  }
  n.seen[slot] = {t.tag, from, t.expire_us, true};
  return true;
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
  uint8_t out[kMaxSealedPacket];
  Writer w = make_writer(out, kMaxPacket);
  write_handshake(w, n.config.protocol, type, h);
  size_t size = w.size;
  if (p.secure && type == PacketType::request) {
    std::memcpy(out + kRequestTokenAt, p.token.data(), kSealedTokenBytes);
  } else if (p.secure && type == PacketType::response) {
    std::memcpy(out + kResponseTokenAt, p.token.data(), kSealedTokenBytes);
  } else if (p.secure && type == PacketType::disconnect) {
    put_u64(out + 5, ++p.send_counter);
    seal_after_header(out, size, p.send_key, p.send_counter);
    size += kTagBytes;
  }
  transport_send(*n.transport, p.address, out, size, now_us);
  p.last_send_us = now_us;
}

// A host's reply to a handshake; sealed with the key to the client when `key` is given (an
// accept or a deny to a client with a token).
void reply(Node& n, const Address& to, PacketType type, const Handshake& h, int64_t now_us,
           const Key* key = nullptr) {
  uint8_t out[kMaxSealedPacket];
  Writer w = make_writer(out, kMaxPacket);
  write_handshake(w, n.config.protocol, type, h);
  size_t size = w.size;
  if (key) {
    seal_after_header(out, size, *key, h.salt);
    size += kTagBytes;
  }
  transport_send(*n.transport, to, out, size, now_us);
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
  p.send_counter = 0;
  p.recv_newest = 0;
  p.recv_seen[0] = p.recv_seen[1] = p.recv_seen[2] = p.recv_seen[3] = 0;
  p.packets = Bucket{};
  p.state = PeerState::connected;
  p.last_recv_us = now_us;
  push(n, EventType::connected, slot, Reason::none);
}

// A packet that is not a payload. `tok` is the token a request or a response carried, opened
// (a host with security on; null otherwise).
void on_handshake(Node& n, const Address& from, PacketType type, const Handshake& h,
                  const OpenedToken* tok, int64_t now_us) {
  const int at = find_peer(n, from);
  Peer* p = at >= 0 ? &n.peers[at] : nullptr;
  const uint64_t salt = h.salt ^ (tok ? token_bits(*tok) : 0);  // what the cookie is made of
  const Key* to_client = tok ? &tok->to_client : nullptr;
  switch (type) {
    case PacketType::request: {
      if (p && p->state == PeerState::connected && !p->outgoing && p->salt == h.salt) {
        reply(n, from, PacketType::accept,
              {h.salt, p->conn.key, static_cast<uint8_t>(at),
               static_cast<uint8_t>(n.config.max_peers), 0},
              now_us, p->secure ? &p->send_key : nullptr);  // our accept was lost: say it again
        return;
      }
      if (!n.config.accept || n.closed) {
        n.stats.denied++;
        reply(n, from, PacketType::deny,
              {h.salt, 0, 0, 0, static_cast<uint8_t>(DenyReason::closed)}, now_us, to_client);
        return;
      }
      reply(n, from, PacketType::challenge, {h.salt, make_cookie(n, from, salt), 0, 0, 0}, now_us);
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
      if (h.value != make_cookie(n, from, salt)) {
        n.stats.bad_cookies++;
        return;
      }
      if (p && p->state == PeerState::connected && !p->outgoing && p->salt == h.salt) {
        reply(n, from, PacketType::accept,
              {h.salt, p->conn.key, static_cast<uint8_t>(at),
               static_cast<uint8_t>(n.config.max_peers), 0},
              now_us, p->secure ? &p->send_key : nullptr);
        return;
      }
      if (p) return;  // this address is another peer of ours already
      const int slot = n.closed ? -1 : free_slot(n);
      if (slot < 0) {
        n.stats.denied++;
        reply(n, from, PacketType::deny,
              {h.salt, 0, 0, 0,
               static_cast<uint8_t>(n.closed ? DenyReason::closed : DenyReason::full)},
              now_us, to_client);
        return;
      }
      Peer& q = n.peers[slot];
      q.address = from;
      q.outgoing = false;
      q.salt = h.salt;
      q.cookie = h.value;
      q.started_us = now_us;
      q.secure = tok != nullptr;
      if (tok) {
        q.send_key = tok->to_client;
        q.recv_key = tok->to_host;
        q.client_id = tok->client_id;
        q.user = tok->user;
      }
      connect_peer(n, slot, make_key(n, h.salt, h.value), now_us);
      reply(n, from, PacketType::accept,
            {h.salt, q.conn.key, static_cast<uint8_t>(slot),
             static_cast<uint8_t>(n.config.max_peers), 0},
            now_us, tok ? &q.send_key : nullptr);
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
        if (p->secure) {  // a secure peer's disconnect is sealed and opened in node_update
          n.stats.forged++;
          return;
        }
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
  node.seen.assign(node.config.security.on && node.config.accept ? kSeenTokens : 0, SeenToken{});
  node.addresses.assign(node.config.guards.address_per_s > 0 ? kAddressBuckets : 0, Bucket{});
  node.handshakes = Bucket{};
}

NodeGuards default_guards() {
  NodeGuards g;
  // All strangers together: a bound on the work a flood from ever-new addresses makes (opening a
  // token is a microsecond or two), far above what honest clients ask.
  g.handshakes_per_s = 20000;
  g.handshake_burst = 4000;
  g.address_per_s = 20;  // a client's request and response, retried every 100 ms
  g.address_burst = 20;
  g.packets_per_s = 400;
  g.packet_burst = 120;
  return g;
}

int node_connect(Node& node, const Address& to, int64_t now_us) {
  const int slot = free_slot(node);
  if (slot < 0) return -1;
  Peer& p = node.peers[slot];
  p.state = PeerState::requesting;
  p.address = to;
  p.outgoing = true;
  p.secure = false;
  p.salt = draw64(node.rng);
  p.started_us = now_us;
  p.last_try_us = now_us;
  send_handshake(node, p, PacketType::request, {p.salt, 0, 0, 0, 0}, now_us);
  return slot;
}

int node_connect_secure(Node& node, const Address& to, const ConnectToken& token, int64_t now_us) {
  const int slot = free_slot(node);
  if (slot < 0) return -1;
  Peer& p = node.peers[slot];
  p.state = PeerState::requesting;
  p.address = to;
  p.outgoing = true;
  p.secure = true;
  p.send_key = token.to_host;
  p.recv_key = token.to_client;
  p.client_id = token.client_id;
  p.token = token.sealed;
  p.salt = draw64(node.rng);
  p.started_us = now_us;
  p.last_try_us = now_us;
  send_handshake(node, p, PacketType::request, {p.salt, 0, 0, 0, 0}, now_us);
  return slot;
}

bool node_retarget(Node& node, int peer, const Address& to, int64_t now_us) {
  if (peer < 0 || peer >= static_cast<int>(node.peers.size())) return false;
  Peer& p = node.peers[peer];
  if (!p.outgoing || (p.state != PeerState::requesting && p.state != PeerState::responding))
    return false;
  p.state = PeerState::requesting;
  p.address = to;
  p.cookie = 0;
  p.started_us = now_us;
  p.last_try_us = now_us;
  send_handshake(node, p, PacketType::request, {p.salt, 0, 0, 0, 0}, now_us);
  return true;
}

void node_update(Node& node, int64_t now_us) {
  node.events.clear();
  node.event_bytes.clear();
  node.foreign.clear();
  node.foreign_bytes.clear();
  Address from;
  while (true) {
    const int got =
        transport_receive(*node.transport, node.buffer, sizeof node.buffer, from, now_us);
    if (got < 0) break;
    auto n = static_cast<size_t>(got);
    Reader r = make_reader(node.buffer, n);
    uint32_t protocol = 0;
    PacketType type{};
    if (!read_packet_start(r, protocol, type) || protocol != node.config.protocol) {
      node.stats.foreign++;
      if (node.config.keep_foreign) {
        node.foreign.push_back(
            {from, static_cast<uint32_t>(node.foreign_bytes.size()), static_cast<uint32_t>(n)});
        node.foreign_bytes.insert(node.foreign_bytes.end(), node.buffer, node.buffer + n);
      }
      continue;
    }
    const int at = find_peer(node, from);
    Peer* sealed_by = at >= 0 && node.peers[at].secure ? &node.peers[at] : nullptr;
    if (type != PacketType::payload) {
      if (sealed_by && sealed_by->outgoing && sealed_by->state != PeerState::connected &&
          (type == PacketType::accept || type == PacketType::deny)) {
        // A host's answer to our token: sealed with the key to us, the salt its counter.
        if (n < kSealedHeader + kTagBytes ||
            !open_after_header(node.buffer, n, sealed_by->recv_key, get_u64(node.buffer + 5))) {
          node.stats.forged++;
          continue;
        }
        n -= kTagBytes;
        r = make_reader(node.buffer, n);
        read_packet_start(r, protocol, type);
      } else if (sealed_by && sealed_by->state == PeerState::connected &&
                 type == PacketType::disconnect) {
        if (n != kSealedHeader + kTagBytes) {
          node.stats.forged++;
          continue;
        }
        const uint64_t counter = get_u64(node.buffer + 5);
        if (!window_allows(*sealed_by, counter)) {
          node.stats.replayed++;
          continue;
        }
        if (!open_after_header(node.buffer, n, sealed_by->recv_key, counter)) {
          node.stats.forged++;
          continue;
        }
        push(node, EventType::disconnected, at, Reason::remote_closed);
        release(*sealed_by);
        continue;
      }
      Handshake h;
      if (!read_handshake(r, type, h)) {
        node.stats.foreign++;
        continue;
      }
      const bool asks = type == PacketType::request || type == PacketType::response;
      if (!asks && at < 0) {
        node.stats.strangers++;
        continue;
      }
      // A stranger's request or response passes the guards before any work is done on it.
      const bool stranger = at < 0 || node.peers[at].state != PeerState::connected;
      if (asks && node.config.accept && stranger && !admit_handshake(node, from, now_us)) {
        node.stats.throttled++;
        continue;
      }
      OpenedToken token;
      const OpenedToken* tok = nullptr;
      if (asks && node.config.accept && node.config.security.on) {
        const NodeSecurity& sec = node.config.security;
        const size_t token_at = type == PacketType::request ? kRequestTokenAt : kResponseTokenAt;
        const int64_t token_now = now_us + sec.clock_offset_us;
        if (!token_open(sec.key, protocol, node.buffer + token_at, token) ||
            token.host_id != sec.host_id) {
          node.stats.bad_tokens++;
          continue;
        }
        if (token.expire_us <= token_now) {
          node.stats.expired_tokens++;
          continue;
        }
        if (!token_fresh(node, token, from, token_now)) {
          node.stats.replayed_tokens++;
          continue;
        }
        tok = &token;
      }
      on_handshake(node, from, type, h, tok, now_us);
      wipe(&token, sizeof token);
      continue;
    }
    if (at < 0 || node.peers[at].state != PeerState::connected) {
      node.stats.strangers++;
      continue;
    }
    Peer& p = node.peers[at];
    const NodeGuards& g = node.config.guards;
    if (!take(p.packets, g.packets_per_s, g.packet_burst, now_us)) {
      node.stats.flooded++;
      continue;
    }
    if (p.secure) {
      // The counter stands where the key does: check it, open the rest, then put the key back so
      // the payload reads as an unsealed one.
      if (n < kPayloadHeader + kTagBytes) {
        node.stats.forged++;
        continue;
      }
      const uint64_t counter = get_u64(node.buffer + 5);
      if (!window_allows(p, counter)) {
        node.stats.replayed++;
        continue;
      }
      if (!open_after_header(node.buffer, n, p.recv_key, counter)) {
        node.stats.forged++;
        continue;
      }
      window_mark(p, counter);
      put_u64(node.buffer + 5, p.conn.key);
      n -= kTagBytes;
      r = make_reader(node.buffer, n);
      read_packet_start(r, protocol, type);
    }
    PayloadHeader h;
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
  uint8_t out[kMaxSealedPacket];
  for (Peer& p : node.peers) {
    if (p.state != PeerState::connected) continue;
    // A few packets a peer a flush at most: a backlog past that waits for the next flush.
    for (int k = 0; k < 8; k++) {
      const bool idle = now_us - p.last_send_us >= node.config.keepalive_us;
      size_t n = connection_write(p.conn, node.config.protocol, out, now_us, k == 0 && idle);
      if (n == 0) break;
      if (p.secure) {
        put_u64(out + 5, ++p.send_counter);
        seal_after_header(out, n, p.send_key, p.send_counter);
        n += kTagBytes;
      }
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
