// The forwarder's WebSocket face (websocket.hpp): the links, the upgrade and the admission, the
// pipe the forward reads its clients from, and each end's close carried to the other.
//
// ge:summary The forwarder's WebSocket face defined: links, admission, the pipe, the closes
// ge:keywords forward websocket ws-forward link-address pipe admission close-reason
// ge:skill net
// ge:test tests/forward_websocket_test.cpp
#include "forward/websocket.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

#include "net/websocket.hpp"

namespace ge::forward {

namespace {

constexpr uint16_t kLinkPort = 80;
constexpr uint16_t kCloseNormal = 1000, kClosePolicy = 1008;

WsForward::Link* find_link(WsForward& w, uint32_t number) {
  for (WsForward::Link& l : w.links)
    if (l.number == number) return &l;
  return nullptr;
}

// A close frame with its code and reason (cut to what a close holds), and the link let go.
void close_link(WsForward::Link& l, uint16_t code, std::string_view why) {
  std::vector<uint8_t> body = {static_cast<uint8_t>(code >> 8), static_cast<uint8_t>(code)};
  const std::string_view reason = why.substr(0, kWsMostReason - 2);
  body.insert(body.end(), reason.begin(), reason.end());
  const std::vector<uint8_t> f = net::ws_frame(net::WsOp::close, body);
  net::tcp_send_bytes(l.stream, f.data(), f.size());
  l.closing = true;
}

bool pipe_send(void* ctx, const net::Address& to, const uint8_t* data, size_t size) {
  WsForward& w = *static_cast<WsForward*>(ctx);
  if (to.family != net::Family::sim || to.port != kLinkPort) return false;
  WsForward::Link* l = find_link(w, net::sim_node(to));
  if (l == nullptr || !l->admitted || l->closing) return false;
  const std::vector<uint8_t> f = net::ws_frame(net::WsOp::binary, {data, size});
  if (l->stream.out.size() - l->stream.out_head + f.size() > net::kStreamBacklog) {
    w.stats.backlogged++;  // the page is not reading: a datagram lost, as UDP would lose it
    return false;
  }
  if (!net::tcp_send_bytes(l->stream, f.data(), f.size())) return false;
  w.stats.datagrams_out++;
  return true;
}

int pipe_receive(void* ctx, uint8_t* buffer, size_t capacity, net::Address& from) {
  WsForward& w = *static_cast<WsForward*>(ctx);
  while (!w.arrived.empty()) {
    WsForward::Arrived a = std::move(w.arrived.front());
    w.arrived.pop_front();
    if (a.data.size() > capacity) continue;  // as a socket would: too big, dropped
    if (!a.data.empty()) std::memcpy(buffer, a.data.data(), a.data.size());
    from = a.from;
    return static_cast<int>(a.data.size());
  }
  return -1;
}

// The upgrade answered, then the link admitted to the server its path names, or closed with why.
void upgrade(WsForward& w, WsForward::Link& l) {
  std::string reply, error, path;
  size_t used = 0;
  const int got = net::ws_handshake(l.in, reply, used, error, &path);
  if (got == 0) return;
  if (got < 0) {
    w.stats.refused++;
    w.log.push_back("refused " + net::to_string(l.stream.peer) + ": " + error);
    l.closing = true;  // not a WebSocket: nothing to say it in
    return;
  }
  net::tcp_send_bytes(l.stream, reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
  l.in.erase(l.in.begin(), l.in.begin() + static_cast<std::ptrdiff_t>(used));
  l.upgraded = true;
  net::Address to;
  Answer refused = Answer::ok;
  std::string why;
  HopClient* c = nullptr;
  if (path.rfind(w.path_prefix, 0) != 0 || !path_destination(path, to)) {
    why = "the page named no server to join";
  } else {
    c = forward_admit(w.fwd, link_address(l.number), to, w.now_us, refused, why);
  }
  if (c == nullptr) {
    w.stats.refused++;
    w.log.push_back("refused " + net::to_string(l.stream.peer) + " (" + path + "): " + why);
    close_link(l, kClosePolicy, why);
    return;
  }
  c->connected = true;  // the link is the route: nothing more to set up
  c->name = net::to_string(l.stream.peer) + " (page)";
  l.admitted = true;
  w.stats.opened++;
  w.log.push_back("page " + net::to_string(l.stream.peer) + " to " + net::to_string(to));
}

// Every whole frame the link holds: a datagram for the forward, a ping answered, a close.
void read_frames(WsForward& w, WsForward::Link& l) {
  size_t at = 0;
  while (!l.closing) {
    net::WsFrame f;
    const int got = net::ws_parse({l.in.data() + at, l.in.size() - at}, f);
    if (got == 0) break;
    if (got < 0 || f.payload.size() > kWsMostDatagram || !f.fin) {
      w.log.push_back("page " + net::to_string(l.stream.peer) + " broke the WebSocket's rules");
      close_link(l, kClosePolicy, "a message the forward cannot carry");
      break;
    }
    at += f.size;
    if (f.op == net::WsOp::ping) {
      const std::vector<uint8_t> pong = net::ws_frame(net::WsOp::pong, f.payload);
      net::tcp_send_bytes(l.stream, pong.data(), pong.size());
    } else if (f.op == net::WsOp::close) {
      close_link(l, kCloseNormal, "");
    } else if (f.op == net::WsOp::binary && l.admitted) {
      w.stats.datagrams_in++;
      w.arrived.push_back({link_address(l.number), std::move(f.payload)});
    }
  }
  l.in.erase(l.in.begin(), l.in.begin() + static_cast<std::ptrdiff_t>(std::min(at, l.in.size())));
}

}  // namespace

net::Address link_address(uint32_t link) { return net::sim_address(link, kLinkPort); }

bool ws_forward_start(WsForward& w, const net::Address& bind, const ForwardConfig& config,
                      OpenUpstream open_upstream, std::string* error,
                      std::string_view path_prefix) {
  ws_forward_close(w);
  if (!net::tcp_listen(w.listener, bind, error)) return false;
  w.path_prefix = std::string(path_prefix);
  net::DatagramPipe pipe;
  pipe.send = pipe_send;
  pipe.receive = pipe_receive;
  pipe.ctx = &w;
  net::open_pipe_transport(w.port, pipe);
  forward_init(w.fwd, config, w.port, std::move(open_upstream));
  w.started = true;
  return true;
}

void ws_forward_update(WsForward& w, int64_t now_us) {
  if (!w.started) return;
  w.now_us = now_us;
  for (;;) {
    WsForward::Link l;
    if (!net::tcp_accept(w.listener, l.stream)) break;
    if (w.links.size() >= static_cast<size_t>(std::max(w.fwd.config.max_clients, 1)) * 2) {
      net::tcp_close(l.stream);  // more half-open links than the forward could ever admit
      w.stats.refused++;
      continue;
    }
    l.number = w.next_link++;
    w.links.push_back(std::move(l));
  }
  for (WsForward::Link& l : w.links) {
    net::tcp_pump(l.stream);
    net::tcp_take_bytes(l.stream, l.in);
    if (!l.upgraded && !l.closing) upgrade(w, l);
    if (l.upgraded) read_frames(w, l);
  }
  // A link that closed, from either end, takes its client with it.
  for (WsForward::Link& l : w.links) {
    if (!l.admitted || !(l.closing || l.stream.closed)) continue;
    if (HopClient* c = forward_client(w.fwd, link_address(l.number))) forward_drop(w.fwd, *c);
    w.log.push_back("page " + net::to_string(l.stream.peer) + " left");
    w.stats.closed++;
    l.admitted = false;
    l.closing = true;
  }
  // An open link is its own keepalive: TCP closes it when the page is gone.
  for (const WsForward::Link& l : w.links)
    if (l.admitted)
      if (HopClient* c = forward_client(w.fwd, link_address(l.number))) c->last_us = now_us;
  forward_update(w.fwd, now_us);
  // A client the forward let go takes its link with it.
  for (WsForward::Link& l : w.links) {
    if (!l.admitted || forward_client(w.fwd, link_address(l.number)) != nullptr) continue;
    w.log.push_back("page " + net::to_string(l.stream.peer) + " went quiet");
    w.stats.closed++;
    l.admitted = false;
    close_link(l, kCloseNormal, "the game went quiet");
  }
  for (WsForward::Link& l : w.links) {
    net::tcp_pump(l.stream);
    if (l.closing || l.stream.closed) net::tcp_close(l.stream);
  }
  std::erase_if(w.links, [](const WsForward::Link& l) { return l.closing || l.stream.closed; });
}

void ws_forward_close(WsForward& w) {
  if (!w.started) return;
  for (WsForward::Link& l : w.links) {
    if (l.upgraded) close_link(l, kCloseNormal, "the forward stopped");
    net::tcp_pump(l.stream);
    net::tcp_close(l.stream);
  }
  forward_close(w.fwd);
  net::close_transport(w.port);
  net::tcp_close_listener(w.listener);
  w.links.clear();
  w.arrived.clear();
  w.started = false;
}

}  // namespace ge::forward
