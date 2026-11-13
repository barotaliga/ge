// A FIXED ROUTE (docs/tv.md § Forward, M57d): a forward port that sends every client to one
// destination, for a game whose clients cannot dial a route (an unmodified client knows only a
// server's address). The player connects to the forward as if it were the server; the forward
// gives the client its own socket upstream and copies the game's datagrams both ways untouched,
// one out for one in. The hop's own datagrams (a dialled route, alive, status, STUN) still work
// beside it, so one port serves dialled and fixed clients alike.
//
// NO AMPLIFICATION: a datagram's source address is proven only by a two-way exchange, which the
// forward cannot read in a game it does not know. Until a client has sent `proven_after`
// datagrams, the forward passes back at most `budget` bytes for each byte the client sent (the
// game's own handshake fits: a challenge for a challenge ask, a refusal or a welcome for a
// connect), and holds the rest back. A game's handshake proves the address to its server, so by
// the time the server sends a whole level the client has spoken enough to be passed everything.
//
//   ge::forward::forward_init(f, config, port, open);  fixed_route_attach(f, face);
//   every frame: ge::forward::forward_update(f, now)
//
// ge:summary A fixed route: a forward port that sends every client to one server, for stock clients
// ge:keywords forward fixed-route port-forward stock-client amplification budget fixed_route_attach
// ge:skill net
// ge:test tests/forward_test.cpp
#pragma once

#include <cstdint>

#include "forward/forward.hpp"
#include "net/address.hpp"

namespace ge::forward {

// The face's settings and counts; the caller keeps it as long as the forward.
struct FixedFace {
  net::Address to;       // the destination every client goes to (on the forward's allow list)
  int proven_after = 3;  // datagrams a client sends before everything is passed back to it
  uint32_t budget = 4;   // until then: bytes passed back for each byte the client sent
  uint64_t clients = 0, proven = 0, held_back = 0, refused = 0;
};

// The forward's face (`ForwardFace`) set to send every unknown client to `face.to`, replacing a
// face already attached.
void fixed_route_attach(Forward& f, FixedFace& face);

}  // namespace ge::forward
