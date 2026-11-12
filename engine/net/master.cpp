// The master, a node's beat and a reader's query (net/master.hpp).
//
// ge:summary The master's list: beats, byes, expiry, pages; the beat and the query clients
// ge:keywords net master beat ack bye list page query expire per-host
// ge:skill net
// ge:test tests/net_master_test.cpp
#include "net/master.hpp"

#include <cstdlib>
#include <cstring>

#include "net/reach_wire.hpp"
#include "net/udp_socket.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

constexpr uint8_t kMagic[4] = {'g', 'e', 'M', 'S'};
constexpr uint8_t kBeat = 1, kAck = 2, kBye = 3, kAsk = 4, kPage = 5;

uint64_t draw64(uint64_t& state) {
  uint64_t x = (state += 0x9e3779b97f4a7c15ull);
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  x ^= x >> 31;
  return x ? x : 1;
}

void write_start(Writer& w, uint8_t kind) {
  write_bytes(w, kMagic, 4);
  write_u8(w, kind);
}

bool read_start(Reader& r, uint8_t& kind) {
  const uint8_t* m = read_view(r, 4);
  if (!m || std::memcmp(m, kMagic, 4) != 0) return false;
  kind = read_u8(r);
  return !r.bad;
}

void send(Transport& t, const Address& to, const Writer& w, int64_t now_us) {
  if (!w.overflow) transport_send(t, to, w.data, w.size, now_us);
}

// The host part of an address (its port left out): what `max_per_host` counts by.
bool same_host(const Address& a, const Address& b) {
  return a.family == b.family && std::memcmp(a.bytes, b.bytes, 16) == 0;
}

MasterEntry* find(Master& m, const Address& at, uint32_t protocol, NodeKind kind) {
  for (MasterEntry& e : m.entries) {
    if (e.address == at && e.protocol == protocol && e.kind == kind) return &e;
  }
  return nullptr;
}

bool matches(const MasterEntry& e, uint32_t protocol, uint8_t kinds) {
  return e.protocol == protocol && (kinds & kind_bit(e.kind));
}

void on_datagram(Master& m, const Address& from, const uint8_t* data, size_t size, int64_t now_us) {
  Transport& t = *m.transport;
  Reader r = make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind)) {
    m.stats.refused++;
    return;
  }
  uint8_t out[kMasterAskSize];
  Writer w = make_writer(out, sizeof out);
  if (kind == kBeat || kind == kBye) {
    const uint32_t protocol = read_u32(r);
    const uint8_t node_kind = read_u8(r);
    const uint32_t version = read_u32(r);
    const uint16_t port = read_u16(r);
    const uint64_t nonce = read_u64(r);
    if (r.bad || size < kMasterBeatSize || node_kind > static_cast<uint8_t>(NodeKind::forward)) {
      m.stats.refused++;
      return;
    }
    Address at = from;
    if (port) at.port = port;
    const auto nk = static_cast<NodeKind>(node_kind);
    MasterEntry* e = find(m, at, protocol, nk);
    if (kind == kBye) {
      if (e) {
        m.entries.erase(m.entries.begin() + (e - m.entries.data()));
        m.stats.byes++;
      }
    } else {
      m.stats.beats++;
      if (!e) {
        int host = 0;
        for (const MasterEntry& o : m.entries) host += same_host(o.address, at) ? 1 : 0;
        if (static_cast<int>(m.entries.size()) >= m.config.max_entries ||
            host >= m.config.max_per_host) {
          m.stats.full++;
          return;
        }
        m.entries.push_back({at, protocol, nk, version, now_us, now_us});
        e = &m.entries.back();
        m.stats.added++;
      }
      e->version = version;
      e->last_us = now_us;
    }
    write_start(w, kAck);
    write_u64(w, nonce);
    send(t, from, w, now_us);
    return;
  }
  if (kind == kAsk) {
    const uint32_t protocol = read_u32(r);
    const uint8_t kinds = read_u8(r);
    const uint16_t page = read_u16(r);
    const uint64_t nonce = read_u64(r);
    if (r.bad || size < kMasterAskSize) {  // an ask is padded to the biggest page
      m.stats.refused++;
      return;
    }
    m.stats.asks++;
    const int total = master_count(m, protocol, kinds);
    write_start(w, kPage);
    write_u64(w, nonce);
    write_u32(w, static_cast<uint32_t>(total));
    write_u16(w, page);
    const size_t count_at = w.size;
    write_u8(w, 0);
    const int first = page * kMasterPage;
    int index = 0, count = 0;
    for (const MasterEntry& e : m.entries) {
      if (!matches(e, protocol, kinds)) continue;
      if (index++ < first) continue;
      if (count == kMasterPage) break;
      reach_wire::write_address(w, e.address);
      write_u8(w, static_cast<uint8_t>(e.kind));
      write_u32(w, e.version);
      count++;
    }
    out[count_at] = static_cast<uint8_t>(count);
    send(t, from, w, now_us);
    m.stats.pages++;
    return;
  }
  m.stats.refused++;
}

