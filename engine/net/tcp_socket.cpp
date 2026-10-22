// The TCP sockets over each OS's API (os_socket.hpp), and the frames on them.
//
// ge:summary TCP listen, accept, connect and the length-framed byte pump over BSD and Winsock
// ge:keywords net tcp socket listen accept connect frame pump send recv select so-error
// ge:skill net
// ge:test tests/net_tcp_socket_test.cpp
#include "net/tcp_socket.hpp"

#include "net/os_socket.hpp"

namespace ge::net {

using namespace os;

namespace {

// A stream socket's options: non-blocking, no Nagle delay (a packet goes when it is sent), and on
// Apple's systems no SIGPIPE from a write to a closed stream.
bool prepare(Handle h) {
  int one = 1;
  setsockopt(h, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
#if defined(SO_NOSIGPIPE)
  setsockopt(h, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&one), sizeof one);
#endif
  return set_nonblocking(h);
}

// Whether a connect in flight has finished: true once it is up or failed (`ok` says which).
bool connect_done(Handle h, bool& ok) {
  fd_set w, e;
  FD_ZERO(&w);
  FD_ZERO(&e);
  FD_SET(h, &w);
  FD_SET(h, &e);
  timeval zero{0, 0};
  const int n = ::select(static_cast<int>(h) + 1, nullptr, &w, &e, &zero);
  if (n <= 0) return false;
  int err = 0;
  SockLen len = sizeof err;
  getsockopt(h, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
  ok = err == 0 && !FD_ISSET(h, &e);
  return true;
}

void compact(std::vector<uint8_t>& b, size_t& head) {
  if (head == 0) return;
  b.erase(b.begin(), b.begin() + static_cast<std::ptrdiff_t>(head));
  head = 0;
}

}  // namespace

bool tcp_listen(TcpListener& l, const Address& bind, std::string* error) {
  tcp_close_listener(l);
  sockaddr_storage sa;
  SockLen len = 0;
  if (!startup()) {
    if (error) *error = "WSAStartup failed: Winsock 2.2 is not available";
    return false;
  }
  if (!to_sockaddr(bind, sa, len)) {
    if (error) *error = "tcp_listen: " + to_string(bind) + " is not an IPv4 or IPv6 address";
    return false;
  }
  const Handle h = ::socket(sa.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (!valid(h)) {
    if (error) *error = os_error("socket()", last_error());
    return false;
  }
  int one = 1;  // a restarted host takes its port back at once
  setsockopt(h, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof one);
  v6_only(h, sa);
  if (!set_nonblocking(h) || ::bind(h, reinterpret_cast<const sockaddr*>(&sa), len) != 0 ||
      ::listen(h, 64) != 0) {
    if (error) *error = os_error("bind() and listen()", last_error()) + " at " + to_string(bind);
    close_handle(static_cast<intptr_t>(h));
    return false;
  }
  l.handle = static_cast<intptr_t>(h);
  l.local = local_of(h);
  return true;
}

void tcp_close_listener(TcpListener& l) {
  if (l.handle != -1) close_handle(l.handle);
  l = TcpListener{};
}

bool tcp_accept(TcpListener& l, TcpStream& out) {
  if (l.handle == -1) return false;
  sockaddr_storage sa;
  SockLen len = sizeof sa;
  const Handle h = ::accept(handle_of(l.handle), reinterpret_cast<sockaddr*>(&sa), &len);
  if (!valid(h)) return false;
  tcp_close(out);
  if (!prepare(h)) {
    close_handle(static_cast<intptr_t>(h));
    return false;
  }
  out.handle = static_cast<intptr_t>(h);
  out.local = local_of(h);
  out.peer = from_sockaddr(sa);
  return true;
}

bool tcp_connect(TcpStream& s, const Address& to, std::string* error) {
  tcp_close(s);
  sockaddr_storage sa;
  SockLen len = 0;
  if (!startup()) {
    if (error) *error = "WSAStartup failed: Winsock 2.2 is not available";
    return false;
  }
  if (!to_sockaddr(to, sa, len)) {
    if (error) *error = "tcp_connect: " + to_string(to) + " is not an IPv4 or IPv6 address";
    return false;
  }
  const Handle h = ::socket(sa.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (!valid(h)) {
    if (error) *error = os_error("socket()", last_error());
    return false;
  }
  if (!prepare(h)) {
    if (error) *error = os_error("set non-blocking", last_error());
    close_handle(static_cast<intptr_t>(h));
    return false;
  }
  const int r = ::connect(h, reinterpret_cast<const sockaddr*>(&sa), len);
  if (r != 0 && !in_progress(last_error())) {
    if (error) *error = os_error("connect()", last_error()) + " to " + to_string(to);
    close_handle(static_cast<intptr_t>(h));
    return false;
  }
  s.handle = static_cast<intptr_t>(h);
  s.peer = to;
  s.connecting = r != 0;
  s.local = local_of(h);
  return true;
}

void tcp_close(TcpStream& s) {
  if (s.handle != -1) close_handle(s.handle);
  s = TcpStream{};
}

bool tcp_send_frame(TcpStream& s, const uint8_t* data, size_t size) {
  if (s.handle == -1 || s.closed || size > kMaxFrame) return false;
  if (s.out.size() - s.out_head + 2 + size > kStreamBacklog) return false;
  s.out.push_back(static_cast<uint8_t>(size & 0xff));
  s.out.push_back(static_cast<uint8_t>(size >> 8));
  s.out.insert(s.out.end(), data, data + size);
  tcp_pump(s);
  return true;
}

void tcp_pump(TcpStream& s) {
  if (s.handle == -1 || s.closed) return;
  const Handle h = handle_of(s.handle);
  if (s.connecting) {
    bool ok = false;
    if (!connect_done(h, ok)) return;
    s.connecting = false;
    if (!ok) {
      s.closed = true;
      return;
    }
    s.local = local_of(h);
  }
  while (s.out_head < s.out.size()) {
    const size_t want = s.out.size() - s.out_head;
    const auto n = ::send(h, reinterpret_cast<const char*>(s.out.data() + s.out_head),
                          static_cast<int>(want), kNoSignal);
    if (n < 0) {
      if (!would_block(last_error())) s.closed = true;
      break;
    }
    s.out_head += static_cast<size_t>(n);
  }
  if (s.out_head == s.out.size()) {
    s.out.clear();
    s.out_head = 0;
  } else if (s.out_head > kStreamBacklog / 2) {
    compact(s.out, s.out_head);
  }
  uint8_t chunk[16384];
  while (true) {
    const auto n = ::recv(h, reinterpret_cast<char*>(chunk), static_cast<int>(sizeof chunk), 0);
    if (n == 0) {
      s.closed = true;
      break;
    }
    if (n < 0) {
      if (!would_block(last_error())) s.closed = true;
      break;
    }
    s.in.insert(s.in.end(), chunk, chunk + n);
  }
}

int tcp_next_frame(TcpStream& s, uint8_t* buffer, size_t capacity) {
  while (s.in.size() - s.in_head >= 2) {
    const size_t n = s.in[s.in_head] | static_cast<size_t>(s.in[s.in_head + 1]) << 8;
    if (s.in.size() - s.in_head < 2 + n) break;
    const uint8_t* at = s.in.data() + s.in_head + 2;
    s.in_head += 2 + n;
    if (n > capacity) continue;
    if (n) std::memcpy(buffer, at, n);
    if (s.in_head == s.in.size()) {
      s.in.clear();
      s.in_head = 0;
    }
    return static_cast<int>(n);
  }
  if (s.in_head > 0 && s.in_head == s.in.size()) {
    s.in.clear();
    s.in_head = 0;
  } else if (s.in_head > 65536) {
    compact(s.in, s.in_head);
  }
  return -1;
}

}  // namespace ge::net
