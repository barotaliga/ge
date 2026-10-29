// The server directory (net/directory.hpp): the LAN probe, the list's pages, the pings, the view.
//
// ge:summary The server directory: LAN adverts and listing pages merged by session, pings, views
// ge:keywords net directory server-browser listing page ping merge filter sort
// ge:skill net
// ge:test tests/net_directory_test.cpp
#include "net/directory.hpp"

#include <algorithm>
#include <cstring>

#include "net/reach_wire.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

uint64_t draw64(core::Rng& rng) {
  const auto hi = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  const auto lo = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  return (hi << 32 | lo) | 1;  // never 0, the "none in flight" nonce
}

void ask_page(Directory& d, int page, int64_t now_us) {
  uint8_t out[reach_wire::kListAsk];
  Writer w = make_writer(out, sizeof out);
  reach_wire::write_start(w, reach_wire::kList);
  write_u32(w, d.config.protocol);
  write_u16(w, static_cast<uint16_t>(page));
  write_pad_to(w, reach_wire::kListAsk);  // the biggest page's size: no answer outweighs it
  transport_send(*d.transport, d.config.rendezvous, out, w.size, now_us);
  d.page_asked_us = now_us;
  d.stats.asks++;
}

// The first page not yet in (the list's page count known), or -1.
int missing_page(const Directory& d) {
  for (int p = 0; p < d.pages; p++) {
    if (!d.page_have[static_cast<size_t>(p)]) return p;
  }
  return -1;
}

// Every page not yet in (or the first, while their count is unknown): one round of asks.
void ask_missing(Directory& d, int64_t now_us) {
  if (!d.pages) {
    ask_page(d, 0, now_us);
  } else {
    for (int p = 0; p < d.pages; p++) {
      if (!d.page_have[static_cast<size_t>(p)]) ask_page(d, p, now_us);
    }
  }
  d.page_tries++;
}

// The LAN's next probe, when one is due.
void lan_tick(Directory& d, int64_t now_us) {
  if (d.config.lan.family != Family::none && d.lan_sent < d.config.lan_probes &&
      now_us >= d.refreshed_us + d.lan_sent * d.config.lan_every_us) {
    lan_probe(*d.transport, d.config.lan, d.config.protocol, d.nonce, now_us);
    d.lan_sent++;
    d.stats.probes++;
  }
}

DirectoryEntry* find(Directory& d, uint64_t session, const Address& lan) {
  for (DirectoryEntry& e : d.entries) {
    if (session && e.advert.session == session) return &e;
    if (!session && (e.from & kFoundLan) && e.lan == lan) return &e;
  }
  return nullptr;
}

DirectoryEntry& entry_for(Directory& d, uint64_t session, const Address& lan) {
  DirectoryEntry* e = find(d, session, lan);
  if (e) return *e;
  d.entries.push_back({});
  return d.entries.back();
}

void on_listing(Directory& d, Reader& r, int64_t now_us) {
  const uint32_t protocol = read_u32(r);
  const int page = read_u16(r), pages = read_u16(r), count = read_u8(r);
  if (r.bad || protocol != d.config.protocol || pages == 0 || page >= pages) {
    d.stats.foreign++;
    return;
  }
  if (d.pages == 0) {
    d.pages = pages;
    d.page_have.assign(static_cast<size_t>(pages), 0);
  }
  if (page >= d.pages || d.page_have[static_cast<size_t>(page)]) {
    d.stats.stale++;
    return;
  }
  for (int i = 0; i < count; i++) {
    const Address at = reach_wire::read_address(r);
    Advert a;
    a.protocol = protocol;
    if (r.bad || !read_advert_body(r, a) || !a.session) {
      d.stats.foreign++;
      return;
    }
    DirectoryEntry& e = entry_for(d, a.session, Address{});
    if (!(e.from & kFoundLan)) e.advert = a;  // a LAN advert is fresher
    e.from |= kFoundListed;
    e.internet = at;
    e.seen_us = now_us;
  }
  d.page_have[static_cast<size_t>(page)] = 1;
  d.stats.pages++;
  if (missing_page(d) < 0) {
    d.list_done = true;
  } else if (page == 0) {
    d.page_tries = 0;
    ask_missing(d, now_us);  // the rest at once, now their count is known
  }
}

