// A LIVE TAP: a running node's every datagram mirrored, as it happens, to a local port where
// `ge net watch` listens, so a game or a dedicated server is measured while it plays without
// stopping it or touching its sockets. Each mirrored datagram is one pcap record (capture.hpp's,
// the same bytes a saved capture holds) behind a four-byte mark, sent over loopback UDP; a tap
// with nobody listening costs a send a datagram and nothing else. Point a transport's `tap` at
// one (transport.hpp), as with a capture.
//
//   net::Tap tap;  net::open_tap(tap, 27950, &error);   transport.tap = &tap;
//   ge net watch --port=27950                            (per connection, every second)
//
// ge:summary A live tap: a running node's datagrams mirrored to a local port for ge net watch
// ge:keywords net tap live watch mirror capture pcap attach running server loopback inspect
// ge:skill net
// ge:test tests/net_tap_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/address.hpp"
#include "net/capture.hpp"
#include "net/udp_socket.hpp"

namespace ge::net {

constexpr uint8_t kTapMark[4] = {'g', 'e', 't', 'p'};
constexpr uint16_t kTapPort = 27950;  // where `ge net watch` listens unless told otherwise

struct Tap {
  UdpSocket socket;
  Address to;       // the watcher: loopback at the tap's port
  Capture scratch;  // one record at a time
  std::vector<uint8_t> frame;
  uint64_t mirrored = 0, failed = 0;
};

// A tap sending to 127.0.0.1:`port` from a port of its own; false and `error` when the OS
// refuses the socket.
bool open_tap(Tap& tap, uint16_t port, std::string* error);
void close_tap(Tap& tap);
void tap_datagram(Tap& tap, int64_t at_us, const Address& from, const Address& to,
                  const uint8_t* data, size_t size);
// One tap frame read back into the datagram it mirrors; false when it is not one.
bool read_tap_frame(const uint8_t* frame, size_t size, CapturedDatagram& out);

}  // namespace ge::net
