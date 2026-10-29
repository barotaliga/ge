// The rendezvous server and its relays (net/rendezvous.hpp).
//
// ge:summary The rendezvous server: register, join, the introduction, STUN, relays forwarding
// ge:keywords net rendezvous relay forward introduce register join stun expire
// ge:skill net
// ge:test tests/net_reach_test.cpp
#include "net/rendezvous.hpp"

#include "net/reach_wire.hpp"
#include "net/stun.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

using namespace reach_wire;

uint64_t draw64(core::Rng& rng) {
  const auto hi = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  const auto lo = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  return hi << 32 | lo;
}

RendezvousSession* find_session(Rendezvous& rv, uint64_t id) {
  for (RendezvousSession& s : rv.sessions) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

void send(Transport& t, const Address& to, const Writer& w, int64_t now_us) {
  if (!w.overflow) transport_send(t, to, w.data, w.size, now_us);
}

// A relay port for this join: the one it already has (a join asked again), or a new one.
Relay* relay_for(Rendezvous& rv, uint64_t session, uint64_t nonce, int64_t now_us) {
  Relay* slot = nullptr;
  for (Relay& r : rv.relays) {
    if (r.open && r.session == session && r.nonce == nonce) return &r;
    if (!r.open && !slot) slot = &r;
  }
  if (!slot) return nullptr;
  Transport& main = *rv.transport;
  bool ok = false;
  if (main.kind == TransportKind::sim) {
    // The next free port on the server's own node; a taken one is skipped.
    for (int tries = 0; tries < 1024 && !ok; tries++) {
      const uint16_t port = rv.next_port++;
      if (rv.next_port < rv.config.first_relay_port) rv.next_port = rv.config.first_relay_port;
      ok = open_sim_transport(slot->transport, *main.sim, sim_address(sim_node(main.local), port));
    }
  } else if (main.kind == TransportKind::udp) {
    Address bind = main.local;
    bind.port = 0;
    ok = open_udp_transport(slot->transport, bind, nullptr);
  }
  if (!ok) return nullptr;
  slot->open = true;
  slot->address = rv.config.public_address;
  slot->address.port = slot->transport.local.port;
  slot->session = session;
  slot->nonce = nonce;
  slot->key = draw64(rv.rng) | 1;
  slot->host = slot->client = Address{};
  slot->host_bound = slot->client_bound = false;
  slot->last_us = now_us;
  rv.stats.relays++;
  return slot;
}

void on_server(Rendezvous& rv, const Address& from, const uint8_t* data, size_t size,
               int64_t now_us) {
  Transport& t = *rv.transport;
  uint8_t out[kMaxSize + kStunAnswerMax];
  if (is_stun(data, size)) {
    const size_t n = stun_answer(data, size, from, out, sizeof out);
    if (n) {
      transport_send(t, from, out, n, now_us);
      rv.stats.stun++;
    } else {
      rv.stats.refused++;
    }
    return;
  }
  Reader r = make_reader(data, size);
  Kind kind{};
  // Every ask is padded to the biggest answer, so no answer outweighs it.
  if (!read_start(r, kind) || size < kMaxSize) {
    rv.stats.refused++;
    return;
  }
  Writer w = make_writer(out, kMaxSize);
  if (kind == kRegister) {
    const uint64_t id = read_u64(r);
    const uint16_t forwarded = read_u16(r);  // the padding's zero when the host has none
    if (r.bad) {
      rv.stats.refused++;
      return;
    }
    RendezvousSession* s = find_session(rv, id);
    if (!s && static_cast<int>(rv.sessions.size()) < rv.config.max_sessions) {
      rv.sessions.push_back({});
      s = &rv.sessions.back();
      s->id = id;
    }
    if (!s) {
      rv.stats.refused++;
      return;
    }
    s->host = from;  // a host whose mapping moved registers from its new address
    s->from = from;
    if (forwarded) s->host.port = forwarded;  // its router forwards this port to it (portmap.hpp)
    s->last_us = now_us;
    write_start(w, kRegistered);
    write_address(w, from);
    send(t, from, w, now_us);
    rv.stats.registered++;
    return;
  }
  if (kind == kJoin) {
    const uint64_t id = read_u64(r), nonce = read_u64(r);
    if (r.bad) {
      rv.stats.refused++;
      return;
    }
    const RendezvousSession* s = find_session(rv, id);
    if (!s) {
      write_start(w, kUnknown);
      write_u64(w, id);
      send(t, from, w, now_us);
      rv.stats.unknown++;
      return;
    }
    const Relay* relay = relay_for(rv, id, nonce, now_us);
    if (!relay) rv.stats.no_relay++;
    const Address relay_at = relay ? relay->address : Address{};
    const uint64_t key = relay ? relay->key : 0;
    write_start(w, kPeer);
    write_address(w, s->host);
    write_u64(w, nonce);
    write_address(w, relay_at);
    write_u64(w, key);
    send(t, from, w, now_us);
    Writer h = make_writer(out, kMaxSize);
    write_start(h, kPeer);
    write_address(h, from);
    write_u64(h, nonce);
    write_address(h, relay_at);
    write_u64(h, key);
    send(t, s->host, h, now_us);
    rv.stats.joined++;
    return;
  }
  if (kind == kAdvertise) {
    const uint64_t id = read_u64(r);
    const uint32_t serial = read_u32(r);
    Advert a;
    a.protocol = read_u32(r);
    const bool listed = read_u8(r) != 0;
    RendezvousSession* s = find_session(rv, id);
    if (r.bad || !read_advert_body(r, a) || !s || !(from == s->from)) {
      rv.stats.refused++;
      return;
    }
    a.session = id;
    s->listed = listed;
    s->advert = a;
    rv.stats.advertised++;
    write_start(w, kAdvertised);  // far smaller than the advertise
    write_u64(w, id);
    write_u32(w, serial);
    send(t, from, w, now_us);
    return;
  }
  if (kind == kList) {
    const uint32_t protocol = read_u32(r);
    const int page = read_u16(r);
    if (r.bad || size < kListAsk) {
      rv.stats.refused++;
      return;
    }
    int count = 0;
    for (const RendezvousSession& s : rv.sessions)
      count += s.listed && s.advert.protocol == protocol;
    const int pages = count ? (count + kListPage - 1) / kListPage : 1;
    uint8_t page_out[kListAsk];
    Writer lw = make_writer(page_out, sizeof page_out);
    write_start(lw, kListing);
    write_u32(lw, protocol);
    write_u16(lw, static_cast<uint16_t>(page));
    write_u16(lw, static_cast<uint16_t>(pages));
    const int first = page * kListPage;
    const int here =
        count - first < 0 ? 0 : (count - first < kListPage ? count - first : kListPage);
    write_u8(lw, static_cast<uint8_t>(here));
    int at = 0;
    for (const RendezvousSession& s : rv.sessions) {
      if (!s.listed || s.advert.protocol != protocol) continue;
      if (at >= first && at < first + here) {
        Advert a = s.advert;
        a.port = s.host.port;  // as clients reach it
        write_address(lw, s.host);
        write_advert_body(lw, a);
      }
      at++;
    }
    if (lw.overflow || lw.size > size) {
      rv.stats.refused++;
      return;
    }
    transport_send(t, from, page_out, lw.size, now_us);
    rv.stats.lists++;
    return;
  }
  rv.stats.refused++;
}

void on_relay(Rendezvous& rv, Relay& relay, const Address& from, const uint8_t* data, size_t size,
              int64_t now_us) {
  Reader r = make_reader(data, size);
  Kind kind{};
  if (read_start(r, kind) && kind == kBind) {
    const uint64_t key = read_u64(r);
    const uint8_t role = read_u8(r);
    if (r.bad || key != relay.key || role > kClient) {
      rv.stats.refused++;
      return;
    }
    (role == kHost ? relay.host : relay.client) = from;
    (role == kHost ? relay.host_bound : relay.client_bound) = true;
    relay.last_us = now_us;
    return;
  }
  const bool from_host = relay.host_bound && from == relay.host;
  const bool from_client = relay.client_bound && from == relay.client;
  if ((!from_host && !from_client) || !relay.host_bound || !relay.client_bound) {
    rv.stats.refused++;
    return;
  }
  transport_send(relay.transport, from_host ? relay.client : relay.host, data, size, now_us);
  relay.last_us = now_us;
  rv.stats.relayed++;
  rv.stats.relayed_bytes += size;
}

}  // namespace

