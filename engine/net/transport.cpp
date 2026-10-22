// The transport's backends behind one switch, and the capture hook.
//
// ge:summary The transport's UDP, TCP and simulated backends and its capture hook
// ge:keywords net transport backend switch capture tcp stream accept
// ge:skill net
// ge:test tests/net_node_test.cpp
#include "net/transport.hpp"

#include <utility>

namespace ge::net {

namespace {

TcpStream* stream_to(Transport& t, const Address& to) {
  for (TcpStream& s : t.streams) {
    if (!s.closed && s.peer == to) return &s;
  }
  return nullptr;
}

// A TCP host takes every stream waiting and lets the closed ones go once they are read out.
void tend_streams(Transport& t) {
  if (t.listener.handle != -1) {
    TcpStream s;
    while (tcp_accept(t.listener, s)) {
      if (t.streams.size() < kMaxStreams) {
        t.streams.push_back(std::move(s));
        s = TcpStream{};
      } else {
        tcp_close(s);
      }
    }
  }
  for (size_t i = 0; i < t.streams.size();) {
    TcpStream& s = t.streams[i];
    if (s.closed && s.in_head == s.in.size()) {
      tcp_close(s);
      t.streams.erase(t.streams.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      i++;
    }
  }
}

int tcp_receive(Transport& t, uint8_t* buffer, size_t capacity, Address& from) {
  if (t.next_stream == 0) tend_streams(t);
  while (t.next_stream < t.streams.size()) {
    TcpStream& s = t.streams[t.next_stream];
    int n = tcp_next_frame(s, buffer, capacity);
    if (n < 0) {
      tcp_pump(s);
      n = tcp_next_frame(s, buffer, capacity);
    }
    if (n >= 0) {
      from = s.peer;
      return n;  // the same stream again next, until it has no whole frame
    }
    t.next_stream++;
  }
  t.next_stream = 0;  // every stream read out: the next receive starts a new round
  return -1;
}

}  // namespace

bool open_tcp_host_transport(Transport& t, const Address& bind, std::string* error) {
  close_transport(t);
  if (!tcp_listen(t.listener, bind, error)) return false;
  t.kind = TransportKind::tcp;
  t.local = t.listener.local;
  t.streams.reserve(kMaxStreams);
  return true;
}

bool open_tcp_client_transport(Transport& t, const Address& host, std::string* error) {
  close_transport(t);
  t.streams.resize(1);
  if (!tcp_connect(t.streams[0], host, error)) {
    t.streams.clear();
    return false;
  }
  t.kind = TransportKind::tcp;
  t.local = t.streams[0].local;
  return true;
}

bool open_sim_transport(Transport& t, SimNetwork& net, const Address& local) {
  close_transport(t);
  if (!sim_bind(net, local)) return false;
  t.kind = TransportKind::sim;
  t.sim = &net;
  t.local = local;
  return true;
}

bool open_udp_transport(Transport& t, const Address& bind, std::string* error) {
  close_transport(t);
  if (!udp_open(t.udp, bind, error)) return false;
  t.kind = TransportKind::udp;
  t.local = t.udp.local;
  return true;
}

void close_transport(Transport& t) {
  if (t.kind == TransportKind::udp) udp_close(t.udp);
  if (t.kind == TransportKind::sim && t.sim) sim_unbind(*t.sim, t.local);
  for (TcpStream& s : t.streams) tcp_close(s);
  t.streams.clear();
  t.next_stream = 0;
  tcp_close_listener(t.listener);
  t.kind = TransportKind::none;
  t.sim = nullptr;
  t.local = Address{};
}

bool transport_send(Transport& t, const Address& to, const uint8_t* data, size_t size,
                    int64_t now_us) {
  bool ok = false;
  switch (t.kind) {
    case TransportKind::udp:
      ok = udp_send(t.udp, to, data, size);
      break;
    case TransportKind::sim:
      ok = sim_send(*t.sim, t.local, to, data, size);
      break;
    case TransportKind::tcp:
      if (TcpStream* s = stream_to(t, to)) ok = tcp_send_frame(*s, data, size);
      break;
    case TransportKind::none:
      break;
  }
  if (!ok) {
    t.send_failures++;
    return false;
  }
  if (t.capture) capture_datagram(*t.capture, now_us, t.local, to, data, size);
  return true;
}

int transport_receive(Transport& t, uint8_t* buffer, size_t capacity, Address& from,
                      int64_t now_us) {
  int n = -1;
  switch (t.kind) {
    case TransportKind::udp:
      n = udp_receive(t.udp, buffer, capacity, from);
      break;
    case TransportKind::sim:
      n = sim_receive_bytes(*t.sim, t.local, buffer, capacity, from);
      break;
    case TransportKind::tcp:
      n = tcp_receive(t, buffer, capacity, from);
      break;
    case TransportKind::none:
      break;
  }
  if (n >= 0 && t.capture) {
    capture_datagram(*t.capture, now_us, from, t.local, buffer, static_cast<size_t>(n));
  }
  return n;
}

}  // namespace ge::net
