// THE TRANSPORT: one way to send and receive datagrams, whatever carries them: a real UDP
// socket, TCP streams carrying them as frames (tcp_socket.hpp), or a socket on the simulated
// network (whose links may be streams too: conditions.hpp). A node (node.hpp) talks only to this,
// so the same node code runs a real game and a headless lab. A transport may have a capture
// attached: every datagram it sends and receives is written to it with the time the caller
// passed, so a real session and a simulated one are captured the same way.
//
// Over TCP a host listens and holds a stream a client (up to kMaxStreams), and a client holds one
// stream to its host; a datagram goes to the stream whose peer is its address, and one to an
// address with no stream fails. A stream that closes is dropped, and the node times the peer out.
//
// ge:summary The transport: datagrams over a UDP socket or the simulated network, captured
// ge:keywords net transport link send receive udp tcp stream sim datagram capture backend
// ge:skill net
// ge:test tests/net_node_test.cpp tests/net_udp_socket_test.cpp tests/net_tcp_socket_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "net/address.hpp"
#include "net/capture.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/tcp_socket.hpp"
#include "net/udp_socket.hpp"

namespace ge::net {

enum class TransportKind : uint8_t { none, udp, sim, tcp };

// The most streams a TCP host holds: every peer's, and room for the ones it will deny.
constexpr size_t kMaxStreams = 64;

struct Tap;  // tap.hpp

// FAKE CONDITIONS on a live transport, for a developer's experiments on a real game: every
// datagram it sends is put through `conditions` (conditions.hpp's fates: delay, jitter, loss,
// a drop pattern) before it leaves, so a LAN game plays as a bad link would. Datagrams held for
// their delay wait in a fixed pool and leave on the transport's next send or receive.
struct FakeLink {
  LinkConditions conditions;
  LinkState state;
  core::Rng rng{0};
  struct Held {
    int64_t at_us = 0;
    Address to;
    uint32_t size = 0;
    uint8_t data[kMaxDatagram];
  };
  std::vector<Held> held;    // the pool, sized once
  std::vector<int> waiting;  // indices into `held`, in the order they were held
  std::vector<int> free;
  uint64_t lost = 0, delayed = 0, overflow = 0;
};

struct Transport {
  TransportKind kind = TransportKind::none;
  Address local;
  UdpSocket udp;
  TcpListener listener;            // a TCP host's
  std::vector<TcpStream> streams;  // a TCP host's accepted streams, or a client's one
  size_t next_stream = 0;          // where the next receive starts looking, round the streams
  SimNetwork* sim = nullptr;
  Capture* capture = nullptr;  // when set, every datagram in and out is written to it
  Tap* tap = nullptr;          // when set, every datagram in and out is mirrored to it (tap.hpp)
  FakeLink* fake = nullptr;    // when set, every send goes through its conditions first
  uint64_t send_failures = 0;
};

// A socket on the simulated network at `local` (family sim); false when it is taken.
bool open_sim_transport(Transport& t, SimNetwork& net, const Address& local);
// A real UDP socket bound at `bind`; false and `error` when the OS refuses it.
bool open_udp_transport(Transport& t, const Address& bind, std::string* error);
// A TCP host listening at `bind`; a TCP client connecting to `host` (it sends once connected).
bool open_tcp_host_transport(Transport& t, const Address& bind, std::string* error);
bool open_tcp_client_transport(Transport& t, const Address& host, std::string* error);
void close_transport(Transport& t);
// Fake conditions on `t`'s sends (null `fake` or default conditions: none); `pool` datagrams can
// be held at once, the rest dropped (counted in `overflow`).
void transport_fake(Transport& t, FakeLink* fake, const LinkConditions& c, uint64_t seed,
                    size_t pool = 256);
// Send what fake conditions held and is now due (a send and a receive do it too).
void transport_pump(Transport& t, int64_t now_us);

bool transport_send(Transport& t, const Address& to, const uint8_t* data, size_t size,
                    int64_t now_us);
// The next datagram waiting: its size, or -1 when none is. A simulated socket delivers what the
// network has delivered by its own clock; advance it first (sim_advance).
int transport_receive(Transport& t, uint8_t* buffer, size_t capacity, Address& from,
                      int64_t now_us);

}  // namespace ge::net