void rendezvous_init(Rendezvous& rv, const RendezvousConfig& config, Transport& transport) {
  rv.config = config;
  if (rv.config.public_address.family == Family::none) rv.config.public_address = transport.local;
  if (rv.config.max_relays < 0) rv.config.max_relays = 0;
  rv.transport = &transport;
  rv.rng = core::make_rng(static_cast<uint32_t>(config.seed ^ (config.seed >> 32)));
  rv.sessions.clear();
  rv.sessions.reserve(static_cast<size_t>(rv.config.max_sessions > 0 ? rv.config.max_sessions : 0));
  rv.relays.clear();
  rv.relays.resize(static_cast<size_t>(rv.config.max_relays));
  rv.next_port = rv.config.first_relay_port;
  rv.stats = RendezvousStats{};
}

void rendezvous_update(Rendezvous& rv, int64_t now_us) {
  Address from;
  while (true) {
    const int n = transport_receive(*rv.transport, rv.buffer, sizeof rv.buffer, from, now_us);
    if (n < 0) break;
    on_server(rv, from, rv.buffer, static_cast<size_t>(n), now_us);
  }
  for (Relay& relay : rv.relays) {
    if (!relay.open) continue;
    while (true) {
      const int n = transport_receive(relay.transport, rv.buffer, sizeof rv.buffer, from, now_us);
      if (n < 0) break;
      on_relay(rv, relay, from, rv.buffer, static_cast<size_t>(n), now_us);
    }
    if (now_us - relay.last_us > rv.config.relay_idle_us) {
      close_transport(relay.transport);
      relay.open = false;
    }
  }
  size_t kept = 0;
  for (const RendezvousSession& s : rv.sessions) {
    if (now_us - s.last_us <= rv.config.session_life_us) rv.sessions[kept++] = s;
  }
  rv.sessions.resize(kept);
}

void rendezvous_close(Rendezvous& rv) {
  for (Relay& relay : rv.relays) {
    if (relay.open) close_transport(relay.transport);
    relay.open = false;
  }
}

int rendezvous_open_relays(const Rendezvous& rv) {
  int n = 0;
  for (const Relay& relay : rv.relays) n += relay.open ? 1 : 0;
  return n;
}

}  // namespace ge::net
