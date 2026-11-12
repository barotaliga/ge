// FORWARD (docs/tv.md § Forward): an opaque hop between a game's client and its server, so a
// player can trade a bad internet path for a better one. Any game on engine/net (or any UDP game)
// goes through one: after the route is set up the forwarder copies datagrams both ways, byte for
// byte, one out for one in. Forwarders chain: the client names the route, and each hop takes the
// next one off it.
//
// THE ROUTE is set up before the game's own handshake, by the forward's datagrams (magic "geFW"):
//   route    client → hop: the client's salt, the cookie (0 the first time), the rest of the route
//            ("host:port@host:port", the game server last), padded to kForwardRouteSize
//   cookie   hop → client: the salt and a cookie only this hop makes from the client's address and
//            salt (so a spoofed address never gets a route, and the hop keeps nothing until then)
//   routed   hop → client: the salt and the answer (ok, or a refusal and why)
//   alive    both ways: the salt and the sender's clock; the hop echoes the client's, so the
//            client knows the hop is there and how far, and a client whose NAT gave it a new port
//            is followed by its salt
// A hop takes the first entry off the route as its destination. With more after it, the
// destination is the next forwarder and the hop dials it from its own socket for this client,
// as a client would (the `Dial` below), answering `routed` only when the whole chain is up; with
// none, the destination is the game server and the hop answers at once. Then the client starts
// its game's own handshake with the first hop's address as if it were the server.
//
// WHAT A HOP REFUSES: a destination not on its allow list (`ForwardConfig::allow`; empty and not
// `open`: no destination at all), a full hop, a route that does not parse. No answer is bigger
// than the datagram that asked for it: the route request is padded past every answer, a status
// ask is answered by net/status.hpp's cookie dance, and a STUN request only when it is as big as
// its answer. A client silent both ways for `idle_us` is dropped.
//
//   forward_init(f, config, port, open_upstream)   every frame: forward_update(f, now)
// A client's half:
//   dial_start(d, chain, seed, now)   every frame: dial_update(d, transport, now)
//   each datagram on that socket: dial_read(d, data, size, from, now)   (true: it was the hop's)
//   once d.state is routed: connect the game to d.first
//
// The clock is the caller's: a chain on the simulated network replays to the bit (the `forward`
// lab).
//
// ge:summary Forward: an opaque hop for any game's datagrams, chained by the route in its setup
// ge:keywords forward forwarder hop route chain proxy-chain better-path allow-list keepalive
// ge:keywords nat-rebinding cookie amplification stun status dial
// ge:skill net
// ge:test tests/forward_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "net/address.hpp"
#include "net/transport.hpp"

namespace ge::forward {

inline constexpr size_t kForwardRouteSize = 300;  // a route request, padded: no answer is bigger
inline constexpr size_t kMostRoute = 255;         // the route's text
inline constexpr int kMostHops = 8;

enum class FwMsg : uint8_t { route = 1, cookie = 2, routed = 3, alive = 4 };

enum class Answer : uint8_t { ok = 0, full = 1, not_allowed = 2, bad_route = 3, unreachable = 4 };
const char* answer_text(Answer a);

// "host:port@host:port" into addresses (each `net::parse_address`'s form); false with `error`.
bool parse_route(std::string_view text, std::vector<net::Address>& out, std::string& error);
std::string route_text(std::span<const net::Address> hops);
// The datagram is one of the forward's own.
bool is_forward(const uint8_t* data, size_t size);

// THE CLIENT'S HALF (and a hop's, toward the next hop).
enum class DialState : uint8_t { idle, asking, routed, refused, timed_out };

struct Dial {
  net::Address first;  // the first hop: where the client sends
  std::string rest;    // the route after it
  uint64_t salt = 0;
  uint64_t cookie = 0;
  DialState state = DialState::idle;
  bool direct = false;  // no hop: `first` is the game server itself
  Answer answer = Answer::ok;
  std::string why;  // a refusal's or a time-out's words
  int64_t started_us = 0, next_us = 0;
  int64_t retry_us = 500000, timeout_us = 10000000, alive_us = 1000000;
  int64_t rtt_us = -1;  // the last alive's round trip
  uint64_t sent = 0, heard = 0;
};

// Start a route through `chain`: the hops, then the game server last (one entry: no hop at all,
// the dial is routed at once).
void dial_start(Dial& d, std::span<const net::Address> chain, uint64_t seed, int64_t now_us);
// Send what is due: the route request again until answered, then an alive every `alive_us`.
void dial_update(Dial& d, net::Transport& t, int64_t now_us);
// A datagram from the socket: true when it was the first hop's answer to this dial (taken).
bool dial_read(Dial& d, const uint8_t* data, size_t size, const net::Address& from, int64_t now_us);

// THE HOP.
struct ForwardConfig {
  std::string name = "forward";
  std::vector<net::Address> allow;  // destinations it may reach (port 0: any port of that host)
  bool open = false;                // any destination (an operator's explicit choice)
  int max_clients = 128;
  int64_t idle_us = 15000000;  // nothing either way this long: the client is dropped
  uint32_t version = 1;        // in its status
  uint64_t seed = 0;           // the secret's and the salts' stream
};

// A socket of its own toward the next hop for one client; false when none can be opened.
using OpenUpstream = std::function<bool(net::Transport& out)>;

struct HopClient {
  net::Address from;  // the client, as its datagrams come now
  uint64_t salt = 0;
  net::Address to;   // the next hop or the game server
  std::string name;  // as `alive` and status show it: the client's address
  std::unique_ptr<net::Transport> up;
  Dial dial;               // toward the next forwarder (rest of the route not empty)
  bool chained = false;    // `to` is a forwarder
  bool connected = false;  // the route is up: datagrams flow
  int64_t since_us = 0, last_us = 0;
  uint64_t packets_up = 0, packets_down = 0, bytes_up = 0, bytes_down = 0;
};

struct ForwardStats {
  uint64_t cookies = 0, routed = 0, refused = 0, bad_cookies = 0, idle_drops = 0;
  uint64_t rebinds = 0, status_answers = 0, stun_answers = 0, unknown = 0, strays = 0;
  uint64_t bytes_in = 0, bytes_out = 0;  // the client socket's, every datagram
};

struct Forward {
  ForwardConfig config;
  net::Transport* port = nullptr;
  OpenUpstream open_upstream;
  uint64_t secret = 0;
  uint64_t draws = 0;
  std::vector<std::unique_ptr<HopClient>> clients;
  ForwardStats stats;
};

void forward_init(Forward& f, const ForwardConfig& config, net::Transport& port,
                  OpenUpstream open_upstream);
// Receive on the client socket and every upstream, copy the game's datagrams, answer the
// forward's own and status and STUN asks, dial onward, drop the idle.
void forward_update(Forward& f, int64_t now_us);
// Close every client's upstream socket.
void forward_close(Forward& f);
// The status protocol a forwarder answers on (net/status.hpp), every forwarder alike.
uint32_t forward_protocol();

}  // namespace ge::forward
