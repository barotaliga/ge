// The hub's poller, defined: the list round, the status asks and their cookie round, the index.
//
// ge:summary The hub's poller defined: the list round, status asks with their cookies, drops
// ge:keywords hub poller list status cookie retry drop index
// ge:skill net
// ge:test tests/hub_poller_test.cpp
#include "hub/poller.hpp"

#include <algorithm>

#include "forward/forward.hpp"

namespace ge::hub {

namespace gn = ge::net;

namespace {

uint64_t draw64(uint64_t& state) {
  uint64_t x = (state += 0x9e3779b97f4a7c15ull);
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  x ^= x >> 31;
  return x ? x : 1;
}

}  // namespace

uint64_t record_hash(const gn::Status& s) {
  uint8_t buf[gn::kMaxDatagram];
  const size_t n = gn::status_write(s, buf, sizeof buf);
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) h = (h ^ buf[i]) * 1099511628211ull;
  return h;
}

namespace {

int64_t period(const Poller& p, const HubNode& n) {
  return n.have && humans(n.status) > 0 ? p.config.busy_us : p.config.status_us;
}

void send_ask(Poller& p, gn::Transport& t, HubNode& n, int64_t now_us) {
  uint8_t ask[gn::kStatusAskSize];
  // A forwarder answers on the forward's own protocol, whatever game it serves.
  const uint32_t protocol =
      n.kind == gn::NodeKind::forward ? ge::forward::forward_protocol() : p.config.protocol;
  gn::status_ask(protocol, n.nonce, n.cookie, ask);
  gn::transport_send(t, n.address, ask, sizeof ask, now_us);
  n.asked_us = now_us;
  p.stats.asks++;
}

HubNode* find(Poller& p, const gn::Address& a) {
  for (HubNode& n : p.nodes) {
    if (n.address == a) return &n;
  }
  return nullptr;
}

// The finished list read into the index: listed flags, new nodes added at once.
void take_list(Poller& p, int64_t now_us) {
  for (HubNode& n : p.nodes) n.listed = false;
  for (const gn::MasterEntry& e : p.query.found) {
    if ((gn::kind_bit(e.kind) & p.config.kinds) == 0 || e.protocol != p.config.protocol) continue;
    p.stats.list_entries++;
    HubNode* n = find(p, e.address);
    if (!n) {
      if (static_cast<int>(p.nodes.size()) >= p.config.max_nodes) continue;
      p.nodes.push_back({});
      n = &p.nodes.back();
      n->address = e.address;
      n->first_us = now_us;
      n->due_us = now_us;
      n->nonce = draw64(p.rng);
      p.stats.added++;
      p.serial++;
    } else if (n->browsed) {  // the engine's master lists it now: its own record from here on
      n->browsed = false;
      n->due_us = now_us;
      n->nonce = draw64(p.rng);
      p.serial++;
    }
    n->kind = e.kind;
    n->version = e.version;
    n->listed = true;
  }
  p.stats.lists++;
  p.listed_us = now_us;
  p.querying = false;
  p.next_list_us = now_us + p.config.list_us;
}

}  // namespace

int humans(const gn::Status& s) {
  int n = 0;
  for (const gn::StatusPlayer& pl : s.players) {
    if ((pl.flags & (gn::kPlayerBot | gn::kPlayerSpectator | gn::kPlayerViewer)) == 0) n++;
  }
  return n;
}

void poller_init(Poller& p, const PollerConfig& config, int64_t now_us) {
  p.config = config;
  p.query = {};
  p.querying = false;
  p.next_list_us = now_us;
  p.listed_us = -1;
  p.nodes.clear();
  p.stats = {};
  p.serial = 0;
  p.rng = config.seed * 0x2545f4914f6cdd1dull + 11;
}

