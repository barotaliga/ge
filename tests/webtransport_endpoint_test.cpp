// engine/webtransport/endpoint.hpp: a server and a client on loopback UDP. The session opens on a
// path the server serves, datagrams go both ways, the client refuses a certificate that is not
// the one its hash names, a path the server does not serve is answered 404, and a close reaches
// the other side. A machine with no loopback socket skips.
//
// ge:summary Test: a WebTransport session on loopback: open, datagrams, refusals, close
// ge:keywords test webtransport quic session datagram loopback certificate-hash
// ge:skill net testing
#include "webtransport/endpoint.hpp"

#include <string>

#include "check.hpp"

using namespace ge;
using namespace ge::webtransport;

namespace {

struct Pair {
  net::UdpSocket server_socket, client_socket;
  Certificate cert;
  Server server;
  Client client;
  std::vector<Event> events;
  bool ok = false;

  explicit Pair(const ServerConfig& config = {}) {
    std::string err;
    if (!net::udp_open(server_socket, net::loopback_v4(0), &err)) return;
    if (!net::udp_open(client_socket, net::loopback_v4(0), &err)) return;
    if (!make_certificate(cert, 1800000000, "test", &err)) return;
    net::UdpSocket* ss = &server_socket;
    server.set_local(server_socket.local);
    ok = server.start(
        cert, config,
        [ss](const net::Address& to, const uint8_t* d, size_t n) { net::udp_send(*ss, to, d, n); },
        &err);
  }
  ~Pair() {
    net::udp_close(server_socket);
    net::udp_close(client_socket);
  }
  bool connect(std::string_view path, const std::array<uint8_t, 32>& hash) {
    net::UdpSocket* cs = &client_socket;
    std::string err;
    return client.connect(
        client_socket.local, server_socket.local, "127.0.0.1", path, hash,
        [cs](const net::Address& to, const uint8_t* d, size_t n) { net::udp_send(*cs, to, d, n); },
        now_ns(), &err);
  }
  // Runs both ends until `done` or about two seconds pass.
  template <class Done>
  bool run(Done done) {
    const uint64_t start = now_ns();
    while (now_ns() - start < 2000000000ull) {
      const uint64_t now = now_ns();
      pump(server_socket, server, now);
      pump(client_socket, client, now);
      for (Event& e : server.take_events()) events.push_back(std::move(e));
      if (done()) return true;
    }
    return false;
  }
  int count(Event::Kind kind) const {
    int n = 0;
    for (const Event& e : events) n += e.kind == kind;
    return n;
  }
};

}  // namespace

TEST(webtransport_session_opens_and_echoes) {
  Pair p;
  if (!p.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  CHECK(p.connect("/play?to=127.0.0.1:27500", p.cert.hash));
  CHECK_MSG(p.run([&] { return p.client.state() == Client::State::open && !p.events.empty(); }),
            p.client.why());
  CHECK(p.count(Event::Kind::opened) == 1);
  CHECK(p.events[0].path == "/play?to=127.0.0.1:27500");
  CHECK(p.server.sessions() == 1);
  const uint32_t session = p.events[0].session;

  const uint8_t ping[5] = {'p', 'i', 'n', 'g', 0};
  CHECK(p.client.send(ping, sizeof ping));
  CHECK(p.run([&] { return p.count(Event::Kind::datagram) == 1; }));
  const uint8_t pong[1000] = {9};
  CHECK(p.server.send(session, pong, sizeof pong));
  std::vector<std::vector<uint8_t>> got;
  CHECK(p.run([&] {
    for (auto& d : p.client.take_datagrams()) got.push_back(std::move(d));
    return !got.empty();
  }));
  CHECK(got.size() == 1 && got[0].size() == 1000 && got[0][0] == 9);
  const uint8_t too_big[kMaxDatagram + 1] = {};
  CHECK(!p.server.send(session, too_big, sizeof too_big));
  CHECK(!p.client.send(too_big, sizeof too_big));

  p.client.close("done");
  CHECK(p.run([&] { return p.count(Event::Kind::closed) == 1; }));
  CHECK(p.server.sessions() == 0);
  CHECK(!p.server.send(session, ping, sizeof ping));
}

TEST(webtransport_client_refuses_another_certificate) {
  Pair p;
  if (!p.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  std::array<uint8_t, 32> wrong = p.cert.hash;
  wrong[0] ^= 1;
  CHECK(p.connect("/play", wrong));
  CHECK(p.run([&] { return p.client.state() == Client::State::closed; }));
  CHECK(p.client.why().find("not the one its hash names") != std::string::npos);
  CHECK(p.count(Event::Kind::opened) == 0);
}

TEST(webtransport_server_answers_404_off_its_path) {
  ServerConfig config;
  config.path_prefix = "/play";
  Pair p(config);
  if (!p.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  CHECK(p.connect("/other", p.cert.hash));
  CHECK(p.run([&] { return p.client.state() == Client::State::closed; }));
  CHECK(p.client.why().find("404") != std::string::npos);
  CHECK(p.count(Event::Kind::opened) == 0);
}

TEST(webtransport_server_close_reaches_the_client) {
  Pair p;
  if (!p.ok) {
    ge::test::skip("no loopback UDP socket here");
    return;
  }
  CHECK(p.connect("/", p.cert.hash));
  CHECK(p.run([&] { return p.client.state() == Client::State::open && !p.events.empty(); }));
  p.server.close(p.events[0].session, "the server is going down");
  for (Event& e : p.server.take_events()) p.events.push_back(std::move(e));
  CHECK(p.count(Event::Kind::closed) == 1);
  CHECK(p.events.back().why == "the server is going down");
  CHECK(p.run([&] { return p.client.state() == Client::State::closed; }));
  CHECK_MSG(p.client.why() == "the server is going down", p.client.why());
  CHECK(p.server.connections() == 0);
}

TEST(webtransport_server_without_a_certificate_refuses_to_start) {
  Server s;
  std::string err;
  CHECK(!s.start(Certificate{}, ServerConfig{}, nullptr, &err));
  CHECK(err.find("no certificate") != std::string::npos);
}
