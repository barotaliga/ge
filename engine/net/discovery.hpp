// FINDING A GAME ON THE LAN: a client broadcasts a PROBE to the game's discovery port and every
// host listening there answers with an ADVERT: where its session is, its name, how many are
// seated of how many, and whether it is in the lobby. The discovery port is its own socket (a
// game picks one fixed port; the session's node may be on any), so a probe never reaches a node
// and a node's port never needs to be known in advance. The same probe sent straight to a host's
// session address is a PING (a hosting node answers it on its own socket, hosting.hpp): the
// server browser times the advert that comes back (directory.hpp).
//
// The datagrams, outside the node's wire format (packet.hpp) and recognised by their magic:
//   probe   "geLN" 1, protocol u32, nonce u64, padding to kProbeSize
//   advert  "geLN" 2, protocol u32, nonce u64, then the advert's body:
//           the session's port u16, seated u8, max u8, phase u8, session id u64, flags u8,
//           name (u8 length, bytes)
// The body is also how the rendezvous lists a session (rendezvous.hpp), so a host found on the
// LAN and the same host listed on the internet carry the same session id and the browser shows
// it once.
// A host answers only a probe of its own protocol, with the probe's nonce, and never with more
// bytes than the probe carried, so discovery cannot be used to amplify traffic at a third
// party. A client keeps the adverts that carry its nonce, one a host (the newest wins).
//
// Over UDP the probe goes to 255.255.255.255 (or a subnet's broadcast) on the discovery port;
// over the simulated network to `sim_broadcast(port)` (sim_network.hpp).
//
// ge:summary LAN discovery: a broadcast probe, each host's advert, the hosts found, the ping
// ge:keywords net lan discovery broadcast probe advert find-game server-browser beacon local
// ge:keywords network host list ping session-id locked password
// ge:skill net
// ge:test tests/net_discovery_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/address.hpp"
#include "net/session.hpp"
#include "net/wire.hpp"

namespace ge::net {

constexpr size_t kProbeSize = 64;
constexpr size_t kAdvertHead = 17;  // magic, kind, protocol, nonce
constexpr size_t kAdvertBody = 15;  // port, seated, max, phase, session, flags, the name's length
constexpr size_t kMaxAdvertName = kProbeSize - kAdvertHead - kAdvertBody;  // never outgrows a probe

// Advert::flags.
constexpr uint8_t kAdvertLocked = 1;  // joining takes a password (the game checks it)

// What a host says about itself; `port` is the port its session's node listens on.
struct Advert {
  uint32_t protocol = 0;
  uint16_t port = 0;
  int seated = 0, max_players = 0;
  SessionPhase phase = SessionPhase::lobby;
  uint64_t session = 0;  // the host's id at the rendezvous (0: none, the LAN only)
  uint8_t flags = 0;
  char name[kMaxAdvertName + 1] = {};
};

// A host found: the advert, and the address its session is at (the advert's sender with the
// session's port).
struct Found {
  Address session;
  Advert advert;
  int64_t seen_us = 0;
};

// The advert's body, as the advert and the rendezvous's listing both carry it.
void write_advert_body(Writer& w, const Advert& a);
// False when it does not read (`a.protocol` is the caller's).
bool read_advert_body(Reader& r, Advert& a);

// HOST: the advert answering one datagram, into `out` (kProbeSize bytes); its size, or 0 when the
// datagram is not a probe of `advert.protocol` or is shorter than the answer.
size_t lan_answer_probe(const uint8_t* data, size_t size, const Advert& advert, uint8_t* out);
// HOST: answer every probe of `advert.protocol` waiting on `t` (the discovery socket); the
// number answered.
int lan_answer(Transport& t, const Advert& advert, int64_t now_us);
// An advert off a session host: its seats, its maximum, its phase.
Advert advert_of(const SessionHost& host, uint32_t protocol, uint16_t port, const char* name);

// CLIENT: send a probe to `to`: the broadcast address on the discovery port (the LAN), or a
// host's session address (a ping).
bool lan_probe(Transport& t, const Address& to, uint32_t protocol, uint64_t nonce, int64_t now_us);
// CLIENT: one datagram read as an advert of `protocol`: its nonce and the advert; false when it
// is anything else.
bool lan_read_advert(const uint8_t* data, size_t size, uint32_t protocol, uint64_t& nonce,
                     Advert& advert);
// Read the adverts waiting on `t` that answer `nonce`, into `found` (one a host); the number read.
int lan_collect(Transport& t, uint32_t protocol, uint64_t nonce, std::vector<Found>& found,
                int64_t now_us);

}  // namespace ge::net
