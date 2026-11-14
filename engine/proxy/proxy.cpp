// Proxy (proxy.hpp): the link frames, repeats, packing and coding, the near and the far proxies'
// sessions.
//
// ge:summary Proxy: link frames, repeats, the XOR and zero-run packing, near and far sessions
// ge:keywords proxy link frame repeat pack window session open cookie
// ge:skill net
// ge:test tests/proxy_test.cpp
#include "proxy/proxy.hpp"

#include <algorithm>
#include <cstring>

#include "net/wire.hpp"

namespace ge::proxy {

namespace {

// A datagram's frame says by its kind how the datagram is written: as it came, packed or coded.
enum class Kind : uint8_t {
  data = 1,
  ack = 2,
  open = 3,
  cookie = 4,
  opened = 5,
  packed = 6,
  coded = 7
};
constexpr uint8_t kMark = 0xA0;
constexpr uint32_t kPackWithin = 48;  // a reference this far back still sits in the other's ring
constexpr size_t kMostWhy = 120;
// A number from its low 16 bits: the one nearest `near` (the newest this end knows of); 0 when it
// would fall at or below zero.
uint32_t widen(uint32_t near, uint16_t low) {
  const int16_t step =
      static_cast<int16_t>(static_cast<uint16_t>(low - static_cast<uint16_t>(near)));
  const int64_t v = int64_t{near} + step;
  return v > 0 && v <= int64_t{UINT32_MAX} ? static_cast<uint32_t>(v) : 0;
}

uint64_t mix(uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

net::Writer frame(uint8_t* buf, size_t cap, Kind kind, uint32_t session) {
  net::Writer w = net::make_writer(buf, cap);
  net::write_u8(w, static_cast<uint8_t>(kMark | static_cast<uint8_t>(kind)));
  net::write_u32(w, session);
  return w;
}

bool read_kind(net::Reader& r, Kind& kind, uint32_t& session) {
  const uint8_t b = net::read_u8(r);
  session = net::read_u32(r);
  if (r.bad || (b & 0xF0) != kMark) return false;
  kind = static_cast<Kind>(b & 0x0F);
  return true;
}

size_t put_varint(uint8_t* out, size_t at, size_t cap, uint64_t v) {
  while (v >= 0x80) {
    if (at >= cap) return cap + 1;
    out[at++] = static_cast<uint8_t>(v | 0x80);
    v >>= 7;
  }
  if (at >= cap) return cap + 1;
  out[at++] = static_cast<uint8_t>(v);
  return at;
}

void keep(Kept& k, uint32_t seq, std::span<const uint8_t> bytes) {
  k.seq = seq;
  k.size = static_cast<uint16_t>(bytes.size());
  std::memcpy(k.bytes.data(), bytes.data(), bytes.size());
}

std::span<const uint8_t> kept(const Kept& k) { return {k.bytes.data(), k.size}; }

void emit(LinkEnd& e, const net::Writer& w, int64_t now_us, const FrameOut& out) {
  if (w.overflow) return;
  out({w.data, w.size});
  e.stats.frames_out++;
  e.stats.bytes_out += w.size;
  e.last_out_us = now_us;
}

bool allowed(const ProxyConfig& c, const net::Address& to) {
  if (c.open) return true;
  for (const net::Address& a : c.allow) {
    net::Address host = to;
    if (a.port == 0) host.port = 0;
    if (a == host) return true;
  }
  return false;
}

uint64_t make_cookie(const ProxyFar& f, const net::Address& from, uint64_t salt) {
  const uint64_t c = mix(f.secret ^ mix(net::address_hash(from) ^ mix(salt)));
  return c ? c : 1;
}

}  // namespace

size_t pack_datagram(std::span<const uint8_t> d, std::span<const uint8_t> ref, uint8_t* out,
                     size_t capacity) {
  const size_t cap = std::min(capacity, d.size() > 0 ? d.size() - 1 : 0);  // must get smaller
  size_t at = put_varint(out, 0, cap, d.size());
  size_t i = 0;
  const auto x = [&](size_t k) {
    return k < ref.size() ? static_cast<uint8_t>(d[k] ^ ref[k]) : d[k];
  };
  while (i < d.size() && at <= cap) {
    size_t zeros = 0;
    while (i + zeros < d.size() && x(i + zeros) == 0) zeros++;
    size_t lit = 0;
    // A literal run ends at the next run of three or more zeros.
    while (i + zeros + lit < d.size()) {
      const size_t k = i + zeros + lit;
      if (x(k) == 0 && k + 2 < d.size() && x(k + 1) == 0 && x(k + 2) == 0) break;
      lit++;
    }
    at = put_varint(out, at, cap, zeros);
    at = put_varint(out, at, cap, lit);
    if (at > cap || at + lit > cap) return 0;
    for (size_t k = 0; k < lit; k++) out[at + k] = x(i + zeros + k);
    at += lit;
    i += zeros + lit;
  }
  return at <= cap ? at : 0;
}

bool unpack_datagram(std::span<const uint8_t> packed, std::span<const uint8_t> ref,
                     std::vector<uint8_t>& out) {
  net::Reader r = net::make_reader(packed.data(), packed.size());
  const uint64_t size = net::read_varint(r);
  if (r.bad || size > kMostDatagram) return false;
  out.assign(static_cast<size_t>(size), 0);
  size_t i = 0;
  while (i < out.size()) {
    const uint64_t zeros = net::read_varint(r);
    const uint64_t lit = net::read_varint(r);
    if (r.bad || zeros + lit == 0 || zeros + lit > out.size() - i || lit > net::remaining(r))
      return false;
    i += static_cast<size_t>(zeros);
    for (uint64_t k = 0; k < lit; k++) out[i++] = net::read_u8(r);
  }
  if (net::remaining(r) != 0) return false;
  for (size_t k = 0; k < out.size() && k < ref.size(); k++) out[k] ^= ref[k];
  return true;
}

uint32_t frame_session(std::span<const uint8_t> f) {
  net::Reader r = net::make_reader(f.data(), f.size());
  Kind kind{};
  uint32_t session = 0;
  return read_kind(r, kind, session) ? session : 0;
}

namespace {

// Every copy now due, oldest first: sent before a newer datagram so the far end receives them in
// the order the game sent them (a copy behind a newer datagram would arrive out of order, and a
// game that drops what is older than the newest it has would lose it all the same).
void send_due_copies(LinkEnd& e, int64_t now_us, const FrameOut& out) {
  for (size_t i = 0; i < e.copies.size();) {
    if (e.copies[i].at_us > now_us) {
      i++;
      continue;
    }
    out({e.copies[i].bytes.data(), e.copies[i].size});
    e.stats.frames_out++;
    e.stats.bytes_out += e.copies[i].size;
    e.last_out_us = now_us;
    e.copies.erase(e.copies.begin() + static_cast<std::ptrdiff_t>(i));
  }
}

}  // namespace

void link_send(LinkEnd& e, std::span<const uint8_t> d, int64_t now_us, const FrameOut& out) {
  if (d.size() > kMostDatagram) return;
  send_due_copies(e, now_us, out);
  const uint32_t seq = e.next_seq++;
  const Kept& ref = e.sent[e.acked % kKept];
  const bool held = e.acked != 0 && ref.seq == e.acked && seq - e.acked < kPackWithin;
  size_t size = 0;
  Kind kind = Kind::data;
  uint8_t pbuf[net::kMaxDatagram];
  if (e.config.code) {
    if (e.sent_models.empty()) e.sent_models.resize(kKept);
    LinkModels& now = e.sent_models[seq % kKept];
    if (held) {
      LinkModels m = e.sent_models[e.acked % kKept];
      size = code_datagram(d, kept(ref), m, pbuf, d.size());
      if (size) {
        now = m;
        kind = Kind::coded;
      }
    }
    if (!size) now = LinkModels{};
  }
  if (!size && e.config.pack && held) {
    size = pack_datagram(d, kept(ref), pbuf, d.size());
    if (size) kind = Kind::packed;
  }
  uint8_t buf[net::kMaxDatagram];
  net::Writer w = frame(buf, sizeof buf, kind, e.session);
  net::write_u16(w, static_cast<uint16_t>(seq));
  net::write_u16(w, static_cast<uint16_t>(e.newest));
  if (kind != Kind::data) {
    net::write_u8(w, static_cast<uint8_t>(seq - e.acked));
    net::write_bytes(w, pbuf, size);
    (kind == Kind::coded ? e.stats.coded : e.stats.packed)++;
  } else {
    net::write_bytes(w, d.data(), d.size());
  }
  keep(e.sent[seq % kKept], seq, d);
  e.stats.datagrams_out++;
  e.stats.payload_out += d.size();
  emit(e, w, now_us, out);
  for (int k = 1; k < std::clamp(e.config.repeat, 1, 5); k++) {
    LinkEnd::Copy c;
    c.at_us = now_us + e.config.gap_us * k;
    c.size = static_cast<uint16_t>(w.size);
    std::memcpy(c.bytes.data(), buf, w.size);
    e.copies.push_back(c);
  }
}

void link_update(LinkEnd& e, int64_t now_us, const FrameOut& out) {
  send_due_copies(e, now_us, out);
  if (e.newest != 0 && now_us - e.last_out_us >= e.config.ack_us) {
    uint8_t buf[16];
    net::Writer w = frame(buf, sizeof buf, Kind::ack, e.session);
    net::write_u16(w, static_cast<uint16_t>(e.newest));
    emit(e, w, now_us, out);
  }
}

int link_receive(LinkEnd& e, std::span<const uint8_t> f, std::vector<uint8_t>& datagram) {
  net::Reader r = net::make_reader(f.data(), f.size());
  Kind kind{};
  uint32_t session = 0;
  if (!read_kind(r, kind, session) ||
      (kind != Kind::data && kind != Kind::ack && kind != Kind::packed && kind != Kind::coded))
    return -1;
  e.stats.frames_in++;
  // An ack names a datagram this end sent: the one nearest the newest sent with those low bits.
  const auto take_ack = [&](uint16_t low) {
    const uint32_t ack = widen(e.next_seq - 1, low);
    if (ack > e.acked && ack < e.next_seq) e.acked = ack;
  };
  if (kind == Kind::ack) {
    const uint16_t ack = net::read_u16(r);
    if (!r.bad) take_ack(ack);
    return 0;
  }
  const uint32_t seq = widen(e.newest, net::read_u16(r));
  const uint16_t ack = net::read_u16(r);
  if (r.bad || seq == 0) {
    e.stats.broken++;
    return 0;
  }
  take_ack(ack);
  // Seen already, or too old to tell?
  if (seq <= e.newest) {
    const uint32_t back = e.newest - seq;
    if (back >= 64) {
      e.stats.late++;
      return 0;
    }
    if ((e.window >> back) & 1) {
      e.stats.duplicates++;
      return 0;
    }
  }
  if (kind == Kind::coded && e.got_models.empty()) e.got_models.resize(kKept);  // all fresh so far
  if (kind != Kind::data) {
    const uint32_t ref = seq - net::read_u8(r);
    const Kept& k = e.got[ref % kKept];
    if (r.bad || ref == seq || k.seq != ref) {
      e.stats.no_ref++;
      return 0;
    }
    const std::span<const uint8_t> body{f.data() + r.at, f.size() - r.at};
    bool ok = false;
    if (kind == Kind::coded) {
      LinkModels m = e.got_models[ref % kKept];
      ok = decode_datagram(body, kept(k), m, datagram, kMostDatagram);
      if (ok) e.got_models[seq % kKept] = m;
    } else {
      ok = unpack_datagram(body, kept(k), datagram);
    }
    if (!ok) {
      e.stats.broken++;
      return 0;
    }
  } else {
    datagram.assign(f.begin() + static_cast<std::ptrdiff_t>(r.at), f.end());
  }
  if (kind != Kind::coded && !e.got_models.empty()) e.got_models[seq % kKept] = LinkModels{};
  if (seq > e.newest) {
    const uint32_t shift = seq - e.newest;
    e.window = shift >= 64 ? 0 : e.window << shift;
    e.newest = seq;
    e.window |= 1;
  } else {
    e.window |= uint64_t{1} << (e.newest - seq);
  }
  keep(e.got[seq % kKept], seq, datagram);
  e.stats.datagrams_in++;
  return 1;
}

// THE NEAR PROXY.

void proxy_near_init(ProxyNear& n, const ProxyConfig& config, net::Transport& client_port,
                     net::Transport& link_port, const net::Address& far,
                     const net::Address& server) {
  n = ProxyNear{};
  n.config = config;
  n.client_port = &client_port;
  n.link_port = &link_port;
  n.far = far;
  n.server = server;
}

namespace {

void send_open(ProxyNear& n, NearClient& c, int64_t now_us) {
  uint8_t buf[kOpenSize];
  net::Writer w = frame(buf, sizeof buf, Kind::open, c.link.session);
  net::write_u64(w, c.salt);
  net::write_u64(w, c.cookie);
  const std::string to = net::to_string(n.server);
  net::write_varint(w, to.size());
  net::write_bytes(w, to.data(), to.size());
  net::write_pad_to(w, kOpenSize);
  if (!w.overflow) net::transport_send(*n.link_port, n.far, buf, w.size, now_us);
}

NearClient* near_by_session(ProxyNear& n, uint32_t session) {
  for (auto& c : n.clients)
    if (c->link.session == session) return c.get();
  return nullptr;
}

}  // namespace

void proxy_near_update(ProxyNear& n, int64_t now_us) {
  uint8_t buf[net::kMaxDatagram];
  net::Address from;
  int got = 0;
  // The game's clients.
  while ((got = net::transport_receive(*n.client_port, buf, sizeof buf, from, now_us)) >= 0) {
    NearClient* c = nullptr;
    for (auto& k : n.clients)
      if (k->from == from) c = k.get();
    if (c == nullptr) {
      if (static_cast<int>(n.clients.size()) >= n.config.max_sessions) {
        n.strays++;
        continue;
      }
      auto k = std::make_unique<NearClient>();
      k->from = from;
      k->salt = mix(n.config.seed ^ mix(net::address_hash(from)) ^ ++n.draws) | 1;
      k->link.config = n.config.link;
      k->link.session = static_cast<uint32_t>(k->salt >> 32) | 1;
      k->since_us = k->last_us = now_us;
      c = k.get();
      n.clients.push_back(std::move(k));
    }
    c->last_us = now_us;
    if (!c->opened) continue;  // the game's resend brings it again once the session is up
    if (n.config.tap)
      n.config.tap(
          {c->link.session, c->from, n.server, true, {buf, static_cast<size_t>(got)}, now_us});
    link_send(c->link, {buf, static_cast<size_t>(got)}, now_us, [&](std::span<const uint8_t> f) {
      net::transport_send(*n.link_port, n.far, f.data(), f.size(), now_us);
    });
  }
  // The far proxy.
  std::vector<uint8_t> datagram;
  while ((got = net::transport_receive(*n.link_port, buf, sizeof buf, from, now_us)) >= 0) {
    if (from != n.far) {
      n.strays++;
      continue;
    }
    const std::span<const uint8_t> f{buf, static_cast<size_t>(got)};
    net::Reader r = net::make_reader(buf, f.size());
    Kind kind{};
    uint32_t session = 0;
    if (!read_kind(r, kind, session)) continue;
    NearClient* c = near_by_session(n, session);
    if (c == nullptr) continue;
    if (kind == Kind::cookie) {
      const uint64_t salt = net::read_u64(r);
      const uint64_t cookie = net::read_u64(r);
      if (!r.bad && salt == c->salt && !c->opened) {
        c->cookie = cookie;
        c->next_open_us = now_us;
      }
    } else if (kind == Kind::opened) {
      const uint64_t salt = net::read_u64(r);
      const uint8_t answer = net::read_u8(r);
      const uint64_t len = net::read_varint(r);
      if (r.bad || salt != c->salt || len > net::remaining(r) || c->opened) continue;
      if (answer == 0) {
        c->opened = true;
      } else {
        c->refused = true;
        c->why.assign(reinterpret_cast<const char*>(buf + r.at), static_cast<size_t>(len));
      }
    } else if (link_receive(c->link, f, datagram) == 1) {
      if (n.config.tap) n.config.tap({c->link.session, c->from, n.server, false, datagram, now_us});
      net::transport_send(*n.client_port, c->from, datagram.data(), datagram.size(), now_us);
      c->last_us = now_us;
    }
  }
  for (size_t i = 0; i < n.clients.size();) {
    NearClient& c = *n.clients[i];
    if (!c.opened && !c.refused && now_us >= c.next_open_us) {
      if (now_us - c.since_us >= n.config.open_timeout_us) {
        c.refused = true;
        c.why = "no answer from the far proxy at " + net::to_string(n.far);
      } else {
        send_open(n, c, now_us);
        c.next_open_us = now_us + n.config.retry_us;
      }
    }
    if (c.opened)
      link_update(c.link, now_us, [&](std::span<const uint8_t> f) {
        net::transport_send(*n.link_port, n.far, f.data(), f.size(), now_us);
      });
    if (c.refused || now_us - c.last_us >= n.config.idle_us) {
      (c.refused ? n.refused : n.idle_drops)++;
      if (c.refused) n.last_refusal = c.why;
      n.clients.erase(n.clients.begin() + static_cast<std::ptrdiff_t>(i));
      continue;
    }
    i++;
  }
}

// THE FAR PROXY.

void proxy_far_init(ProxyFar& f, const ProxyConfig& config, net::Transport& link_port,
                    OpenUpstream open_upstream) {
  proxy_far_close(f);
  f = ProxyFar{};
  f.config = config;
  f.link_port = &link_port;
  f.open_upstream = std::move(open_upstream);
  f.secret = mix(config.seed ^ 0x70726f7879ull);
}

void proxy_far_close(ProxyFar& f) {
  for (auto& s : f.sessions)
    if (s->up) net::close_transport(*s->up);
  f.sessions.clear();
}

namespace {

void far_opened(ProxyFar& f, const net::Address& to, uint32_t session, uint64_t salt,
                uint8_t answer, std::string_view why, int64_t now_us) {
  uint8_t buf[kOpenSize];
  net::Writer w = frame(buf, sizeof buf, Kind::opened, session);
  net::write_u64(w, salt);
  net::write_u8(w, answer);
  why = why.substr(0, kMostWhy);
  net::write_varint(w, why.size());
  net::write_bytes(w, why.data(), why.size());
  if (!w.overflow) net::transport_send(*f.link_port, to, buf, w.size, now_us);
}

void far_open(ProxyFar& f, const net::Address& from, uint32_t session, net::Reader& r, size_t size,
              int64_t now_us) {
  const uint64_t salt = net::read_u64(r);
  const uint64_t cookie = net::read_u64(r);
  const uint64_t len = net::read_varint(r);
  if (r.bad || len > net::remaining(r) || size < kOpenSize) return;
  const std::string text(reinterpret_cast<const char*>(r.data + r.at), static_cast<size_t>(len));
  if (cookie != make_cookie(f, from, salt)) {
    uint8_t buf[32];
    net::Writer w = frame(buf, sizeof buf, Kind::cookie, session);
    net::write_u64(w, salt);
    net::write_u64(w, make_cookie(f, from, salt));
    net::transport_send(*f.link_port, from, buf, w.size, now_us);
    f.cookies++;
    return;
  }
  for (auto& s : f.sessions)
    if (s->near == from && s->link.session == session) {
      far_opened(f, from, session, salt, 0, "", now_us);  // asked again: the answer was lost
      return;
    }
  net::Address server;
  const auto refuse = [&](const std::string& why) {
    f.refused++;
    far_opened(f, from, session, salt, 1, why, now_us);
  };
  if (!net::parse_address(text, server)) return refuse("'" + text + "' is not an address");
  if (!allowed(f.config, server))
    return refuse(f.config.name + " does not reach " + net::to_string(server));
  if (static_cast<int>(f.sessions.size()) >= f.config.max_sessions)
    return refuse(f.config.name + " is full");
  auto s = std::make_unique<FarSession>();
  s->up = std::make_unique<net::Transport>();
  if (!f.open_upstream || !f.open_upstream(*s->up))
    return refuse(f.config.name + " has no socket free");
  s->near = from;
  s->server = server;
  s->link.config = f.config.link;
  s->link.session = session;
  s->since_us = s->last_us = now_us;
  f.sessions.push_back(std::move(s));
  far_opened(f, from, session, salt, 0, "", now_us);
}

}  // namespace

void proxy_far_update(ProxyFar& f, int64_t now_us) {
  uint8_t buf[net::kMaxDatagram];
  net::Address from;
  int got = 0;
  std::vector<uint8_t> datagram;
  while ((got = net::transport_receive(*f.link_port, buf, sizeof buf, from, now_us)) >= 0) {
    const std::span<const uint8_t> fr{buf, static_cast<size_t>(got)};
    net::Reader r = net::make_reader(buf, fr.size());
    Kind kind{};
    uint32_t session = 0;
    if (!read_kind(r, kind, session)) {
      f.strays++;
      continue;
    }
    if (kind == Kind::open) {
      far_open(f, from, session, r, fr.size(), now_us);
      continue;
    }
    FarSession* s = nullptr;
    for (auto& k : f.sessions)
      if (k->near == from && k->link.session == session) s = k.get();
    if (s == nullptr) {
      f.strays++;
      continue;
    }
    s->last_us = now_us;
    if (link_receive(s->link, fr, datagram) == 1) {
      if (f.config.tap) f.config.tap({s->link.session, s->near, s->server, true, datagram, now_us});
      net::transport_send(*s->up, s->server, datagram.data(), datagram.size(), now_us);
    }
  }
  for (size_t i = 0; i < f.sessions.size();) {
    FarSession& s = *f.sessions[i];
    const auto out = [&](std::span<const uint8_t> fr) {
      net::transport_send(*f.link_port, s.near, fr.data(), fr.size(), now_us);
    };
    while ((got = net::transport_receive(*s.up, buf, sizeof buf, from, now_us)) >= 0) {
      if (from != s.server) continue;
      if (f.config.tap)
        f.config.tap(
            {s.link.session, s.near, s.server, false, {buf, static_cast<size_t>(got)}, now_us});
      link_send(s.link, {buf, static_cast<size_t>(got)}, now_us, out);
      s.last_us = now_us;
    }
    link_update(s.link, now_us, out);
    if (now_us - s.last_us >= f.config.idle_us) {
      f.idle_drops++;
      net::close_transport(*s.up);
      f.sessions.erase(f.sessions.begin() + static_cast<std::ptrdiff_t>(i));
      continue;
    }
    i++;
  }
}

}  // namespace ge::proxy