void write_beat(Writer& w, uint8_t kind, const MasterBeatConfig& c, uint64_t nonce) {
  write_start(w, kind);
  write_u32(w, c.protocol);
  write_u8(w, static_cast<uint8_t>(c.kind));
  write_u32(w, c.version);
  write_u16(w, c.port);
  write_u64(w, nonce);
  write_pad_to(w, kMasterBeatSize);
}

}  // namespace

void master_init(Master& m, const MasterConfig& config, Transport& transport) {
  m.config = config;
  m.transport = &transport;
  m.entries.clear();
  m.entries.reserve(static_cast<size_t>(config.max_entries));
  m.stats = {};
}

void master_update(Master& m, int64_t now_us) {
  Address from;
  for (;;) {
    const int n = transport_receive(*m.transport, m.buffer, sizeof m.buffer, from, now_us);
    if (n < 0) break;
    on_datagram(m, from, m.buffer, static_cast<size_t>(n), now_us);
  }
  size_t kept = 0;
  for (size_t i = 0; i < m.entries.size(); i++) {
    if (now_us - m.entries[i].last_us > m.config.expire_us) {
      m.stats.expired++;
      continue;
    }
    m.entries[kept++] = m.entries[i];
  }
  m.entries.resize(kept);
}

int master_count(const Master& m, uint32_t protocol, uint8_t kinds) {
  int n = 0;
  for (const MasterEntry& e : m.entries) n += matches(e, protocol, kinds) ? 1 : 0;
  return n;
}

void master_beat_init(MasterBeat& b, const MasterBeatConfig& config) {
  b.config = config;
  b.masters.assign(config.masters.size(), {});
  uint64_t seed = config.seed * 0x2545f4914f6cdd1dull + 7;
  for (MasterBeatState& s : b.masters) s.nonce = draw64(seed);
  b.beats = b.acks = 0;
  b.stopped = false;
}

void master_beat_update(MasterBeat& b, Transport& t, int64_t now_us) {
  if (b.stopped) return;
  for (size_t i = 0; i < b.masters.size(); i++) {
    MasterBeatState& s = b.masters[i];
    if (now_us < s.next_us) continue;
    uint8_t out[kMasterBeatSize];
    Writer w = make_writer(out, sizeof out);
    write_beat(w, kBeat, b.config, s.nonce);
    send(t, b.config.masters[i], w, now_us);
    b.beats++;
    // Unanswered, again soon; answered, master_beat_read moves it to the next beat.
    s.next_us = now_us + b.config.ask_us;
  }
}

bool master_beat_read(MasterBeat& b, const uint8_t* data, size_t size, const Address& from,
                      int64_t now_us) {
  Reader r = make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind) || kind != kAck) return false;
  const uint64_t nonce = read_u64(r);
  if (r.bad) return true;
  for (size_t i = 0; i < b.masters.size(); i++) {
    MasterBeatState& s = b.masters[i];
    if (b.config.masters[i] != from || s.nonce != nonce) continue;
    s.acked_us = now_us;
    s.next_us = now_us + b.config.beat_us;
    uint64_t seed = s.nonce;
    s.nonce = draw64(seed);  // the next beat's: a late ack of this one is not taken for it
    b.acks++;
  }
  return true;
}

void master_beat_stop(MasterBeat& b, Transport& t, int64_t now_us) {
  if (b.stopped) return;
  b.stopped = true;
  for (size_t i = 0; i < b.masters.size(); i++) {
    uint8_t out[kMasterBeatSize];
    Writer w = make_writer(out, sizeof out);
    write_beat(w, kBye, b.config, b.masters[i].nonce);
    for (int k = 0; k < b.config.bye_tries; k++) send(t, b.config.masters[i], w, now_us);
  }
}

int master_beat_listed(const MasterBeat& b, int64_t now_us, int64_t within_us) {
  int n = 0;
  for (const MasterBeatState& s : b.masters) {
    n += s.acked_us >= 0 && now_us - s.acked_us <= within_us ? 1 : 0;
  }
  return n;
}

