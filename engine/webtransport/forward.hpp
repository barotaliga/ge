// THE FORWARDER'S WEBTRANSPORT FACE (docs/hub.md § Playing in a browser): a game in a browser page
// cannot send UDP, so it opens a WebTransport session here, naming its server in the path
// (`/play?to=host:port`), and every datagram on that session goes on to the server as UDP, one
// for one, as engine/forward copies any client's. The answers come back the same way onto the
// session. The game on the page and the server never know the hop is there: the server sees the
// forward's own socket as the player, as it would behind any forwarder.
//
// One `Forward` (engine/forward) does the copying, the allow list, the limit and the idle drop;
// its client socket is a pipe transport (net/transport.hpp) over the WebTransport server, each
// session a client at an address of its own (`session_address`: the simulated network's form,
// never a routable one). A session opened to a destination the forward refuses is closed with the
// refusal's words; a session that closes drops its client, and a client the forward drops closes
// its session. An open session is its own keepalive (QUIC closes it when the page is gone), so a
// page that stops for a while keeps its place.
//
//   WebForward w; web_forward_start(w, cert, bind, config, open_upstream, &error);
//   every frame: web_forward_update(w, now_us)      at the end: web_forward_close(w)
//
// Not simulation code: QUIC's timers and keys make no two runs alike.
//
// ge:summary The forwarder's WebTransport face: each page's session sent on to its server as UDP
// ge:keywords webtransport forward web-forward page play-in-browser session datagram pipe hop
// ge:keywords to-query session-address
// ge:skill net
// ge:test tests/webtransport_forward_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "forward/forward.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"
#include "webtransport/certificate.hpp"
#include "webtransport/endpoint.hpp"

namespace ge::webtransport {

// The address a session's datagrams come from inside the forward.
net::Address session_address(uint32_t session);
// The server a session's path names (forward.hpp's, shared with the WebSocket face).
using forward::path_destination;

struct WebForwardStats {
  uint64_t opened = 0, refused = 0, closed = 0, datagrams_in = 0, datagrams_out = 0;
  uint64_t too_big = 0;  // an answer bigger than a session's datagram: dropped
};

struct WebForward {
  struct Arrived {
    net::Address from;
    std::vector<uint8_t> data;
  };
  struct Session {
    uint32_t number = 0;
    net::Address peer;  // the page's
  };
  net::Transport socket;  // the WebTransport server's UDP socket
  Server server;
  net::Transport port;  // the forward's client socket: the pipe over `server`
  forward::Forward fwd;
  std::deque<Arrived> arrived;  // the sessions' datagrams the forward has not read yet
  std::vector<Session> sessions;
  WebForwardStats stats;
  std::vector<std::string> log;  // a line for each session opened, refused and closed
  int64_t now_us = 0;            // the frame's clock, for the socket's sends
  bool started = false;
};

// Listens for pages on `bind` with `cert`, forwarding as `config` allows (each client's upstream
// socket from `open_upstream`); false with `error` when the socket or TLS cannot be set up.
// `path_prefix` is the path a page's session must open on. `w` stays where it is once started (its
// pipe and its server point at it).
bool web_forward_start(WebForward& w, const Certificate& cert, const net::Address& bind,
                       const forward::ForwardConfig& config, forward::OpenUpstream open_upstream,
                       std::string* error, std::string_view path_prefix = "/play");
void web_forward_update(WebForward& w, int64_t now_us);
void web_forward_close(WebForward& w);

}  // namespace ge::webtransport
