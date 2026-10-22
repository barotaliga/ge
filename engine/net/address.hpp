// AN ADDRESS a datagram is sent to: an IPv4 or IPv6 host and port, or a node of the simulated
// network (sim_network.hpp), so every layer above the transport names a peer the same way
// whether the packets cross a real socket or the lab's virtual one. A plain value: compare,
// hash and print it; parse one typed by a person or a flag.
//
// ge:summary A peer's address: IPv4, IPv6 or a simulated node, with its port; parse and print
// ge:keywords net network address ipv4 ipv6 port endpoint peer parse format sim-address
// ge:skill net
// ge:test tests/net_address_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ge::net {

enum class Family : uint8_t { none, ipv4, ipv6, sim };

struct Address {
  Family family = Family::none;
  // IPv4: bytes[0..3] in network order; IPv6: all 16; sim: the node id, little endian, in [0..3].
  uint8_t bytes[16] = {};
  uint16_t port = 0;
};

bool operator==(const Address& a, const Address& b);
inline bool operator!=(const Address& a, const Address& b) { return !(a == b); }

Address ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port);
// 127.0.0.1:port and [::1]:port.
Address loopback_v4(uint16_t port);
// A node of the simulated network: `node` is its index there, `port` its socket on it.
Address sim_address(uint32_t node, uint16_t port);
uint32_t sim_node(const Address& a);

// "1.2.3.4:5", "[::1]:5", "sim:3:5"; false when the text is none of them.
bool parse_address(std::string_view text, Address& out);
// The same forms back; "none" for an empty address.
std::string to_string(const Address& a);
// FNV-1a over the family, the bytes the family uses and the port: a key for a host's tables.
uint64_t address_hash(const Address& a);

}  // namespace ge::net