void poller_update(Poller& p, gn::Transport& t, int64_t now_us) {
  if (!p.querying && now_us >= p.next_list_us && !p.config.masters.empty()) {
    gn::MasterQueryConfig qc;
    qc.masters = p.config.masters;
    qc.protocol = p.config.protocol;
    qc.kinds = p.config.kinds;
    qc.seed = draw64(p.rng);
    gn::master_query_start(p.query, qc, now_us);
    p.querying = true;
  }
  if (p.querying) {
    gn::master_query_update(p.query, t, now_us);
    if (p.query.done) take_list(p, now_us);
  }

  // Drop what no master lists and has been silent too long.
  const size_t before = p.nodes.size();
  p.nodes.erase(std::remove_if(p.nodes.begin(), p.nodes.end(),
                               [&](const HubNode& n) {
                                 if (n.browsed) return false;  // browse.hpp keeps its own
                                 const int64_t last = n.heard_us >= 0 ? n.heard_us : n.first_us;
                                 return !n.listed && now_us - last > p.config.drop_us;
                               }),
                p.nodes.end());
  if (p.nodes.size() != before) {
    p.stats.dropped += before - p.nodes.size();
    p.serial++;
  }

  for (HubNode& n : p.nodes) {
    if (n.browsed) continue;
    if (n.asked_us >= 0) {
      if (now_us - n.asked_us < p.config.retry_us) continue;
      if (n.tries >= p.config.tries) {  // no answer: its next turn
        n.asked_us = -1;
        n.tries = 0;
        n.cookie = 0;
        n.due_us = now_us + period(p, n);
        continue;
      }
      n.tries++;
      send_ask(p, t, n, now_us);
    } else if (now_us >= n.due_us) {
      n.tries = 1;
      send_ask(p, t, n, now_us);
    }
  }
}

bool poller_read(Poller& p, gn::Transport& t, const uint8_t* data, size_t size,
                 const gn::Address& from, int64_t now_us) {
  if (p.querying && gn::master_query_read(p.query, data, size, from, now_us)) {
    if (p.query.done) take_list(p, now_us);
    return true;
  }
  HubNode* n = find(p, from);
  if (!n || n->asked_us < 0) {
    p.stats.stray++;
    return false;
  }
  uint64_t cookie = n->cookie;
  gn::Status s;
  switch (gn::status_read_reply(data, size, n->nonce, cookie, s)) {
    case gn::StatusReply::none:
      p.stats.stray++;
      return false;
    case gn::StatusReply::cookie:
      p.stats.cookies++;
      n->cookie = cookie;
      send_ask(p, t, *n, now_us);  // the record's ask, at once
      return true;
    case gn::StatusReply::status:
      break;
  }
  p.stats.records++;
  const uint64_t h = record_hash(s);
  if (!n->have || h != n->record_hash) {
    n->changes++;
    p.stats.changes++;
    p.serial++;
  }
  n->record_hash = h;
  if (!n->have || s.phase != n->status.phase) n->phase_us = now_us;
  n->status = std::move(s);
  n->have = true;
  n->heard_us = now_us;
  n->asked_us = -1;
  n->tries = 0;
  n->nonce = draw64(p.rng);
  n->due_us = now_us + period(p, *n);
  return true;
}

void poller_pump(Poller& p, gn::Transport& t, int64_t now_us) {
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  int n = 0;
  while ((n = gn::transport_receive(t, buf, sizeof buf, from, now_us)) >= 0) {
    poller_read(p, t, buf, static_cast<size_t>(n), from, now_us);
  }
  poller_update(p, t, now_us);
}

const HubNode* poller_find(const Poller& p, const gn::Address& address) {
  for (const HubNode& n : p.nodes) {
    if (n.address == address) return &n;
  }
  return nullptr;
}

int poller_count(const Poller& p, uint8_t kinds) {
  int c = 0;
  for (const HubNode& n : p.nodes) c += n.have && (gn::kind_bit(n.kind) & kinds) ? 1 : 0;
  return c;
}

const HubNode* poller_page_forward(const Poller& p, const gn::Address& server) {
  const HubNode* best = nullptr;
  for (const HubNode& n : p.nodes) {
    if (!n.have || n.kind != gn::NodeKind::forward) continue;
    bool port = false;
    for (const auto& [k, v] : n.status.keys)
      port = port || (k == ge::forward::kWebTransportKey && !v.empty());
    if (!port || ge::forward::status_cert(n.status).empty() ||
        !ge::forward::status_reaches(n.status, server))
      continue;
    if (best == nullptr || n.status.players.size() < best->status.players.size()) best = &n;
  }
  return best;
}

}  // namespace ge::hub
