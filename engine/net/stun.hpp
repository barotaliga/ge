// STUN (RFC 8489's Binding method, the part a game needs): ask a server which address and port a
// datagram from this socket arrives from, the public face a NAT gave it. A request is 20 bytes
// (the type, a zero length, the magic cookie, a 96-bit transaction id); the answer carries
// XOR-MAPPED-ADDRESS (the address XORed with the cookie, so routers that rewrite addresses in
// payloads leave it alone), or MAPPED-ADDRESS from an old server. Big endian, as the RFC says.
//
// The engine's rendezvous answers STUN on its own port (rendezvous.hpp), and any public STUN
// server answers the same request. A simulated node is written as IPv4 in 198.18.0.0/15
// (address.hpp's sim_to_ipv4) and read back as itself.
//
// ge:summary STUN binding: the request, a server's answer, the public address read back
// ge:keywords net stun binding public-address mapped-address xor-mapped-address nat reflexive
// ge:keywords transaction cookie rfc8489 rfc5389
// ge:skill net
// ge:test tests/net_stun_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "net/address.hpp"

namespace ge::net {

constexpr uint32_t kStunCookie = 0x2112A442u;
constexpr size_t kStunRequestSize = 20;
constexpr size_t kStunAnswerMax = 44;  // the header and an IPv6 XOR-MAPPED-ADDRESS

// A Binding request with transaction id `id` into `out`; its size.
size_t stun_request(uint8_t out[kStunRequestSize], const uint8_t id[12]);
// The datagram is a STUN message (the cookie where it belongs, the length in step).
bool is_stun(const uint8_t* data, size_t size);
// SERVER: the success answer to a Binding request, telling `from` how it was seen; 0 when the
// datagram is not a Binding request or `out` is too small.
size_t stun_answer(const uint8_t* request, size_t size, const Address& from, uint8_t* out,
                   size_t capacity);
// CLIENT: read a success answer to transaction `id`: the address it names.
bool stun_read_answer(const uint8_t* data, size_t size, const uint8_t id[12], Address& mapped);

}  // namespace ge::net
