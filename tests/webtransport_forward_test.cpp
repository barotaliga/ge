// engine/webtransport/forward.hpp: a page's session through the forwarder to a game server on UDP.
// On loopback, a WebTransport client stands in for the page and a UDP socket that echoes for the
// server: a datagram on the session reaches the server from the forward's own socket and its
// answer comes back on the session; a server the forward does not allow closes the session with
// the refusal; a page that leaves drops its client; and the path's `to=` is read, encoded or not.
// A machine with no loopback socket skips.
//
// ge:summary Test: the forwarder's WebTransport face, a page's session to a UDP server and back
// ge:keywords test webtransport forward web-forward session datagram loopback echo allow-list
// ge:skill net testing
#include "webtransport/forward.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "net/udp_socket.hpp"

using namespace ge;
using namespace ge::webtransport;

namespace {

struct Rig {
  Certificate cert;
  WebForward w;
  net::UdpSocket game, page_socket;
  Client page;
  int64_t start_us = 0;
  uint64_t echoed = 0;
  net::Address echoed_from;
  bool ok = false;

  explicit Rig(bool allow_game) {
    std::string err;
    if (!net::udp_open(game, net::loopback_v4(0), &err)) return;
    if (!net::udp_open(page_socket, net::loopback_v4(0), &err)) return;
    if (!make_certificate(cert, 1800000000, "test", &err)) return;
    forward::ForwardConfig fc;
    if (allow_game) fc.allow.push_back(game.local);
    fc.seed = 5;
    ok = web_forward_start(
        w, cert, net::loopback_v4(0), fc,
        [](net::Transport& out) {
          return net::open_udp_transport(out, net::loopback_v4(0), nullptr);
        },
        &err);
    start_us = static_cast<int64_t>(now_ns() / 1000);
  }
  ~Rig() {
    web_forward_close(w);
    net::udp_close(game);
    net::udp_close(page_socket);
  }
  int64_t now_us() const { return static_cast<int64_t>(now_ns() / 1000) - start_us; }
  bool open(const std::string& path) {
    net::UdpSocket* ps = &page_socket;
    std::string err;
    return page.connect(
        page_socket.local, w.socket.local, "127.0.0.1", path, cert.hash,
        [ps](const net::Address& to, const uint8_t* d, size_t n) { net::udp_send(*ps, to, d, n); },
        now_ns(), &err);
  }
  // Runs the page, the forward and the echoing server until `done` or about two seconds pass.
  template <class Done>
  bool run(Done done) {
    const int64_t until = now_us() + 2000000;
    uint8_t buf[2048];
    net::Address from;
    while (now_us() < until) {
      pump(page_socket, page, now_ns());
      web_forward_update(w, now_us());
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

TEST(web_forward_reads_the_path_s_server) {
  net::Address a;
  CHECK(path_destination("/play?to=127.0.0.1:27500", a));
  CHECK(a == net::ipv4(127, 0, 0, 1, 27500));
  CHECK(path_destination("/play?x=1&to=10.0.0.7%3A27510&y", a));
  CHECK(a == net::ipv4(10, 0, 0, 7, 27510));
  CHECK(!path_destination("/play", a));
  CHECK(!path_destination("/play?to=", a));
  CHECK(!path_destination("/play?too=1.2.3.4:5", a));
  CHECK(session_address(3) != session_address(4));
}

TEST(web_forward_carries_a_page_to_its_server_and_back) {
  Rig r(true);
  if (!r.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  CHECK(r.open(r.path_to_game()));
  CHECK_MSG(r.run([&] { return r.page.state() == Client::State::open && r.w.stats.opened == 1; }),
            r.page.why());
  CHECK(r.w.fwd.clients.size() == 1);
  const uint8_t hello[6] = {0xff, 0xff, 0xff, 0xff, 'h', 'i'};
  CHECK(r.page.send(hello, sizeof hello));
  std::vector<std::vector<uint8_t>> got;
  CHECK(r.run([&] {
    for (auto& d : r.page.take_datagrams()) got.push_back(std::move(d));
    return !got.empty();
  }));
  CHECK(got.size() == 1 && got[0] == std::vector<uint8_t>(hello, hello + sizeof hello));
  CHECK(r.echoed == 1);
  // The server saw the forward's upstream socket, never the page.
  CHECK(r.echoed_from != r.page_socket.local);
  CHECK(r.w.stats.datagrams_in == 1 && r.w.stats.datagrams_out == 1);

  r.page.close("done");
  CHECK(r.run([&] { return r.w.stats.closed == 1; }));
  CHECK(r.w.sessions.empty());
  CHECK(r.run([&] { return r.w.fwd.clients.empty(); }));
}

TEST(web_forward_closes_a_session_to_a_server_it_does_not_allow) {
  Rig r(false);
  if (!r.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  CHECK(r.open(r.path_to_game()));
  CHECK(r.run([&] { return r.page.state() == Client::State::closed; }));
  CHECK(r.w.stats.refused == 1 && r.w.stats.opened == 0);
  CHECK(r.w.fwd.clients.empty());
  CHECK_MSG(r.page.why().find("does not forward to") != std::string::npos, r.page.why());
}

TEST(web_forward_closes_a_session_that_names_no_server) {
  Rig r(true);
  if (!r.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  CHECK(r.open("/play"));
  CHECK(r.run([&] { return r.page.state() == Client::State::closed; }));
  CHECK(r.w.stats.refused == 1);
}