void on_advert(Directory& d, const Address& from, const uint8_t* data, size_t size,
               int64_t now_us) {
  uint64_t nonce = 0;
  Advert a;
  if (!lan_read_advert(data, size, d.config.protocol, nonce, a)) {
    d.stats.foreign++;
    return;
  }
  // A ping's answer: its entry's nonce, the ping still in flight.
  for (DirectoryEntry& e : d.entries) {
    for (int k = 0; k < e.pings_sent; k++) {
      if (!e.ping_at_us[k] || nonce != e.ping_nonce[k]) continue;
      const int ms = static_cast<int>((now_us - e.ping_at_us[k] + 500) / 1000);
      if (e.ping_ms < 0 || ms < e.ping_ms) e.ping_ms = ms;
      e.ping_at_us[k] = 0;
      e.pings_answered++;
      e.advert.seated = a.seated;
      e.advert.phase = a.phase;
      e.seen_us = now_us;
      d.stats.pongs++;
      return;
    }
  }
  if (nonce != d.nonce) {
    d.stats.stale++;
    return;
  }
  Address lan = from;
  lan.port = a.port;
  DirectoryEntry& e = entry_for(d, a.session, lan);
  e.advert = a;
  e.from |= kFoundLan;
  e.lan = lan;
  e.seen_us = now_us;
  d.stats.adverts++;
}

int pings_of(const Directory& d) {
  return d.config.pings < 0 ? 0 : (d.config.pings > kMaxPings ? kMaxPings : d.config.pings);
}

void ping(Directory& d, int64_t now_us) {
  for (DirectoryEntry& e : d.entries) {
    for (int k = 0; k < e.pings_sent; k++) {
      if (e.ping_at_us[k] && now_us - e.ping_at_us[k] > d.config.ping_timeout_us)
        e.ping_at_us[k] = 0;  // lost
    }
  }
  if (now_us < d.next_ping_us) return;
  // Round the list: the first entry with the fewest pings sent.
  DirectoryEntry* next = nullptr;
  for (DirectoryEntry& e : d.entries) {
    if (e.pings_sent < pings_of(d) && (!next || e.pings_sent < next->pings_sent)) next = &e;
  }
  if (!next) return;
  const int k = next->pings_sent++;
  next->ping_nonce[k] = draw64(d.rng);
  next->ping_at_us[k] = now_us;
  lan_probe(*d.transport, (next->from & kFoundLan) ? next->lan : next->internet, d.config.protocol,
            next->ping_nonce[k], now_us);
  d.stats.pings++;
  d.next_ping_us = now_us + d.config.ping_gap_us;
}

char fold(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool contains(const char* name, const char* part) {
  const size_t n = std::strlen(name), m = std::strlen(part);
  for (size_t i = 0; i + m <= n; i++) {
    size_t k = 0;
    while (k < m && fold(name[i + k]) == fold(part[k])) k++;
    if (k == m) return true;
  }
  return m == 0;
}

}  // namespace

void directory_init(Directory& d, const DirectoryConfig& config, Transport& transport) {
  d.config = config;
  d.transport = &transport;
  d.rng = core::make_rng(static_cast<uint32_t>(config.seed ^ (config.seed >> 32)));
  d.refreshed_us = -1;
  d.entries.clear();
  d.stats = DirectoryStats{};
}

void directory_refresh(Directory& d, int64_t now_us) {
  d.entries.clear();
  d.nonce = draw64(d.rng);
  d.refreshed_us = now_us;
  d.next_ping_us = now_us;
  d.pages = 0;
  d.page_have.clear();
  d.page_tries = 0;
  d.list_done = d.config.rendezvous.family == Family::none;
  d.lan_sent = 0;
  lan_tick(d, now_us);
  if (!d.list_done) ask_missing(d, now_us);
}

