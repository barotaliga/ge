// STUN's Binding request and answer (net/stun.hpp).
//
// ge:summary STUN binding messages written and read: request, answer, XOR-MAPPED-ADDRESS
// ge:keywords net stun binding xor-mapped-address public-address
// ge:skill net
// ge:test tests/net_stun_test.cpp
#include "net/stun.hpp"

#include <cstring>

namespace ge::net {

namespace {

constexpr uint16_t kBindingRequest = 0x0001, kBindingSuccess = 0x0101;
constexpr uint16_t kMappedAddress = 0x0001, kXorMappedAddress = 0x0020;

void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}
void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = static_cast<uint8_t>(v >> (24 - 8 * i));
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
         static_cast<uint32_t>(p[2]) << 8 | p[3];
}

void header(uint8_t* out, uint16_t type, uint16_t length, const uint8_t id[12]) {
  put16(out, type);
  put16(out + 2, length);
  put32(out + 4, kStunCookie);
  std::memcpy(out + 8, id, 12);
}

}  // namespace

size_t stun_request(uint8_t out[kStunRequestSize], const uint8_t id[12]) {
  header(out, kBindingRequest, 0, id);
  return kStunRequestSize;
}

bool is_stun(const uint8_t* data, size_t size) {
  return size >= 20 && (data[0] & 0xc0) == 0 && get32(data + 4) == kStunCookie &&
         get16(data + 2) + size_t{20} == size && (get16(data + 2) & 3) == 0;
}

size_t stun_answer(const uint8_t* request, size_t size, const Address& from, uint8_t* out,
                   size_t capacity) {
  if (!is_stun(request, size) || get16(request) != kBindingRequest) return 0;
  if (from.family != Family::ipv4 && from.family != Family::ipv6 && from.family != Family::sim)
    return 0;
  const uint8_t* id = request + 8;
  const bool v6 = from.family == Family::ipv6;
  const size_t value = v6 ? 20 : 8;
  if (capacity < 20 + 4 + value) return 0;
  header(out, kBindingSuccess, static_cast<uint16_t>(4 + value), id);
  uint8_t* a = out + 20;
  put16(a, kXorMappedAddress);
  put16(a + 2, static_cast<uint16_t>(value));
  a[4] = 0;
  a[5] = v6 ? 0x02 : 0x01;
  put16(a + 6, static_cast<uint16_t>(from.port ^ (kStunCookie >> 16)));
  uint8_t ip[16] = {};
  if (from.family == Family::sim) {
    sim_to_ipv4(from, ip);
  } else {
    std::memcpy(ip, from.bytes, v6 ? 16 : 4);
  }
  // The address XORed with the cookie, and for IPv6 the cookie then the transaction id.
  uint8_t mask[16];
  put32(mask, kStunCookie);
  std::memcpy(mask + 4, id, 12);
  for (size_t i = 0; i < (v6 ? 16u : 4u); i++) a[8 + i] = ip[i] ^ mask[i];
  return 20 + 4 + value;
}

bool stun_read_answer(const uint8_t* data, size_t size, const uint8_t id[12], Address& mapped) {
  if (!is_stun(data, size) || get16(data) != kBindingSuccess || std::memcmp(data + 8, id, 12) != 0)
    return false;
  uint8_t mask[16];
  put32(mask, kStunCookie);
  std::memcpy(mask + 4, id, 12);
  bool found = false;
  for (size_t at = 20; at + 4 <= size;) {
    const uint16_t type = get16(data + at);
    const size_t length = get16(data + at + 2);
    const uint8_t* v = data + at + 4;
    if (at + 4 + length > size) return false;
    const bool x = type == kXorMappedAddress;
    if ((x || type == kMappedAddress) && length >= 8 && !(found && !x)) {
      const bool v6 = v[1] == 0x02;
      if ((v6 && length < 20) || (!v6 && v[1] != 0x01)) return false;
      uint16_t port = get16(v + 2);
      uint8_t ip[16];
      for (size_t i = 0; i < (v6 ? 16u : 4u); i++) ip[i] = x ? v[4 + i] ^ mask[i] : v[4 + i];
      if (x) port = static_cast<uint16_t>(port ^ (kStunCookie >> 16));
      if (v6) {
        mapped = Address{};
        mapped.family = Family::ipv6;
        std::memcpy(mapped.bytes, ip, 16);
        mapped.port = port;
      } else {
        mapped = ipv4_or_sim(ip, port);
      }
      found = true;
    }
    at += 4 + ((length + 3) & ~size_t{3});
  }
  return found;
}

}  // namespace ge::net
