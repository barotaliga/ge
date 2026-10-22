// The transport's two backends behind one switch, and the capture hook.
//
// ge:summary The transport's UDP and simulated backends and its capture hook
// ge:keywords net transport backend switch capture
// ge:skill net
// ge:test tests/net_node_test.cpp
#include "net/transport.hpp"

namespace ge::net {

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
    case TransportKind::none:
      break;
  }
  if (n >= 0 && t.capture) {
    capture_datagram(*t.capture, now_us, from, t.local, buffer, static_cast<size_t>(n));
  }
  return n;
}

}  // namespace ge::net
