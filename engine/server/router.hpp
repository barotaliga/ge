// THE SERVER'S PORT OPENED ON THE HOME ROUTER: a dedicated server at home asks the router to
// forward its game port from the internet (`sv_open_port 1`), so players anywhere join it with no
// one setting up the router by hand. The same for every game on the engine: the asking is
// engine/net/portmap.hpp's (PCP, then NAT-PMP, then UPnP), from a socket of its own beside the
// game's.
//
// - THE ROUTER: `sv_gateway` names it (an address, port 5351 when left out); "" finds the
//   machine's default route where the system tells it (Linux), and UPnP's search finds a router
//   with none named.
// - THE FORWARD is asked for the game's UDP port, the same port outside, for an hour at a time and
//   renewed at half its life. A lease the router drops is asked for again.
// - WHAT IT SAYS (`said`, for the server's console and log), once each: the forward made with the
//   public address players reach the server at, or why none was ("no router answered"), or the
//   forward lost.
// - CLOSED with the server: the router is asked to take the forward down (`router_close`, then
//   `router_frame` until `router_done`, a second at most).
//
// The clock is the caller's; over the simulated network it replays to the bit (the router's test
// runs against engine/net/sim_router.hpp).
//
// ge:summary The server's game port forwarded by the home router (sv_open_port, sv_gateway)
// ge:keywords sv_open_port sv_gateway port-forward open-port router upnp nat-pmp pcp home-server
// ge:keywords public-address default-gateway router_open router_frame router_close
// ge:skill net
// ge:test tests/server_router_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/address.hpp"
#include "net/portmap.hpp"
#include "net/transport.hpp"

namespace ge::server {

struct ServerRouter {
  net::Portmap pm;
  net::Transport* transport = nullptr;  // the router's socket (the caller's)
  bool on = false;
  net::PortmapState told = net::PortmapState::idle;  // the state last said
  uint16_t port = 0;
  std::vector<std::string> said;  // lines for the console, in order (the caller clears them)
};

// The router as `sv_gateway` names it (`text`: an address, port 5351 when left out), else the
// machine's default route (Linux's route table); false when neither gives one.
bool router_gateway(const std::string& text, net::Address& out);

// Ask the router at `gateway` (none: UPnP's search alone) to forward UDP `port` to this machine,
// over `transport` (a socket of the router's own). `seed` draws PCP's nonce.
void router_open(ServerRouter& r, net::Transport& transport, const net::Address& gateway,
                 uint16_t port, uint64_t seed, int64_t now_us);
// Read the router's answers, renew, and say what changed.
void router_frame(ServerRouter& r, int64_t now_us);
// Ask the router to take the forward down; `router_frame` until `router_done`.
void router_close(ServerRouter& r, int64_t now_us);
bool router_done(const ServerRouter& r);

}  // namespace ge::server
