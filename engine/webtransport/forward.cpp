// The forwarder's WebTransport face (forward.hpp): the pipe the forward reads its clients from, the
// sessions admitted by the server their path names, and each end's close carried to the other.
//
// ge:summary The forwarder's WebTransport face defined: the pipe, the admission, the closes
// ge:keywords webtransport forward web-forward pipe session-address path-destination
// ge:skill net
// ge:test tests/webtransport_forward_test.cpp
#include "webtransport/forward.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace ge::webtransport {

namespace {

constexpr uint16_t kSessionPort = 443;

int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string unescape(std::string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
      out += static_cast<char>(hex(s[i + 1]) * 16 + hex(s[i + 2]));
      i += 2;
    } else {
      out += s[i] == '+' ? ' ' : s[i];
    }
  }
  return out;
}

WebForward::Session* find_session(WebForward& w, uint32_t number) {
  for (WebForward::Session& s : w.sessions)
    if (s.number == number) return &s;
  return nullptr;
}

bool pipe_send(void* ctx, const net::Address& to, const uint8_t* data, size_t size) {
  WebForward& w = *static_cast<WebForward*>(ctx);
  if (to.family != net::Family::sim || to.port != kSessionPort) return false;
  const uint32_t number = net::sim_node(to);
  if (find_session(w, number) == nullptr) return false;
  if (size > kMaxDatagram) {
    w.stats.too_big++;
    return false;
  }
  if (!w.server.send(number, data, size)) return false;
  w.stats.datagrams_out++;
  return true;
}

int pipe_receive(void* ctx, uint8_t* buffer, size_t capacity, net::Address& from) {
  WebForward& w = *static_cast<WebForward*>(ctx);
  while (!w.arrived.empty()) {
    WebForward::Arrived a = std::move(w.arrived.front());
    w.arrived.pop_front();
    if (a.data.size() > capacity) continue;  // as a socket would: too big, dropped
    if (!a.data.empty()) std::memcpy(buffer, a.data.data(), a.data.size());
    from = a.from;
    return static_cast<int>(a.data.size());
  }
  return -1;
}

void opened(WebForward& w, const Event& e) {
  net::Address to;
  forward::Answer refused = forward::Answer::ok;
  std::string why;
  forward::HopClient* c = nullptr;
  if (!path_destination(e.path, to)) {
    why = "the page named no server to join";
  } else {
    c = forward::forward_admit(w.fwd, session_address(e.session), to, w.now_us, refused, why);
  }
  if (c == nullptr) {
    w.stats.refused++;
    w.log.push_back("refused " + net::to_string(e.peer) + " (" + e.path + "): " + why);
    w.server.close(e.session, why);
    return;
  }
  c->connected = true;  // the session is the route: nothing more to set up
  c->name = net::to_string(e.peer) + " (page)";
  w.sessions.push_back({e.session, e.peer});
  w.stats.opened++;
  w.log.push_back("page " + net::to_string(e.peer) + " to " + net::to_string(to));
}

void closed(WebForward& w, const Event& e) {
  WebForward::Session* s = find_session(w, e.session);
  if (s == nullptr) return;
  if (forward::HopClient* c = forward::forward_client(w.fwd, session_address(e.session)))
    forward::forward_drop(w.fwd, *c);
  w.log.push_back("page " + net::to_string(s->peer) + " left: " + e.why);
  w.stats.closed++;
  w.sessions.erase(w.sessions.begin() + (s - w.sessions.data()));  // s points into w.sessions
}

}  // namespace

net::Address session_address(uint32_t session) { return net::sim_address(session, kSessionPort); }

bool path_destination(std::string_view path, net::Address& out) {
  const size_t q = path.find('?');
  if (q == std::string_view::npos) return false;
  std::string_view query = path.substr(q + 1);
  while (!query.empty()) {
    const size_t amp = query.find('&');
    const std::string_view pair = query.substr(0, amp);
    if (pair.substr(0, 3) == "to=") return net::parse_address(unescape(pair.substr(3)), out);
    if (amp == std::string_view::npos) break;
    query.remove_prefix(amp + 1);
  }
  return false;
}

bool web_forward_start(WebForward& w, const Certificate& cert, const net::Address& bind,
                       const forward::ForwardConfig& config, forward::OpenUpstream open_upstream,
                       std::string* error, std::string_view path_prefix) {
  web_forward_close(w);
  if (!net::open_udp_transport(w.socket, bind, error)) return false;
  WebForward* self = &w;
  ServerConfig sc;
  sc.path_prefix = std::string(path_prefix);
  sc.max_connections = static_cast<size_t>(std::max(config.max_clients, 1));
  w.server.set_local(w.socket.local);
  if (!w.server.start(
          cert, sc,
          [self](const net::Address& to, const uint8_t* d, size_t n) {
            net::transport_send(self->socket, to, d, n, self->now_us);
          },
          error)) {
    net::close_transport(w.socket);
    return false;
  }
  net::DatagramPipe pipe;
  pipe.send = pipe_send;
  pipe.receive = pipe_receive;
  pipe.ctx = self;
  net::open_pipe_transport(w.port, pipe);
  forward::forward_init(w.fwd, config, w.port, std::move(open_upstream));
  w.started = true;
  return true;
}

void web_forward_update(WebForward& w, int64_t now_us) {
  if (!w.started) return;
  w.now_us = now_us;
  const uint64_t now_ns = static_cast<uint64_t>(std::max<int64_t>(now_us, 0)) * 1000u;
  uint8_t buf[2048];
  net::Address from;
  int got = 0;
  while ((got = net::transport_receive(w.socket, buf, sizeof buf, from, now_us)) >= 0)
    w.server.receive(from, buf, static_cast<size_t>(got), now_ns);
  w.server.update(now_ns);
  for (Event& e : w.server.take_events()) {
    switch (e.kind) {
      case Event::Kind::opened:
        opened(w, e);
        break;
      case Event::Kind::datagram:
        if (find_session(w, e.session) != nullptr) {
          w.stats.datagrams_in++;
          w.arrived.push_back({session_address(e.session), std::move(e.data)});
        }
        break;
      case Event::Kind::closed:
        closed(w, e);
        break;
    }
  }
  // An open session is its own keepalive: QUIC closes it when the page is gone, and a page that
  // stops for a while (a tab in the background, a slow first frame) keeps its place.
  for (const WebForward::Session& s : w.sessions)
    if (forward::HopClient* c = forward::forward_client(w.fwd, session_address(s.number)))
      c->last_us = now_us;
  forward::forward_update(w.fwd, now_us);
  // A client the forward let go takes its session with it.
  for (size_t i = 0; i < w.sessions.size();) {
    if (forward::forward_client(w.fwd, session_address(w.sessions[i].number)) == nullptr) {
      w.log.push_back("page " + net::to_string(w.sessions[i].peer) + " went quiet");
      w.server.close(w.sessions[i].number, "the game went quiet");
      w.stats.closed++;
      w.sessions.erase(w.sessions.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      i++;
    }
  }
  w.server.update(now_ns);  // the answers out this frame
}

void web_forward_close(WebForward& w) {
  if (!w.started) return;
  for (const WebForward::Session& s : w.sessions) w.server.close(s.number, "the forward stopped");
  w.server.update(static_cast<uint64_t>(std::max<int64_t>(w.now_us, 0)) * 1000u);
  forward::forward_close(w.fwd);
  net::close_transport(w.port);
  net::close_transport(w.socket);
  w.sessions.clear();
  w.arrived.clear();
  w.started = false;
}

}  // namespace ge::webtransport
