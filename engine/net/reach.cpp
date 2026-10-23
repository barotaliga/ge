// The rendezvous's two ends: the host's registration and punches, the client's join, direct
// connect and relay fallback (net/reach.hpp).
//
// ge:summary Reach's host and client: register, punch, join, retarget, relay, the outcome
// ge:keywords net reach punch register join retarget relay fallback outcome
// ge:skill net
// ge:test tests/net_reach_test.cpp
#include "net/reach.hpp"

#include "net/reach_wire.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

using namespace reach_wire;

constexpr int64_t kLongAgo = INT64_MIN / 4;

uint64_t draw64(core::Rng& rng) {
  const auto hi = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  const auto lo = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  return hi << 32 | lo;
}

// An ask to the server, padded to the biggest answer (rendezvous.hpp: no amplification).
void ask(Node& node, const Address& to, Kind kind, uint64_t a, uint64_t b, bool two, int64_t now_us,
         uint16_t port = 0) {
  uint8_t out[kMaxSize];
  Writer w = make_writer(out, sizeof out);
  write_start(w, kind);
  write_u64(w, a);
  if (two) write_u64(w, b);
  if (port) write_u16(w, port);
  write_pad_to(w, kMaxSize);
  transport_send(*node.transport, to, out, w.size, now_us);
}

void punch(Node& node, const Address& to, uint64_t nonce, int64_t now_us) {
  uint8_t out[16];
  Writer w = make_writer(out, sizeof out);
  write_start(w, kPunch);
  write_u64(w, nonce);
  transport_send(*node.transport, to, out, w.size, now_us);
}

void bind(Node& node, const Address& relay, uint64_t key, Role role, int64_t now_us) {
  uint8_t out[16];
  Writer w = make_writer(out, sizeof out);
  write_start(w, kBind);
  write_u64(w, key);
  write_u8(w, role);
  transport_send(*node.transport, relay, out, w.size, now_us);
}

// A peer message: who, the nonce, the relay and its key.
struct PeerInfo {
  Address address, relay;
  uint64_t nonce = 0, key = 0;
};

bool read_peer(Reader& r, PeerInfo& p) {
  p.address = read_address(r);
  p.nonce = read_u64(r);
  p.relay = read_address(r);
  p.key = read_u64(r);
  return !r.bad;
}

void fail(ReachClient& c, ReachWhy why, int64_t now_us) {
  if (c.peer >= 0 && c.node->peers[c.peer].state != PeerState::free &&
      !peer_connected(*c.node, c.peer)) {
    node_disconnect(*c.node, c.peer, now_us);
  }
  c.peer = -1;
  c.state = ReachState::failed;
  c.why = why;
}

void to_relay(ReachClient& c, int64_t now_us) {
  if (c.relay.family == Family::none) {
    fail(c, ReachWhy::no_relay, now_us);
    return;
  }
  bind(*c.node, c.relay, c.key, kClient, now_us);
  c.stats.binds++;
  c.last_bind_us = now_us;
  c.state = ReachState::relaying;
  c.host_address = c.relay;
  if (!node_retarget(*c.node, c.peer, c.relay, now_us))
    c.peer = node_connect(*c.node, c.relay, now_us);
  if (c.peer < 0) fail(c, ReachWhy::timed_out, now_us);
}

}  // namespace

const char* reach_path_name(ReachPath p) {
  switch (p) {
    case ReachPath::none:
      return "none";
    case ReachPath::direct:
      return "direct";
    case ReachPath::relay:
      return "relay";
  }
  return "?";
}

const char* reach_state_name(ReachState s) {
  switch (s) {
    case ReachState::idle:
      return "idle";
    case ReachState::joining:
      return "joining";
    case ReachState::direct:
      return "direct";
    case ReachState::relaying:
      return "relaying";
    case ReachState::connected:
      return "connected";
    case ReachState::failed:
      return "failed";
  }
  return "?";
}

const char* reach_why_name(ReachWhy w) {
  switch (w) {
    case ReachWhy::none:
      return "none";
    case ReachWhy::unknown_session:
      return "unknown-session";
    case ReachWhy::no_answer:
      return "no-answer";
    case ReachWhy::no_relay:
      return "no-relay";
    case ReachWhy::timed_out:
      return "timed-out";
  }
  return "?";
}

void reach_host_init(ReachHost& h, const ReachConfig& config, Node& node) {
  h = ReachHost{};
  h.config = config;
  h.node = &node;
  h.last_register_us = h.last_ask_us = kLongAgo;
}

void reach_host_update(ReachHost& h, int64_t now_us) {
  Node& node = *h.node;
  for (const Foreign& f : node.foreign) {
    if (f.from != h.config.rendezvous) continue;
    Reader r = make_reader(node.foreign_bytes.data() + f.offset, f.size);
    Kind kind{};
    if (!read_start(r, kind)) continue;
    if (kind == kRegistered) {
      const Address seen = read_address(r);
      if (r.bad) continue;
      h.registered = true;
      h.public_address = seen;
      h.last_register_us = now_us;
    } else if (kind == kPeer) {
      PeerInfo p;
      if (!read_peer(r, p)) continue;
      Introduced* in = nullptr;
      for (Introduced& i : h.introduced) {
        if (i.nonce == p.nonce) in = &i;
      }
      if (!in) {
        h.introduced.push_back({});
        in = &h.introduced.back();
        in->nonce = p.nonce;
        in->punches_left = h.config.punches;
        in->next_punch_us = now_us;
        in->last_bind_us = kLongAgo;
        in->at_us = now_us;
      }
      in->client = p.address;
      in->relay = p.relay;
      in->key = p.key;
    }
  }
  const bool due = !h.registered || now_us - h.last_register_us >= h.config.register_us;
  if (due && now_us - h.last_ask_us >= h.config.ask_us) {
    ask(node, h.config.rendezvous, kRegister, h.config.session, 0, false, now_us, h.forwarded_port);
    h.last_ask_us = now_us;
    h.stats.asks++;
  }
  size_t kept = 0;
  for (Introduced& in : h.introduced) {
    if (now_us - in.at_us > h.config.introduced_life_us) continue;
    if (in.punches_left > 0 && now_us >= in.next_punch_us) {
      punch(node, in.client, in.nonce, now_us);
      in.punches_left--;
      in.next_punch_us = now_us + h.config.punch_every_us;
      h.stats.punches++;
    }
    // Bound quickly while the client may still need it, then kept.
    const int64_t every =
        now_us - in.at_us < h.config.give_up_us ? h.config.ask_us : h.config.bind_every_us;
    if (in.relay.family != Family::none && now_us - in.last_bind_us >= every) {
      bind(node, in.relay, in.key, kHost, now_us);
      in.last_bind_us = now_us;
      h.stats.binds++;
    }
    h.introduced[kept++] = in;
  }
  h.introduced.resize(kept);
}