void directory_update(Directory& d, int64_t now_us) {
  if (!d.transport) return;
  Address from;
  for (;;) {
    const int n = transport_receive(*d.transport, d.buffer, sizeof d.buffer, from, now_us);
    if (n < 0) break;
    Reader r = make_reader(d.buffer, static_cast<size_t>(n));
    reach_wire::Kind kind{};
    if (reach_wire::read_start(r, kind)) {
      if (kind == reach_wire::kListing && from == d.config.rendezvous && !d.list_done)
        on_listing(d, r, now_us);
      else
        d.stats.foreign++;
      continue;
    }
    on_advert(d, from, d.buffer, static_cast<size_t>(n), now_us);
  }
  if (d.refreshed_us < 0) return;
  lan_tick(d, now_us);
  if (!d.list_done && now_us - d.page_asked_us >= d.config.ask_us) {
    if (d.page_tries >= d.config.asks)
      d.list_done = true;  // the server is not answering: the LAN's games still show
    else
      ask_missing(d, now_us);
  }
  ping(d, now_us);
}

bool directory_done(const Directory& d, int64_t now_us) {
  if (d.refreshed_us < 0 || !d.list_done) return false;
  if (d.config.lan.family != Family::none && now_us - d.refreshed_us < d.config.lan_wait_us)
    return false;
  for (const DirectoryEntry& e : d.entries) {
    if (e.pings_sent < pings_of(d)) return false;
    for (int k = 0; k < e.pings_sent; k++) {
      if (e.ping_at_us[k]) return false;
    }
  }
  return true;
}

const char* directory_sort_name(DirectorySort s) {
  switch (s) {
    case DirectorySort::ping:
      return "ping";
    case DirectorySort::players:
      return "players";
    case DirectorySort::name:
      return "name";
  }
  return "?";
}

void directory_view(const Directory& d, const DirectoryFilter& f, DirectorySort sort,
                    std::vector<int>& rows) {
  rows.clear();
  for (size_t i = 0; i < d.entries.size(); i++) {
    const DirectoryEntry& e = d.entries[i];
    const Advert& a = e.advert;
    if (!(e.from & f.from)) continue;
    if (f.hide_full && a.seated >= a.max_players) continue;
    if (f.hide_empty && a.seated == 0) continue;
    if (f.hide_playing && a.phase == SessionPhase::playing) continue;
    if (f.hide_locked && (a.flags & kAdvertLocked)) continue;
    if (f.max_ping_ms > 0 && (e.ping_ms < 0 || e.ping_ms > f.max_ping_ms)) continue;
    if (f.name_has && !contains(a.name, f.name_has)) continue;
    rows.push_back(static_cast<int>(i));
  }
  const auto by_name = [&](const DirectoryEntry& x, const DirectoryEntry& y) {
    const int c = std::strcmp(x.advert.name, y.advert.name);
    if (c != 0) return c < 0;
    return x.advert.session < y.advert.session;
  };
  std::stable_sort(rows.begin(), rows.end(), [&](int ia, int ib) {
    const DirectoryEntry& x = d.entries[static_cast<size_t>(ia)];
    const DirectoryEntry& y = d.entries[static_cast<size_t>(ib)];
    if (sort == DirectorySort::ping) {
      const int px = x.ping_ms < 0 ? INT32_MAX : x.ping_ms,
                py = y.ping_ms < 0 ? INT32_MAX : y.ping_ms;
      if (px != py) return px < py;
    } else if (sort == DirectorySort::players) {
      if (x.advert.seated != y.advert.seated) return x.advert.seated > y.advert.seated;
    }
    return by_name(x, y);
  });
}

JoinRoute directory_route(const DirectoryEntry& e) {
  JoinRoute r;
  if (e.from & kFoundLan) {
    r.lan = true;
    r.address = e.lan;
  } else {
    r.session = e.advert.session;
  }
  return r;
}

}  // namespace ge::net
