// THE FORWARDER'S WEBSOCKET FACE (docs/hub.md § Playing in a browser): the way in for a page whose
// browser has no WebTransport, or whose WebTransport session does not open. The page opens a
// WebSocket here naming its server in the path (`/play?to=host:port`), and every binary message on
// it is one of the game's datagrams, sent on to the server as UDP, one for one, as the forward
// copies any client's; each answer comes back as one message. The game on the page and the server
// never know the hop is there.
//
// A WebSocket is a TCP stream, so a datagram lost on the way is resent and holds up the ones after
// it: the game plays, but a lossy link stutters where WebTransport's datagrams would not. It is the
// fallback, never the first choice.
//
// One `Forward` does the copying, the allow list, the limit and the idle drop; its client socket is
// a pipe transport over the links, each link a client at an address of its own (`link_address`:
// the simulated network's form, never a routable one). A link that names a destination the forward
// refuses is closed with the refusal's words (the close frame's reason, which the page reads); a
// link that closes drops its client, and a client the forward drops closes its link. An open link
// is its own keepalive (TCP closes it when the page is gone).
//
//   WsForward w; ws_forward_start(w, bind, config, open_upstream, &error);
//   every frame: ws_forward_update(w, now_us)      at the end: ws_forward_close(w)
//
// ge:summary The forwarder's WebSocket face: each page's link sent on to its server as UDP
// ge:keywords forward websocket ws-forward page play-in-browser fallback link datagram pipe hop
// ge:keywords to-query link-address
// ge:skill net
// ge:test tests/forward_websocket_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "forward/forward.hpp"
#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "net/transport.hpp"

namespace ge::forward {

// The most bytes one of the game's datagrams may have on a link (a bigger message drops the link).
inline constexpr size_t kWsMostDatagram = 8192;
// A WebSocket close's reason holds at most 123 bytes.
inline constexpr size_t kWsMostReason = 123;

// The address a link's datagrams come from inside the forward.
net::Address link_address(uint32_t link);

struct WsForwardStats {
  uint64_t opened = 0, refused = 0, closed = 0, datagrams_in = 0, datagrams_out = 0;
  uint64_t backlogged = 0;  // an answer the link's stream had no room for: dropped
};

struct WsForward {
  struct Arrived {
    net::Address from;
    std::vector<uint8_t> data;
  };
  struct Link {
    uint32_t number = 0;
    net::TcpStream stream;
    std::vector<uint8_t> in;
    bool upgraded = false;  // the handshake answered
    bool admitted = false;  // a client of the forward
    bool closing = false;   // dropped at the end of the frame
  };
  net::TcpListener listener;
  net::Transport port;  // the forward's client socket: the pipe over the links
  Forward fwd;
  std::deque<Arrived> arrived;  // the links' datagrams the forward has not read yet
  std::vector<Link> links;
  uint32_t next_link = 1;
  std::string path_prefix = "/play";
  WsForwardStats stats;
  std::vector<std::string> log;  // a line for each link opened, refused and closed
  int64_t now_us = 0;
  bool started = false;
};

// Listens for pages on `bind`, forwarding as `config` allows (each client's upstream socket from
// `open_upstream`); false with `error` when the socket cannot be opened. `path_prefix` is the path
// a page's link must open on. `w` stays where it is once started (its pipe points at it).
bool ws_forward_start(WsForward& w, const net::Address& bind, const ForwardConfig& config,
                      OpenUpstream open_upstream, std::string* error,
                      std::string_view path_prefix = "/play");
void ws_forward_update(WsForward& w, int64_t now_us);
void ws_forward_close(WsForward& w);

}  // namespace ge::forward
