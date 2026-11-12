// THE CLASSIC FACE OF OUR FORWARD (docs/tv.md § Forward, M57d): the engine's forward
// (engine/forward/forward.hpp) taking classic clients and classic forwarders the way they set a
// route up, so a classic player chains through ours and ours chains through theirs.
//
// THE ROUTE is the `prx` key of the connect's user info: the rest of the route after this hop,
// "host[:port]@...@server[:port]" (a hop with no port is a server's, 27500). A client asks this
// hop for a challenge and connects with `prx` set; the hop takes the first entry off it as its
// destination (the next forwarder or the server), writes the rest back (or removes the key when
// none is left), adds `*qwfwd` (so the server knows a forwarder carried the client) and runs the
// handshake upstream from the client's own socket: a challenge asked every 2 s, then the connect
// with the client's qport. Unlike the classic forwarders it answers the client's `j` only when the
// destination has accepted, and passes the destination's refusal on in its words, so a chain
// connects end to end or says why not. Then the game's datagrams flow both ways untouched.
//
// WHAT IT ANSWERS ITSELF: `ping` (`l`), `status` (its info string, `*version` "qwfwd ge 1", the
// name, the clients), `pingstatus` (each server it knows and its ping, for a client's route
// finder: 8 bytes a server), `getchallenge`. `rcon` from a connected client goes up; any other
// out-of-band text is dropped. A client's `drop` goes up three times and frees it.
//
// BETTER THAN THE CLASSIC FORWARDERS: the challenge is made from the client's address with a
// secret (nothing kept before a valid connect); `status` and `pingstatus` are answered once a
// second a host at most; a client whose NAT gives it a new port is followed by its qport on the
// same host; the allow list and the limit are the forward's.
//
// THE SERVERS IT PINGS (for `pingstatus`): `ClassicFace::servers`, and every allowed destination
// with a port. One probe (`k`) every `probe_us`, round robin, each server at most once every
// `every_us`; a server's ping is the round trip of its `l` in milliseconds (65535 until one).
//
//   ge::forward::forward_init(f, config, port, open);  classic_forward_attach(f, face);
//   every frame: ge::forward::forward_update(f, now)
//
// ge:summary The classic face of our forward: classic clients and forwarders chain through ours
// ge:keywords forward classic-forward prx route chain getchallenge connect pingstatus status
// ge:keywords route-finder qport nat-rebinding classic_forward_attach
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_forward_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "forward/forward.hpp"
#include "net/address.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

inline constexpr uint16_t kForwardPort = 30000;  // the classic forwarders' port

struct FaceServer {
  gn::Address address;
  int ping_ms = 65535;  // 65535: no answer yet
  int64_t probed_us = -1, answered_us = -1;
};

struct FaceCounts {
  int64_t challenges = 0, connects = 0, accepted = 0, refused = 0, timed_out = 0, drops = 0;
  int64_t pings = 0, statuses = 0, pingstatuses = 0, limited = 0, rebinds = 0, probes = 0;
};

struct ClassicFace {
  std::string hostport, countrycode, city, coords;  // in the info string when set
  std::vector<gn::Address> servers;  // pinged for `pingstatus`, beside the allow list
  int64_t probe_us = 100000, every_us = 60000000;
  int64_t retry_us = 2000000;  // a challenge or a connect upstream asked again
  int tries = 5;               // asked this many times before the client is told no answer
  // Kept by the face:
  std::vector<FaceServer> known;
  size_t next_probe = 0;
  int64_t next_probe_us = 0;
  std::vector<std::pair<uint64_t, int64_t>> answered;  // a host's last status answer
  uint64_t secret = 0;
  FaceCounts counts;
};

// The face's hooks set on `f` (which keeps a pointer to `face`: neither may move after).
void classic_forward_attach(ge::forward::Forward& f, ClassicFace& face);
// The forward's classic info string ("\*version\qwfwd ge 1\hostname\...").
std::string classic_forward_info(const ge::forward::Forward& f, const ClassicFace& face);
// "host[:port]" with the classic server's port when none; false when it is no address.
bool classic_destination(const std::string& text, gn::Address& out);

}  // namespace shooters_edge::net
