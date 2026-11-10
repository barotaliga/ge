// The server list (net/server_list.hpp): the broadcast, the masters, the queries, the view.
//
// ge:summary The server list's refresh, queries, replies and view
// ge:keywords net server-list status-query master-server ping pacing view sort filter
// ge:skill net
// ge:test tests/net_server_list_test.cpp
#include "net/server_list.hpp"

#include <algorithm>

namespace ge::net {

namespace {

// Big enough for any UDP datagram: a master's list may come in one large one.
constexpr size_t kBuffer = 65536;

int find_entry(const ServerList& l, const Address& a) {
  for (size_t i = 0; i < l.entries.size(); i++)
    if (l.entries[i].address == a) return static_cast<int>(i);
  return -1;
}

int find_master(const ServerList& l, const Address& a) {
  for (size_t i = 0; i < l.config.masters.size(); i++)
    if (l.config.masters[i] == a) return static_cast<int>(i);
  return -1;
}

void send_status(ServerList& l, const Address& to, int64_t now_us) {
  uint8_t out[256];
  const size_t n = l.query.status(out, sizeof out);
  transport_send(*l.transport, to, out, n, now_us);
  l.stats.queries++;
}

void ask_masters(ServerList& l, int64_t now_us) {
  if (l.query.master == nullptr) return;
  uint8_t out[256];
  const size_t n = l.query.master(out, sizeof out);
  for (size_t i = 0; i < l.masters.size(); i++) {
    ServerList::Master& m = l.masters[i];
    if (m.answered || m.tries >= l.config.master_tries) continue;
    if (m.asked_us >= 0 && now_us - m.asked_us < l.config.master_timeout_us) continue;
    transport_send(*l.transport, l.config.masters[i], out, n, now_us);
    m.tries++;
    m.asked_us = now_us;
    l.stats.master_asks++;
  }
}

// The queries due, one every `gap_us` round the list.
void send_queries(ServerList& l, int64_t now_us) {
  // A stall (a slow frame) does not save up a burst: at most a few gaps' worth at once.
  l.next_send_us = std::max(l.next_send_us, now_us - 4 * l.config.gap_us);
  for (ServerEntry& e : l.entries) {
    if (l.next_send_us > now_us) return;
    if (e.answered || e.tries >= l.config.tries) continue;
    if (e.asked_us >= 0 && now_us - e.asked_us < l.config.timeout_us) continue;
    send_status(l, e.address, now_us);
    e.tries++;
    e.asked_us = now_us;
    l.next_send_us += l.config.gap_us;
  }
}

void read_master(ServerList& l, int m, const uint8_t* data, size_t size, int64_t now_us) {
  std::vector<Address> got;
  if (l.query.read_master == nullptr || !l.query.read_master(data, size, got)) {
    l.stats.foreign++;
    return;
  }
  l.masters[static_cast<size_t>(m)].answered = true;
  l.stats.master_replies++;
  l.stats.master_addresses += got.size();
  for (const Address& a : got) server_list_add(l, a, kServerFromMaster, now_us);
}

void read_reply(ServerList& l, const Address& from, const uint8_t* data, size_t size,
                int64_t now_us) {
  const int m = find_master(l, from);
  if (m >= 0) {
    read_master(l, m, data, size, now_us);
    return;
  }
  ServerInfo info;
  if (!l.query.read_status(data, size, info)) {
    l.stats.foreign++;
    return;
  }
  int i = find_entry(l, from);
  if (i < 0 && l.lan_sent_us >= 0 && now_us - l.lan_sent_us <= l.config.lan_wait_us)
    i = server_list_add(l, from, kServerFromLan, now_us);  // a LAN server answering the broadcast
  if (i < 0) {
    l.stats.foreign++;  // a reply from a server this refresh never asked (or past `most`)
    return;
  }
  ServerEntry& e = l.entries[static_cast<size_t>(i)];
  const int64_t sent = e.asked_us;
  const bool first = !e.answered;
  e.answered = true;
  e.info = std::move(info);
  if (first && sent >= 0) e.ping_ms = static_cast<int>((now_us - sent + 999) / 1000);
  l.stats.replies++;
}

char fold(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool has(const std::string& text, const std::string& part) {
  if (part.empty()) return true;
  if (part.size() > text.size()) return false;
  for (size_t at = 0; at + part.size() <= text.size(); at++) {
    size_t k = 0;
    while (k < part.size() && fold(text[at + k]) == fold(part[k])) k++;
    if (k == part.size()) return true;
  }
  return false;
}

// -1, 0, 1 as `a` sorts before, with, after `b`, ASCII case folded.
int compare_text(const std::string& a, const std::string& b) {
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; i++) {
    const char x = fold(a[i]), y = fold(b[i]);
    if (x != y) return x < y ? -1 : 1;
  }
  return a.size() == b.size() ? 0 : a.size() < b.size() ? -1 : 1;
}

int compare_address(const Address& a, const Address& b) {
  if (a.family != b.family) return a.family < b.family ? -1 : 1;
  for (size_t i = 0; i < sizeof a.bytes; i++)
    if (a.bytes[i] != b.bytes[i]) return a.bytes[i] < b.bytes[i] ? -1 : 1;
  return a.port == b.port ? 0 : a.port < b.port ? -1 : 1;
}

}  // namespace

void server_list_init(ServerList& l, const ServerListConfig& config, const ServerQuery& query,
                      Transport& transport) {
  l = ServerList{};
  l.config = config;
  l.query = query;
  l.transport = &transport;
  l.buffer.resize(kBuffer);
  l.entries.reserve(std::min<size_t>(config.most, 256));
}

void server_list_refresh(ServerList& l, int64_t now_us) {
  l.entries.clear();
  l.refreshed_us = now_us;
  l.next_send_us = now_us;
  l.masters.assign(l.config.masters.size(), ServerList::Master{});
  l.lan_sent_us = -1;
  if (!l.config.lan.empty()) {
    for (const Address& a : l.config.lan) send_status(l, a, now_us);
    l.lan_sent_us = now_us;
  }
  ask_masters(l, now_us);
  for (const Address& a : l.config.servers) server_list_add(l, a, kServerFromNamed, now_us);
  send_queries(l, now_us);
}

int server_list_add(ServerList& l, const Address& address, uint8_t from, int64_t) {
  const int i = find_entry(l, address);
  if (i >= 0) {
    l.entries[static_cast<size_t>(i)].from |= from;
    return i;
  }
  if (l.entries.size() >= l.config.most) {
    l.stats.dropped++;
    return -1;
  }
  ServerEntry e;
  e.address = address;
  e.from = from;
  if (from == kServerFromLan) {  // the broadcast was its query
    e.tries = 1;
    e.asked_us = l.lan_sent_us;
  }
  l.entries.push_back(std::move(e));
  return static_cast<int>(l.entries.size() - 1);
}

void server_list_update(ServerList& l, int64_t now_us) {
  if (l.transport == nullptr || l.refreshed_us < 0) return;
  Address from;
  for (;;) {
    const int n = transport_receive(*l.transport, l.buffer.data(), l.buffer.size(), from, now_us);
    if (n < 0) break;
    read_reply(l, from, l.buffer.data(), static_cast<size_t>(n), now_us);
  }
  ask_masters(l, now_us);
  send_queries(l, now_us);
}

bool server_list_done(const ServerList& l, int64_t now_us) {
  if (l.refreshed_us < 0) return true;
  if (l.lan_sent_us >= 0 && now_us - l.lan_sent_us < l.config.lan_wait_us) return false;
  if (l.query.master != nullptr) {
    for (const ServerList::Master& m : l.masters) {
      if (m.answered) continue;
      if (m.tries < l.config.master_tries) return false;
      if (now_us - m.asked_us < l.config.master_timeout_us) return false;
    }
  }
  for (const ServerEntry& e : l.entries) {
    if (e.answered) continue;
    if (e.tries < l.config.tries) return false;
    if (now_us - e.asked_us < l.config.timeout_us) return false;
  }
  return true;
}

int server_list_answered(const ServerList& l) {
  int n = 0;
  for (const ServerEntry& e : l.entries) n += e.answered ? 1 : 0;
  return n;
}

const char* server_sort_name(ServerSort s) {
  switch (s) {
    case ServerSort::ping:
      return "ping";
    case ServerSort::players:
      return "players";
    case ServerSort::name:
      return "name";
    case ServerSort::map:
      return "map";
    case ServerSort::mode:
      return "mode";
  }
  return "?";
}

void server_list_view(const ServerList& l, const ServerFilter& filter, ServerSort sort,
                      std::vector<int>& rows) {
  rows.clear();
  for (size_t i = 0; i < l.entries.size(); i++) {
    const ServerEntry& e = l.entries[i];
    const ServerInfo& s = e.info;
    if (!(e.from & filter.from)) continue;
    if (filter.hide_silent && !e.answered) continue;
    if (filter.hide_full && s.max_players > 0 && s.players >= s.max_players) continue;
    if (filter.hide_empty && s.players == 0) continue;
    if (filter.max_ping_ms > 0 && (e.ping_ms < 0 || e.ping_ms > filter.max_ping_ms)) continue;
    if (!filter.mode.empty() && compare_text(s.mode, filter.mode) != 0) continue;
    if (!filter.name_has.empty() && !has(s.name, filter.name_has) && !has(s.map, filter.name_has) &&
        !has(s.mode, filter.name_has))
      continue;
    rows.push_back(static_cast<int>(i));
  }
  auto before = [&](int ia, int ib) {
    const ServerEntry& a = l.entries[static_cast<size_t>(ia)];
    const ServerEntry& b = l.entries[static_cast<size_t>(ib)];
    int c = 0;
    switch (sort) {
      case ServerSort::ping: {
        const int pa = a.ping_ms < 0 ? 1 << 30 : a.ping_ms;
        const int pb = b.ping_ms < 0 ? 1 << 30 : b.ping_ms;
        c = pa == pb ? 0 : pa < pb ? -1 : 1;
        break;
      }
      case ServerSort::players:
        c = a.info.players == b.info.players ? 0 : a.info.players > b.info.players ? -1 : 1;
        break;
      case ServerSort::name:
        break;
      case ServerSort::map:
        c = compare_text(a.info.map, b.info.map);
        break;
      case ServerSort::mode:
        c = compare_text(a.info.mode, b.info.mode);
        break;
    }
    if (c == 0) c = compare_text(a.info.name, b.info.name);
    if (c == 0) c = compare_address(a.address, b.address);
    return c < 0;
  };
  std::sort(rows.begin(), rows.end(), before);
}

}  // namespace ge::net
