// engine/forward/websocket.hpp: a page's WebSocket through the forwarder to a game server on UDP.
// On loopback, a TCP stream that speaks WebSocket stands in for the page and a UDP socket that
// echoes for the server: a message on the link reaches the server from the forward's own socket and
// its answer comes back as one message; a server the forward does not allow closes the link with
// the refusal as the close's reason; a page that leaves drops its client; and the handshake reads
// the path. A machine with no loopback socket skips.
//
// ge:summary Test: the forwarder's WebSocket face, a page's link to a UDP server and back
// ge:keywords test forward websocket ws-forward link datagram loopback echo allow-list close-reason
// ge:skill net testing
#include "forward/websocket.hpp"

#include <chrono>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/udp_socket.hpp"
#include "net/websocket.hpp"

using namespace ge;
using namespace ge::forward;

namespace {

int64_t clock_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::vector<uint8_t> bytes_of(std::string_view s) { return {s.begin(), s.end()}; }

struct Rig {
  WsForward w;
  net::UdpSocket game;
  net::TcpStream page;
  std::vector<uint8_t> page_in;
  int64_t start_us = clock_us();
  uint64_t echoed = 0;
  net::Address echoed_from;
  bool ok = false;

  explicit Rig(bool allow_game) {
    std::string err;
    if (!net::udp_open(game, net::loopback_v4(0), &err)) return;
    ForwardConfig fc;
    if (allow_game) fc.allow.push_back(game.local);
    fc.seed = 5;
    ok = ws_forward_start(
        w, net::loopback_v4(0), fc,
        [](net::Transport& out) {
          return net::open_udp_transport(out, net::loopback_v4(0), nullptr);
        },
        &err);
  }
  ~Rig() {
    net::tcp_close(page);
    ws_forward_close(w);
    net::udp_close(game);
  }
  int64_t now_us() const { return clock_us() - start_us; }
  // The page's upgrade on `path`.
  bool open(const std::string& path) {
    if (!net::tcp_connect(page, net::loopback_v4(w.listener.local.port), nullptr)) return false;
    const std::string req = "GET " + path +
                            " HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
                            "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                            "Sec-WebSocket-Version: 13\r\n\r\n";
    return net::tcp_send_bytes(page, reinterpret_cast<const uint8_t*>(req.data()), req.size());
  }
  void send(net::WsOp op, std::span<const uint8_t> payload) {
    const uint8_t mask[4] = {1, 2, 3, 4};  // a page's frames are masked
    const auto f = net::ws_frame(op, payload, mask);
    net::tcp_send_bytes(page, f.data(), f.size());
  }
  // The page's next frame, the server's answer skipped past the upgrade's reply.
  bool take(net::WsFrame& f) {
    const std::string_view text(reinterpret_cast<const char*>(page_in.data()), page_in.size());
    size_t at = 0;
    if (text.rfind("HTTP/1.1 101", 0) == 0) {
      const size_t end = text.find("\r\n\r\n");
      if (end == std::string_view::npos) return false;
      at = end + 4;
    }
    if (net::ws_parse({page_in.data() + at, page_in.size() - at}, f) != 1) return false;
    page_in.erase(page_in.begin(), page_in.begin() + static_cast<std::ptrdiff_t>(at + f.size));
    return true;
  }
  // Runs the page, the forward and the echoing server until `done` or about two seconds pass.
  template <class Done>
  bool run(Done done) {
    const int64_t until = now_us() + 2000000;
    uint8_t buf[2048];
    net::Address from;
    while (now_us() < until) {
      net::tcp_pump(page);
      net::tcp_take_bytes(page, page_in);
      ws_forward_update(w, now_us());
      int got = 0;
      while ((got = net::udp_receive(game, buf, sizeof buf, from)) >= 0) {
        echoed++;
        echoed_from = from;
        net::udp_send(game, from, buf, static_cast<size_t>(got));
      }
      if (done()) return true;
    }
    return false;
  }
  std::string path_to_game() const { return "/play?to=" + net::to_string(game.local); }
};

}  // namespace

TEST(the_handshake_reads_the_path) {
  const std::string req =
      "GET /play?to=10.0.0.7%3A27510 HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: "
      "dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
  std::string reply, error, path;
  size_t used = 0;
  CHECK(net::ws_handshake(bytes_of(req), reply, used, error, &path) == 1);
  CHECK(path == "/play?to=10.0.0.7%3A27510");
  net::Address a;
  CHECK(path_destination(path, a) && net::to_string(a) == "10.0.0.7:27510");
  CHECK(link_address(3) != link_address(4));
}

TEST(a_page_s_link_reaches_its_server_and_back) {
  Rig r(true);
  if (!r.ok) {
    ge::test::skip("no loopback socket here");
    return;
  }
  CHECK(r.open(r.path_to_game()));
  CHECK(r.run([&] { return r.w.stats.opened == 1; }));
  CHECK(r.w.fwd.clients.size() == 1);
  const std::vector<uint8_t> hello = {0xff, 0xff, 0xff, 0xff, 'h', 'i'};
  r.send(net::WsOp::binary, hello);
  net::WsFrame got;
  CHECK(r.run([&] { return r.take(got); }));
  CHECK(got.op == net::WsOp::binary && got.payload == hello);
  CHECK(r.echoed == 1);
  // The server saw the forward's upstream socket, never the page.
  CHECK(r.echoed_from != r.page.local);
  CHECK(r.w.stats.datagrams_in == 1 && r.w.stats.datagrams_out == 1);

  // A ping is answered; then the page leaves and its client goes with it.
  r.send(net::WsOp::ping, bytes_of("p"));
  CHECK(r.run([&] { return r.take(got); }));
  CHECK(got.op == net::WsOp::pong && got.payload == bytes_of("p"));
  r.send(net::WsOp::close, {});
  CHECK(r.run([&] { return r.w.stats.closed == 1; }));
  CHECK(r.run([&] { return r.w.links.empty() && r.w.fwd.clients.empty(); }));
}

TEST(a_link_to_a_server_the_forward_does_not_allow_is_closed_with_why) {
  Rig r(false);
  if (!r.ok) {
    ge::test::skip("no loopback socket here");
    return;
  }
  CHECK(r.open(r.path_to_game()));
  net::WsFrame got;
  CHECK(r.run([&] { return r.take(got); }));
  CHECK(got.op == net::WsOp::close && got.payload.size() > 2);
  CHECK((got.payload[0] << 8 | got.payload[1]) == 1008);
  const std::string why(got.payload.begin() + 2, got.payload.end());
  CHECK_MSG(why.find("does not forward to") != std::string::npos, why);
  CHECK(r.w.stats.refused == 1 && r.w.stats.opened == 0);
  CHECK(r.w.fwd.clients.empty());
}

TEST(a_link_that_names_no_server_is_closed) {
  Rig r(true);
  if (!r.ok) {
    ge::test::skip("no loopback socket here");
    return;
  }
  CHECK(r.open("/play"));
  net::WsFrame got;
  CHECK(r.run([&] { return r.take(got); }));
  CHECK(got.op == net::WsOp::close);
  CHECK(r.w.stats.refused == 1);
}
