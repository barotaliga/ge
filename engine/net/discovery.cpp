// LAN discovery: the probe, the advert and the hosts found (net/discovery.hpp).
//
// ge:summary LAN discovery's datagrams: probes answered with adverts, adverts collected per host
// ge:keywords net lan discovery broadcast probe advert find-game
// ge:skill net
// ge:test tests/net_discovery_test.cpp
#include "net/discovery.hpp"

#include <cstring>

#include "net/session_wire.hpp"
#include "net/wire.hpp"

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

int lan_answer(Transport& t, const Advert& advert, int64_t now_us) {
  int answered = 0;
  uint8_t in[kMaxDatagram];
  Address from;
  for (;;) {
    const int n = transport_receive(t, in, sizeof in, from, now_us);
    if (n <= 0) break;
    Reader r = make_reader(in, static_cast<size_t>(n));
    uint8_t kind = 0;
    uint32_t protocol = 0;
    uint64_t nonce = 0;
    if (!read_head(r, kind, protocol, nonce) || kind != kProbe || protocol != advert.protocol)
      continue;
    uint8_t out[kProbeSize];
    Writer w = make_writer(out, sizeof out);
    write_bytes(w, kMagic, 4);
    write_u8(w, kAdvert);
    write_u32(w, advert.protocol);
    write_u64(w, nonce);
    write_u16(w, advert.port);
    write_u8(w, static_cast<uint8_t>(advert.seated));
    write_u8(w, static_cast<uint8_t>(advert.max_players));
    write_u8(w, static_cast<uint8_t>(advert.phase));
    const size_t name = std::strlen(advert.name);
    write_u8(w, static_cast<uint8_t>(name));
    write_bytes(w, advert.name, name);
    // Never more than the probe carried (no amplification).
    if (w.overflow || w.size > static_cast<size_t>(n)) continue;
    if (transport_send(t, from, out, w.size, now_us)) answered++;
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

int lan_collect(Transport& t, uint32_t protocol, uint64_t nonce, std::vector<Found>& found,
                int64_t now_us) {
  int read = 0;
  uint8_t in[kMaxDatagram];
  Address from;
  for (;;) {
    const int n = transport_receive(t, in, sizeof in, from, now_us);
    if (n <= 0) break;
    Reader r = make_reader(in, static_cast<size_t>(n));
    uint8_t kind = 0;
    uint32_t got_protocol = 0;
    uint64_t got_nonce = 0;
    if (!read_head(r, kind, got_protocol, got_nonce) || kind != kAdvert ||
        got_protocol != protocol || got_nonce != nonce)
      continue;
    Found f;
    f.advert.protocol = got_protocol;
    f.advert.port = read_u16(r);
    f.advert.seated = read_u8(r);
    f.advert.max_players = read_u8(r);
    f.advert.phase = read_u8(r) ? SessionPhase::playing : SessionPhase::lobby;
    const size_t name = read_u8(r);
    if (r.bad || name > kMaxAdvertName) continue;
    read_bytes(r, f.advert.name, name);
    if (r.bad) continue;
    f.advert.name[name] = 0;
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
