// WEBTRANSPORT (docs/hub.md § Playing in a browser): how a game in a browser page reaches the
// network without a WebSocket server in between. A page cannot send UDP; it can open a
// WebTransport session (QUIC, HTTP/3, an extended CONNECT) and send unreliable datagrams on it,
// which is what a game's packets want. The endpoint here is the far side of that session: the
// forwarder (engine/forward) takes each session's datagrams and sends them on to the game server
// as UDP, one for one.
//
// THE SERVER takes UDP datagrams from its socket (`receive`), answers through the caller's `Send`,
// and reports what happened as events: a session opened (its path, origin and peer), a datagram
// on it, a session closed and why. One session per connection, as a page opens it. QUIC (ngtcp2)
// and TLS 1.3 (picotls) do the transport; h3.hpp the HTTP/3 a browser needs. The certificate is
// certificate.hpp's: self-signed, trusted by its hash, which the page is handed.
//
//   Server s; s.start(cert, config, send, &error);
//   every datagram from the socket: s.receive(from, data, size, now_ns)
//   every frame: s.update(now_ns); for (Event& e : s.take_events()) ...; s.send(session, ...)
//
// THE CLIENT is the same session from the other end, for tests and tools: it connects, checks
// the certificate against the hash it was given and the handshake's signature against the
// certificate's key, opens the session and sends datagrams. A browser does the same.
//
// Not simulation code: the clock is the caller's (nanoseconds, any origin, never going back), but
// QUIC's timers, its random connection ids and the TLS keys make no two runs alike.
//
// ge:summary A WebTransport server and client: QUIC datagrams in sessions a browser opens
// ge:keywords webtransport quic http3 datagram session browser ngtcp2 picotls tls13 server client
// ge:keywords extended-connect forward page
// ge:skill net
// ge:test tests/webtransport_endpoint_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "net/address.hpp"
#include "net/udp_socket.hpp"
#include "webtransport/certificate.hpp"

namespace ge::webtransport {

// A datagram's payload may be at most this big: what fits in one QUIC packet on any path a page
// takes (the minimum QUIC packet less its headers and the session id).
constexpr size_t kMaxDatagram = 1150;

// What the caller sends for the endpoint: one UDP datagram to `to`.
using Send = std::function<void(const net::Address& to, const uint8_t* data, size_t size)>;

struct Event {
  enum class Kind : uint8_t { opened, datagram, closed };
  Kind kind = Kind::datagram;
  uint32_t session = 0;       // the endpoint's own number for it, from 1
  net::Address peer;          // the page's address
  std::string path;           // opened: the CONNECT's path ("/play?to=...")
  std::string origin;         // opened: the page's origin
  std::vector<uint8_t> data;  // datagram: the payload
  std::string why;            // closed: what ended it
};

struct ServerConfig {
  std::string path_prefix = "/";  // a CONNECT whose path starts otherwise is answered 404
  size_t max_connections = 256;   // a new one past it is not answered
  uint64_t idle_ms = 30000;       // a connection silent this long is closed
};

class Server {
 public:
  Server();
  ~Server();
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  // Ready to answer with `cert`; `send` carries every datagram out. False with `error` when the
  // TLS context cannot be set up.
  bool start(const Certificate& cert, const ServerConfig& config, Send send, std::string* error);
  // `local` is the socket's own address (the path's local end); the default is fine for one
  // socket.
  void set_local(const net::Address& local);
  void receive(const net::Address& from, const uint8_t* data, size_t size, uint64_t now_ns);
  void update(uint64_t now_ns);
  // Queues a datagram on `session` for the next `update`; false when the session is gone or the
  // payload is bigger than kMaxDatagram.
  bool send(uint32_t session, const uint8_t* data, size_t size);
  // Ends a session (and its connection); its `closed` event follows.
  void close(uint32_t session, std::string_view why);
  std::vector<Event> take_events();
  size_t connections() const;
  size_t sessions() const;

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

class Client {
 public:
  enum class State : uint8_t { idle, connecting, open, closed };
  Client();
  ~Client();
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Starts the handshake with `server`; the session opens on `path` once the certificate matches
  // `hash`. `authority` is the host the page would name ("127.0.0.1:4433").
  bool connect(const net::Address& local, const net::Address& server, std::string_view authority,
               std::string_view path, const std::array<uint8_t, 32>& hash, Send send,
               uint64_t now_ns, std::string* error);
  void receive(const net::Address& from, const uint8_t* data, size_t size, uint64_t now_ns);
  void update(uint64_t now_ns);
  bool send(const uint8_t* data, size_t size);
  void close(std::string_view why);
  State state() const;
  // The datagrams that arrived since the last call, oldest first.
  std::vector<std::vector<uint8_t>> take_datagrams();
  // Why the session closed or never opened.
  const std::string& why() const;

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

// Drains `socket` into `server` (or `client`) and runs its timers: one call a frame.
void pump(net::UdpSocket& socket, Server& server, uint64_t now_ns);
void pump(net::UdpSocket& socket, Client& client, uint64_t now_ns);

// The monotonic clock in nanoseconds, for a caller with no clock of its own.
uint64_t now_ns();

}  // namespace ge::webtransport