void reach_host_forward(ReachHost& h, uint16_t port, int64_t now_us) {
  if (h.forwarded_port == port) return;
  h.forwarded_port = port;
  // Registered again at once, so the server introduces clients by the forwarded port from now.
  h.registered = false;
  h.last_ask_us = kLongAgo;
  (void)now_us;
}

void reach_client_init(ReachClient& c, const ReachConfig& config, Node& node) {
  c = ReachClient{};
  c.config = config;
  c.node = &node;
  core::Rng rng = core::make_rng(static_cast<uint32_t>(config.seed ^ (config.seed >> 32)));
  c.nonce = draw64(rng) | 1;
}

void reach_client_start(ReachClient& c, int64_t now_us) {
  c.state = ReachState::joining;
  c.why = ReachWhy::none;
  c.path = ReachPath::none;
  c.peer = -1;
  c.unknown = false;
  c.started_us = now_us;
  ask(*c.node, c.config.rendezvous, kJoin, c.config.session, c.nonce, true, now_us);
  c.last_ask_us = now_us;
  c.stats.asks++;
}

void reach_client_update(ReachClient& c, int64_t now_us) {
  if (c.state == ReachState::idle || c.state == ReachState::failed) return;
  Node& node = *c.node;
  for (const Foreign& f : node.foreign) {
    Reader r = make_reader(node.foreign_bytes.data() + f.offset, f.size);
    Kind kind{};
    if (!read_start(r, kind)) continue;
    if (f.from == c.config.rendezvous && kind == kPeer && c.state == ReachState::joining) {
      PeerInfo p;
      if (!read_peer(r, p) || p.nonce != c.nonce) continue;
      c.host_public = c.host_address = p.address;
      c.relay = p.relay;
      c.key = p.key;
      c.peer = node_connect(node, c.host_public, now_us);
      if (c.peer < 0) {
        fail(c, ReachWhy::timed_out, now_us);
        return;
      }
      c.state = ReachState::direct;
      c.direct_since_us = now_us;
    } else if (f.from == c.config.rendezvous && kind == kUnknown) {
      c.unknown = true;  // the host may be registering now (its ask lost): asked again till give-up
    } else if (kind == kPunch && c.state == ReachState::direct) {
      // The host's NAT gave this pair its own port: the connect goes where the punch is from.
      const uint64_t nonce = read_u64(r);
      if (r.bad || nonce != c.nonce || f.from == c.host_address) continue;
      if (node_retarget(node, c.peer, f.from, now_us)) {
        c.host_address = f.from;
        c.stats.retargets++;
      }
    }
  }
  for (const Event& e : node.events) {
    if (e.peer != c.peer || c.state == ReachState::connected) continue;
    if (e.type == EventType::connected) {
      c.state = ReachState::connected;
      c.path = c.host_address == c.relay ? ReachPath::relay : ReachPath::direct;
      c.connected_us = now_us - c.started_us;
    } else if (e.type == EventType::connect_failed) {
      c.peer = -1;
      if (c.state == ReachState::direct) {
        to_relay(c, now_us);
      } else {
        fail(c, ReachWhy::timed_out, now_us);
        return;
      }
    }
  }
  switch (c.state) {
    case ReachState::joining:
      if (now_us - c.started_us >= c.config.give_up_us) {
        fail(c, c.unknown ? ReachWhy::unknown_session : ReachWhy::no_answer, now_us);
        return;
      }
      if (now_us - c.last_ask_us >= c.config.ask_us) {
        ask(node, c.config.rendezvous, kJoin, c.config.session, c.nonce, true, now_us);
        c.last_ask_us = now_us;
        c.stats.asks++;
      }
      break;
    case ReachState::direct:
      if (now_us - c.direct_since_us >= c.config.direct_us) to_relay(c, now_us);
      break;
    case ReachState::relaying:
      if (now_us - c.last_bind_us >= c.config.ask_us) {
        bind(node, c.relay, c.key, kClient, now_us);
        c.last_bind_us = now_us;
        c.stats.binds++;
      }
      break;
    case ReachState::connected:
      if (c.path == ReachPath::relay && now_us - c.last_bind_us >= c.config.bind_every_us) {
        bind(node, c.relay, c.key, kClient, now_us);
        c.last_bind_us = now_us;
        c.stats.binds++;
      }
      return;
    default:
      return;
  }
  if (c.state != ReachState::connected && c.state != ReachState::failed &&
      now_us - c.started_us >= c.config.give_up_us) {
    fail(c, ReachWhy::timed_out, now_us);
  }
}

}  // namespace ge::net
