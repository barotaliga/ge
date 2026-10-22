// THE TRANSPORT: one way to send and receive datagrams, whatever carries them: a real UDP
// socket or a socket on the simulated network (and a TCP stream, M17c). A node (node.hpp) talks
// only to this, so the same node code runs a real game and a headless lab. A transport may have
// a capture attached: every datagram it sends and receives is written to it with the time the
// caller passed, so a real session and a simulated one are captured the same way.
//
// ge:summary The transport: datagrams over a UDP socket or the simulated network, captured
// ge:keywords net transport link send receive udp sim datagram capture backend
// ge:skill net
// ge:test tests/net_node_test.cpp tests/net_udp_socket_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "net/address.hpp"
#include "net/capture.hpp"
#include "net/sim_network.hpp"
#include "net/udp_socket.hpp"

namespace ge::net {

enum class TransportKind : uint8_t { none, udp, sim };

struct Transport {
  TransportKind kind = TransportKind::none;
  Address local;
  UdpSocket udp;
  SimNetwork* sim = nullptr;
  Capture* capture = nullptr;  // when set, every datagram in and out is written to it
  uint64_t send_failures = 0;
};

// A socket on the simulated network at `local` (family sim); false when it is taken.
bool open_sim_transport(Transport& t, SimNetwork& net, const Address& local);
// A real UDP socket bound at `bind`; false and `error` when the OS refuses it.
bool open_udp_transport(Transport& t, const Address& bind, std::string* error);
void close_transport(Transport& t);

bool transport_send(Transport& t, const Address& to, const uint8_t* data, size_t size,
                    int64_t now_us);
// The next datagram waiting: its size, or -1 when none is. A simulated socket delivers what the
// network has delivered by its own clock; advance it first (sim_advance).
int transport_receive(Transport& t, uint8_t* buffer, size_t capacity, Address& from,
                      int64_t now_us);

}  // namespace ge::net