void master_query_start(MasterQuery& q, const MasterQueryConfig& config, int64_t now_us) {
  (void)now_us;
  q.config = config;
  q.masters.assign(config.masters.size(), {});
  uint64_t seed = config.seed * 0x9e3779b97f4a7c15ull + 3;
  for (MasterQueryState& s : q.masters) {
    s.nonce = draw64(seed);
    // Page 0 due at once (INT64_MIN: never asked); the rest once its answer gives the total.
    s.asked_us.assign(1, INT64_MIN);
    s.tries.assign(1, 0);
  }
  q.found.clear();
  q.done = q.masters.empty();
}

void master_query_update(MasterQuery& q, Transport& t, int64_t now_us) {
  bool all = true;
  for (size_t i = 0; i < q.masters.size(); i++) {
    MasterQueryState& s = q.masters[i];
    if (s.done) continue;
    bool left = false;
    for (size_t page = 0; page < s.asked_us.size(); page++) {
      if (s.asked_us[page] == -1) continue;  // answered
      left = true;
      if (s.tries[page] >= q.config.tries) {
        // Asked as often as it may be: waited for once more, then given up on.
        if (now_us - s.asked_us[page] >= q.config.ask_us) left = false;
        continue;
      }
      if (s.asked_us[page] != INT64_MIN && now_us - s.asked_us[page] < q.config.ask_us) continue;
      uint8_t out[kMasterAskSize];
      Writer w = make_writer(out, sizeof out);
      write_start(w, kAsk);
      write_u32(w, q.config.protocol);
      write_u8(w, q.config.kinds);
      write_u16(w, static_cast<uint16_t>(page));
      write_u64(w, s.nonce);
      write_pad_to(w, kMasterAskSize);
      send(t, q.config.masters[i], w, now_us);
      s.asked_us[page] = now_us;
      s.tries[page]++;
    }
    // Every page answered, or asked as often as it may be and waited for: this master is done.
    if (!left) s.done = true;
    if (!s.done) all = false;
  }
  q.done = all;
}

bool master_query_read(MasterQuery& q, const uint8_t* data, size_t size, const Address& from,
                       int64_t now_us) {
  Reader r = make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind) || kind != kPage) return false;
  const uint64_t nonce = read_u64(r);
  const uint32_t total = read_u32(r);
  const uint16_t page = read_u16(r);
  const uint8_t count = read_u8(r);
  if (r.bad) return true;
  for (size_t i = 0; i < q.masters.size(); i++) {
    MasterQueryState& s = q.masters[i];
    if (q.config.masters[i] != from || s.nonce != nonce) continue;
    if (s.total < 0) {
      s.total = static_cast<int>(total);
      const int pages = s.total == 0 ? 1 : (s.total + kMasterPage - 1) / kMasterPage;
      s.asked_us.resize(static_cast<size_t>(pages), INT64_MIN);
      s.tries.resize(static_cast<size_t>(pages), 0);
    }
    if (page >= s.asked_us.size() || s.asked_us[page] == -1) return true;
    s.asked_us[page] = -1;
    for (int k = 0; k < count; k++) {
      MasterEntry e;
      e.address = reach_wire::read_address(r);
      const uint8_t nk = read_u8(r);
      e.version = read_u32(r);
      if (r.bad || nk > static_cast<uint8_t>(NodeKind::forward)) return true;
      e.kind = static_cast<NodeKind>(nk);
      e.protocol = q.config.protocol;
      e.first_us = e.last_us = now_us;
      bool seen = false;
      for (const MasterEntry& f : q.found) {
        if (f.address == e.address && f.kind == e.kind) {
          seen = true;
          break;
        }
      }
      if (!seen) q.found.push_back(e);
    }
  }
  return true;
}

bool master_list(std::string_view text, std::vector<Address>& out, std::string& error) {
  out.clear();
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find_first_of(" ,", at);
    if (end == std::string_view::npos) end = text.size();
    const std::string word(text.substr(at, end - at));
    at = end + 1;
    if (word.empty()) continue;
    Address a;
    if (parse_address(word, a)) {
      out.push_back(a);
      continue;
    }
    const size_t colon = word.rfind(':');
    const std::string host = colon == std::string::npos ? word : word.substr(0, colon);
    const int port = colon == std::string::npos ? kMasterPort : std::atoi(word.c_str() + colon + 1);
    std::string why;
    if (port <= 0 || port > 65535 || !resolve_ipv4(host, static_cast<uint16_t>(port), a, &why)) {
      error = "'" + word + "' is not a master's address (" + (why.empty() ? "bad port" : why) +
              "); next: write host:port, or leave the list empty to use no master";
      return false;
    }
    out.push_back(a);
  }
  return true;
}

}  // namespace ge::net
