// LAN discovery: the probe, the advert and the hosts found (net/discovery.hpp).
//
// ge:summary LAN discovery's datagrams: probes answered with adverts, adverts collected per host
// ge:keywords net lan discovery broadcast probe advert find-game ping
// ge:skill net
// ge:test tests/net_discovery_test.cpp
#include "net/discovery.hpp"

#include <cstring>

#include "net/session_wire.hpp"

namespace ge::net {

namespace {

constexpr uint8_t kMagic[4] = {'g', 'e', 'L', 'N'};
constexpr uint8_t kProbe = 1, kAdvert = 2;

bool read_head(Reader& r, uint8_t& kind, uint32_t& protocol, uint64_t& nonce) {
  uint8_t magic[4];
  read_bytes(r, magic, 4);
  kind = read_u8(r);
  protocol = read_u32(r);
  nonce = read_u64(r);
  return !r.bad && std::memcmp(magic, kMagic, 4) == 0;
}

}  // namespace

void write_advert_body(Writer& w, const Advert& a) {
  write_u16(w, a.port);
  write_u8(w, static_cast<uint8_t>(a.seated));
  write_u8(w, static_cast<uint8_t>(a.max_players));
  write_u8(w, static_cast<uint8_t>(a.phase));
  write_u64(w, a.session);
  write_u8(w, a.flags);
  size_t name = std::strlen(a.name);
  if (name > kMaxAdvertName) name = kMaxAdvertName;
  write_u8(w, static_cast<uint8_t>(name));
  write_bytes(w, a.name, name);
}

bool read_advert_body(Reader& r, Advert& a) {
  a.port = read_u16(r);
  a.seated = read_u8(r);
  a.max_players = read_u8(r);
  a.phase = read_u8(r) ? SessionPhase::playing : SessionPhase::lobby;
  a.session = read_u64(r);
  a.flags = read_u8(r);
  const size_t name = read_u8(r);
  if (r.bad || name > kMaxAdvertName) return false;
  read_bytes(r, a.name, name);
  if (r.bad) return false;
  a.name[name] = 0;
  return true;
}

size_t lan_answer_probe(const uint8_t* data, size_t size, const Advert& advert, uint8_t* out) {
  Reader r = make_reader(data, size);
  uint8_t kind = 0;
  uint32_t protocol = 0;
  uint64_t nonce = 0;
  if (!read_head(r, kind, protocol, nonce) || kind != kProbe || protocol != advert.protocol)
    return 0;
  Writer w = make_writer(out, kProbeSize);
  write_bytes(w, kMagic, 4);
  write_u8(w, kAdvert);
  write_u32(w, advert.protocol);
  write_u64(w, nonce);
  write_advert_body(w, advert);
  // Never more than the probe carried (no amplification).
  if (w.overflow || w.size > size) return 0;
  return w.size;
}

int lan_answer(Transport& t, const Advert& advert, int64_t now_us) {
  int answered = 0;
  uint8_t in[kMaxDatagram];
  Address from;
  for (;;) {
    const int n = transport_receive(t, in, sizeof in, from, now_us);
    if (n <= 0) break;
    uint8_t out[kProbeSize];
    const size_t size = lan_answer_probe(in, static_cast<size_t>(n), advert, out);
    if (size && transport_send(t, from, out, size, now_us)) answered++;
  }
  return answered;
}

Advert advert_of(const SessionHost& host, uint32_t protocol, uint16_t port, const char* name) {
  Advert a;
  a.protocol = protocol;
  a.port = port;
  a.seated = session_player_count(host, false);
  a.max_players = host.config.max_players;
  a.phase = host.phase;
  const size_t n = session_wire::clean_text(reinterpret_cast<const uint8_t*>(name),
                                            std::strlen(name), kMaxAdvertName, a.name);
  a.name[n] = 0;
  return a;
}

bool lan_probe(Transport& t, const Address& to, uint32_t protocol, uint64_t nonce, int64_t now_us) {
  uint8_t out[kProbeSize];
  Writer w = make_writer(out, sizeof out);
  write_bytes(w, kMagic, 4);
  write_u8(w, kProbe);
  write_u32(w, protocol);
  write_u64(w, nonce);
  write_pad_to(w, kProbeSize);
  return transport_send(t, to, out, w.size, now_us);
}

bool lan_read_advert(const uint8_t* data, size_t size, uint32_t protocol, uint64_t& nonce,
                     Advert& advert) {
  Reader r = make_reader(data, size);
  uint8_t kind = 0;
  uint32_t got_protocol = 0;
  if (!read_head(r, kind, got_protocol, nonce) || kind != kAdvert || got_protocol != protocol)
    return false;
  advert = Advert{};
  advert.protocol = got_protocol;
  return read_advert_body(r, advert);
}

int lan_collect(Transport& t, uint32_t protocol, uint64_t nonce, std::vector<Found>& found,
                int64_t now_us) {
  int read = 0;
  uint8_t in[kMaxDatagram];
  Address from;
  for (;;) {
    const int n = transport_receive(t, in, sizeof in, from, now_us);
    if (n <= 0) break;
    Found f;
    uint64_t got_nonce = 0;
    if (!lan_read_advert(in, static_cast<size_t>(n), protocol, got_nonce, f.advert) ||
        got_nonce != nonce)
      continue;
    f.session = from;
    f.session.port = f.advert.port;
    f.seen_us = now_us;
    bool known = false;
    for (Found& k : found) {
      if (k.session == f.session) {
        k = f;
        known = true;
      }
    }
    if (!known) found.push_back(f);
    read++;
  }
  return read;
}

}  // namespace ge::net
