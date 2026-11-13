// A fixed route (fixed.hpp): every unknown client admitted toward one destination, its first
// answers held to a budget until it has spoken enough.
//
// ge:summary A fixed route's face: unknown clients admitted to one server, answers held to a budget
// ge:keywords forward fixed-route port-forward stock-client amplification budget
// ge:skill net
// ge:test tests/forward_test.cpp
#include "forward/fixed.hpp"

#include <string>

#include "net/transport.hpp"

namespace ge::forward {

namespace {

void up(HopClient& c, const uint8_t* data, size_t size, int64_t now_us) {
  net::transport_send(*c.up, c.to, data, size, now_us);
  c.packets_up++;
  c.bytes_up += size;
  c.last_us = now_us;
}

bool from_client(Forward& f, FixedFace& face, const uint8_t* data, size_t size,
                 const net::Address& from, int64_t now_us) {
  HopClient* c = forward_client(f, from);
  if (c == nullptr) {
    Answer refused = Answer::ok;
    std::string why;
    c = forward_admit(f, from, face.to, now_us, refused, why);
    if (c == nullptr) {
      face.refused++;
      return true;
    }
    face.clients++;
  }
  up(*c, data, size, now_us);
  if (!c->connected && c->packets_up >= static_cast<uint64_t>(face.proven_after)) {
    c->connected = true;  // the forward copies both ways from now on
    face.proven++;
  }
  return true;
}

void from_upstream(Forward& f, FixedFace& face, HopClient& c, const uint8_t* data, size_t size,
                   int64_t now_us) {
  if (c.bytes_down + size > c.bytes_up * face.budget) {
    face.held_back++;
    return;
  }
  forward_send(f, c.from, data, size, now_us);
  c.packets_down++;
  c.bytes_down += size;
  c.last_us = now_us;
}

}  // namespace

void fixed_route_attach(Forward& f, FixedFace& face) {
  FixedFace* p = &face;
  f.face = ForwardFace{};
  f.face.from_client = [p](Forward& fo, const uint8_t* d, size_t n, const net::Address& from,
                           int64_t now) { return from_client(fo, *p, d, n, from, now); };
  f.face.from_upstream = [p](Forward& fo, HopClient& c, const uint8_t* d, size_t n, int64_t now) {
    from_upstream(fo, *p, c, d, n, now);
  };
}

}  // namespace ge::forward
