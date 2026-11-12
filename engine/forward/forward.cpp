// Forward (forward.hpp): the route's datagrams, the client's dial and the hop's copying.
//
// ge:summary Forward: the route's datagrams, the dial, the hop's copy, cookies, alive, status
// ge:keywords forward route dial hop cookie alive status stun copy idle
// ge:skill net
// ge:test tests/forward_test.cpp
#include "forward/forward.hpp"

#include <algorithm>

#include "net/packet.hpp"
#include "net/sim_network.hpp"
#include "net/status.hpp"
#include "net/stun.hpp"
#include "net/wire.hpp"

namespace ge::forward {

namespace {

constexpr uint8_t kMagic[4] = {'g', 'e', 'F', 'W'};
constexpr size_t kHead = 4 + 1 + 8;
constexpr size_t kMostWhy = 120;

uint64_t mix(uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

uint64_t make_cookie(const Forward& f, const net::Address& from, uint64_t salt) {
  const uint64_t c = mix(f.secret ^ mix(net::address_hash(from) ^ mix(salt)));
  return c ? c : 1;
}

net::Writer start(uint8_t* buf, size_t cap, FwMsg kind, uint64_t salt) {
  net::Writer w = net::make_writer(buf, cap);
  net::write_bytes(w, kMagic, 4);
  net::write_u8(w, static_cast<uint8_t>(kind));
  net::write_u64(w, salt);
  return w;
}

void send(net::Transport& t, const net::Address& to, const net::Writer& w, int64_t now_us) {
  if (!w.overflow) net::transport_send(t, to, w.data, w.size, now_us);
}

void send_route(net::Transport& t, const Dial& d, int64_t now_us) {
  uint8_t buf[kForwardRouteSize];
  net::Writer w = start(buf, sizeof buf, FwMsg::route, d.salt);
  net::write_u64(w, d.cookie);
  net::write_varint(w, d.rest.size());
  net::write_bytes(w, d.rest.data(), d.rest.size());
  net::write_pad_to(w, kForwardRouteSize);
  send(t, d.first, w, now_us);
}

void send_routed(net::Transport& t, const net::Address& to, uint64_t salt, Answer a,
                 std::string_view why, int64_t now_us) {
  uint8_t buf[kHead + 1 + 2 + kMostWhy];
  net::Writer w = start(buf, sizeof buf, FwMsg::routed, salt);
  net::write_u8(w, static_cast<uint8_t>(a));
  why = why.substr(0, kMostWhy);
  net::write_varint(w, why.size());
  net::write_bytes(w, why.data(), why.size());
  send(t, to, w, now_us);
}

void send_alive(net::Transport& t, const net::Address& to, uint64_t salt, int64_t clock,
                int64_t echo, int64_t now_us) {
  uint8_t buf[kHead + 16];
  net::Writer w = start(buf, sizeof buf, FwMsg::alive, salt);
  net::write_u64(w, static_cast<uint64_t>(clock));
  net::write_u64(w, static_cast<uint64_t>(echo));
  send(t, to, w, now_us);
}

bool allowed(const ForwardConfig& c, const net::Address& to) {
  if (c.open) return true;
  for (const net::Address& a : c.allow) {
    net::Address host = to;
    if (a.port == 0) host.port = 0;
    if (a == host) return true;
  }
  return false;
}

HopClient* by_salt(Forward& f, uint64_t salt) {
  for (auto& c : f.clients)
    if (c->salt == salt) return c.get();
  return nullptr;
}

HopClient* by_address(Forward& f, const net::Address& from) {
  for (auto& c : f.clients)
    if (c->connected && c->from == from) return c.get();
  return nullptr;
}

net::Status status_of(const Forward& f, int64_t now_us) {
  net::Status s;
  s.kind = net::NodeKind::forward;
  s.protocol = forward_protocol();
  s.version = f.config.version;
  s.name = f.config.name;
  s.max_players = f.config.max_clients;
  for (const auto& c : f.clients) {
    if (!c->connected || s.players.size() >= 64) continue;
    net::StatusPlayer p;
    p.name = c->name;
    p.minutes = static_cast<uint16_t>(std::min<int64_t>((now_us - c->since_us) / 60000000, 65535));
    s.players.push_back(std::move(p));
  }
  s.keys = {{"clients", std::to_string(f.clients.size())},
            {"open", f.config.open ? "1" : "0"},
            {"allow", std::to_string(f.config.allow.size())}};
  return s;
}

void route_request(Forward& f, const uint8_t* data, size_t size, const net::Address& from,
                   uint64_t salt, net::Reader& r, int64_t now_us) {
  const uint64_t cookie = net::read_u64(r);
  const uint64_t n = net::read_varint(r);
  if (r.bad || n > kMostRoute || n > net::remaining(r) || size < kForwardRouteSize) return;
  const std::string text(reinterpret_cast<const char*>(data + r.at), static_cast<size_t>(n));
  if (cookie == 0 || cookie != make_cookie(f, from, salt)) {
    if (cookie != 0) f.stats.bad_cookies++;
    uint8_t buf[kHead + 8];
    net::Writer w = start(buf, sizeof buf, FwMsg::cookie, salt);
    net::write_u64(w, make_cookie(f, from, salt));
    send(*f.port, from, w, now_us);
    f.stats.cookies++;
    return;
  }
  if (HopClient* c = by_salt(f, salt)) {  // asked again: the answer was lost
    c->from = from;
    if (c->connected) send_routed(*f.port, from, salt, Answer::ok, "", now_us);
    return;
  }
  const auto refuse = [&](Answer a, const std::string& why) {
    f.stats.refused++;
    send_routed(*f.port, from, salt, a, why, now_us);
  };
  std::vector<net::Address> hops;
  std::string error;
  if (!parse_route(text, hops, error)) return refuse(Answer::bad_route, error);
  if (static_cast<int>(f.clients.size()) >= f.config.max_clients)
    return refuse(Answer::full, f.config.name + " is full");
  if (!allowed(f.config, hops[0]))
    return refuse(Answer::not_allowed,
                  f.config.name + " does not forward to " + net::to_string(hops[0]));
  auto c = std::make_unique<HopClient>();
  c->up = std::make_unique<net::Transport>();
  if (!f.open_upstream || !f.open_upstream(*c->up))
    return refuse(Answer::unreachable, f.config.name + " has no socket free");
  c->from = from;
  c->salt = salt;
  c->to = hops[0];
  c->name = net::to_string(from);
  c->since_us = c->last_us = now_us;
  if (hops.size() > 1) {
    c->chained = true;
    dial_start(c->dial, hops, mix(salt ^ f.secret ^ ++f.draws), now_us);
  } else {
    c->connected = true;
    f.stats.routed++;
    send_routed(*f.port, from, salt, Answer::ok, "", now_us);
  }
  f.clients.push_back(std::move(c));
}

void drop(Forward& f, size_t i) {
  net::close_transport(*f.clients[i]->up);
  f.clients.erase(f.clients.begin() + static_cast<std::ptrdiff_t>(i));
}

}  // namespace

const char* answer_text(Answer a) {
  switch (a) {
    case Answer::ok:
      return "ok";
    case Answer::full:
      return "full";
    case Answer::not_allowed:
      return "not allowed";
    case Answer::bad_route:
      return "bad route";
    case Answer::unreachable:
      return "unreachable";
  }
  return "?";
}

bool parse_route(std::string_view text, std::vector<net::Address>& out, std::string& error) {
  out.clear();
  if (text.empty() || text.size() > kMostRoute) {
    error = text.empty() ? "the route is empty" : "the route is longer than 255 characters";
    return false;
  }
  size_t at = 0;
  for (;;) {
    const size_t sep = text.find('@', at);
    const std::string_view hop = text.substr(at, sep == std::string_view::npos ? sep : sep - at);
    net::Address a;
    if (!net::parse_address(hop, a) || a.port == 0) {
      error = "'" + std::string(hop) + "' in the route is not an address with a port";
      return false;
    }
    out.push_back(a);
    if (static_cast<int>(out.size()) > kMostHops) {
      error = "the route has more than " + std::to_string(kMostHops) + " hops";
      return false;
    }
    if (sep == std::string_view::npos) return true;
    at = sep + 1;
  }
}

std::string route_text(std::span<const net::Address> hops) {
  std::string s;
  for (const net::Address& a : hops) s += (s.empty() ? "" : "@") + net::to_string(a);
  return s;
}

bool is_forward(const uint8_t* data, size_t size) {
  return size >= kHead && std::equal(kMagic, kMagic + 4, data);
}

uint32_t forward_protocol() { return net::protocol_id("forward", 1); }

void dial_start(Dial& d, std::span<const net::Address> chain, uint64_t seed, int64_t now_us) {
  const int64_t retry = d.retry_us, timeout = d.timeout_us, alive = d.alive_us;
  d = Dial{};
  d.retry_us = retry;
  d.timeout_us = timeout;
  d.alive_us = alive;
  if (chain.empty()) {
    d.state = DialState::refused;
    d.answer = Answer::bad_route;
    d.why = "no address to dial";
    return;
  }
  d.first = chain[0];
  d.salt = mix(seed) | 1;
  d.started_us = now_us;
  d.next_us = now_us;
  if (chain.size() == 1) {
    d.direct = true;
    d.state = DialState::routed;
    return;
  }
  d.rest = route_text(chain.subspan(1));
  d.state = DialState::asking;
}

void dial_update(Dial& d, net::Transport& t, int64_t now_us) {
  if (d.direct || now_us < d.next_us) return;
  if (d.state == DialState::asking) {
    if (now_us - d.started_us >= d.timeout_us) {
      d.state = DialState::timed_out;
      d.answer = Answer::unreachable;
      d.why = "no answer from the forwarder at " + net::to_string(d.first) + " in " +
              std::to_string(d.timeout_us / 1000000) + " s";
      return;
    }
    send_route(t, d, now_us);
    d.sent++;
    d.next_us = now_us + d.retry_us;
  } else if (d.state == DialState::routed) {
    send_alive(t, d.first, d.salt, now_us, 0, now_us);
    d.sent++;
    d.next_us = now_us + d.alive_us;
  }
}

bool dial_read(Dial& d, const uint8_t* data, size_t size, const net::Address& from,
               int64_t now_us) {
  if (d.direct || from != d.first || !is_forward(data, size)) return false;
  net::Reader r = net::make_reader(data, size);
  r.at = 4;
  const auto kind = static_cast<FwMsg>(net::read_u8(r));
  if (net::read_u64(r) != d.salt) return false;
  d.heard++;
  switch (kind) {
    case FwMsg::cookie: {
      const uint64_t cookie = net::read_u64(r);
      if (r.bad || d.state != DialState::asking) return true;
      d.cookie = cookie;
      d.next_us = now_us;  // the route again at once, with the cookie
      return true;
    }
    case FwMsg::routed: {
      const auto a = static_cast<Answer>(net::read_u8(r));
      const uint64_t n = net::read_varint(r);
      if (r.bad || n > net::remaining(r) || d.state != DialState::asking) return true;
      if (a == Answer::ok) {
        d.state = DialState::routed;
        d.next_us = now_us + d.alive_us;
      } else {
        d.state = DialState::refused;
        d.answer = a;
        d.why.assign(reinterpret_cast<const char*>(data + r.at), static_cast<size_t>(n));
      }
      return true;
    }
    case FwMsg::alive: {
      net::read_u64(r);
      const auto echo = static_cast<int64_t>(net::read_u64(r));
      if (!r.bad && echo > 0 && echo <= now_us) d.rtt_us = now_us - echo;
      return true;
    }
    default:
      return true;
  }
}

void forward_init(Forward& f, const ForwardConfig& config, net::Transport& port,
                  OpenUpstream open_upstream) {
  forward_close(f);
  f = Forward{};
  f.config = config;
  f.port = &port;
  f.open_upstream = std::move(open_upstream);
  f.secret = mix(config.seed ^ 0x666f7277617264ull);
}

void forward_close(Forward& f) {
  for (auto& c : f.clients)
    if (c->up) net::close_transport(*c->up);
  f.clients.clear();
}

void forward_update(Forward& f, int64_t now_us) {
  if (f.port == nullptr) return;
  uint8_t buf[net::kMaxDatagram];
  uint8_t out[net::kMaxDatagram];
  net::Address from;
  int got = 0;
  bool asked = false;
  net::Status status;
  while ((got = net::transport_receive(*f.port, buf, sizeof buf, from, now_us)) >= 0) {
    const auto size = static_cast<size_t>(got);
    f.stats.bytes_in += size;
    if (net::is_stun(buf, size)) {
      const size_t n = net::stun_answer(buf, size, from, out, std::min(size, sizeof out));
      if (n > 0) {
        net::transport_send(*f.port, from, out, n, now_us);
        f.stats.stun_answers++;
      }
      continue;
    }
    if (net::is_status_ask(buf, size)) {
      if (!asked) {
        status = status_of(f, now_us);
        asked = true;
      }
      const size_t n = net::status_answer(buf, size, from, forward_protocol(), f.secret, now_us,
                                          status, out, sizeof out);
      if (n > 0) {
        net::transport_send(*f.port, from, out, n, now_us);
        f.stats.status_answers++;
      }
      continue;
    }
    if (is_forward(buf, size)) {
      net::Reader r = net::make_reader(buf, size);
      r.at = 4;
      const auto kind = static_cast<FwMsg>(net::read_u8(r));
      const uint64_t salt = net::read_u64(r);
      if (kind == FwMsg::route) {
        route_request(f, buf, size, from, salt, r, now_us);
      } else if (kind == FwMsg::alive) {
        const auto clock = static_cast<int64_t>(net::read_u64(r));
        HopClient* c = by_salt(f, salt);
        if (c == nullptr || r.bad) continue;
        if (c->from != from) {  // its NAT gave it another port: follow it
          c->from = from;
          c->name = net::to_string(from);
          f.stats.rebinds++;
        }
        c->last_us = now_us;
        send_alive(*f.port, from, salt, now_us, clock, now_us);
      } else {
        f.stats.unknown++;
      }
      continue;
    }
    HopClient* c = by_address(f, from);
    if (c == nullptr) {
      f.stats.strays++;
      continue;
    }
    net::transport_send(*c->up, c->to, buf, size, now_us);
    c->packets_up++;
    c->bytes_up += size;
    c->last_us = now_us;
  }
  for (size_t i = 0; i < f.clients.size();) {
    HopClient& c = *f.clients[i];
    while ((got = net::transport_receive(*c.up, buf, sizeof buf, from, now_us)) >= 0) {
      const auto size = static_cast<size_t>(got);
      if (c.chained && dial_read(c.dial, buf, size, from, now_us)) continue;
      if (!c.connected || from != c.to) {
        f.stats.strays++;
        continue;
      }
      net::transport_send(*f.port, c.from, buf, size, now_us);
      f.stats.bytes_out += size;
      c.packets_down++;
      c.bytes_down += size;
      c.last_us = now_us;
    }
    if (c.chained) {
      dial_update(c.dial, *c.up, now_us);
      if (c.dial.state == DialState::routed && !c.connected) {
        c.connected = true;
        f.stats.routed++;
        send_routed(*f.port, c.from, c.salt, Answer::ok, "", now_us);
      } else if (c.dial.state == DialState::refused || c.dial.state == DialState::timed_out) {
        f.stats.refused++;
        send_routed(*f.port, c.from, c.salt, c.dial.answer, c.dial.why, now_us);
        drop(f, i);
        continue;
      }
    }
    if (now_us - c.last_us >= f.config.idle_us) {
      f.stats.idle_drops++;
      drop(f, i);
      continue;
    }
    i++;
  }
}

}  // namespace ge::forward
