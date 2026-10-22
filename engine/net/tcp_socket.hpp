// THE OS'S TCP SOCKETS, carrying the engine's packets as FRAMES: each one its length (16 bits,
// little endian) and its bytes, so the stream gives back the same datagrams that went in, in
// order. For games that want a stream and for networks that let no UDP through (docs/networking.md
// § Decisions). Non-blocking like the UDP socket: `tcp_pump` moves what the OS will take and what
// it has, a frame waits in the stream's buffers until it is whole, and nothing ever blocks a
// frame. A listener accepts streams; a client's stream connects in the background.
//
// ge:summary The OS's non-blocking TCP sockets carrying length-framed datagrams; listen, connect
// ge:keywords net tcp socket stream listen accept connect frame framing length-prefixed
// ge:keywords nodelay non-blocking backlog head-of-line
// ge:skill net
// ge:test tests/net_tcp_socket_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/address.hpp"

namespace ge::net {

// The most a stream holds unsent before a send is refused: the back-pressure a slow link gives.
constexpr size_t kStreamBacklog = size_t{256} * 1024;
// The largest frame: its length is 16 bits.
constexpr size_t kMaxFrame = 0xffff;

struct TcpListener {
  intptr_t handle = -1;
  Address local;
};

struct TcpStream {
  intptr_t handle = -1;
  Address local, peer;
  bool connecting = false;   // a connect still in flight: frames wait until it is up
  bool closed = false;       // the other side closed it or the OS failed it
  std::vector<uint8_t> out;  // frames the OS has not taken yet, from out_head
  size_t out_head = 0;
  std::vector<uint8_t> in;  // bytes read and not yet a whole frame, from in_head
  size_t in_head = 0;
};

// Listen at `bind` (port 0: any free port); false and `error` when the OS refuses.
bool tcp_listen(TcpListener& l, const Address& bind, std::string* error);
void tcp_close_listener(TcpListener& l);
// The next connection waiting on the listener, as a stream; false when none is.
bool tcp_accept(TcpListener& l, TcpStream& out);

// Start connecting to `to`; the stream is `connecting` until the OS says it is up.
bool tcp_connect(TcpStream& s, const Address& to, std::string* error);
void tcp_close(TcpStream& s);

// Queue one frame; false when it is too big, the stream is closed or its backlog is full.
bool tcp_send_frame(TcpStream& s, const uint8_t* data, size_t size);
// Move bytes: what the OS takes of the frames queued, what it has into the receive buffer. A
// stream the other side closed, or the OS failed, is marked `closed`.
void tcp_pump(TcpStream& s);
// The next whole frame received: its size, or -1 when none is whole yet; a frame larger than
// `capacity` is skipped.
int tcp_next_frame(TcpStream& s, uint8_t* buffer, size_t capacity);

}  // namespace ge::net
