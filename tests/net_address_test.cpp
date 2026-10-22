// engine/net/address.hpp: parsing and printing every family, and refusing what is none of them.
//
// ge:summary Test: net addresses parsed, printed, compared and hashed
// ge:keywords test net address parse format ipv4 ipv6 sim
// ge:skill net testing
#include "net/address.hpp"

#include <string>

#include "check.hpp"

using namespace ge::net;

namespace {
std::string round_trip(const char* text) {
  Address a;
  if (!parse_address(text, a)) return "refused";
  return to_string(a);
}
}  // namespace

TEST(address_round_trips_each_family) {
  CHECK(round_trip("1.2.3.4:5") == "1.2.3.4:5");
  CHECK(round_trip("255.255.255.255:65535") == "255.255.255.255:65535");
  CHECK(round_trip("sim:7:40000") == "sim:7:40000");
  CHECK(round_trip("[::1]:9") == "[::1]:9");
  CHECK(round_trip("[::]:9") == "[::]:9");
  CHECK(round_trip("[2001:DB8:0:0:1:0:0:1]:80") == "[2001:db8::1:0:0:1]:80");
  CHECK(round_trip("[fe80::1:2]:1") == "[fe80::1:2]:1");
  CHECK(round_trip("[1:2:3:4:5:6:7:8]:1") == "[1:2:3:4:5:6:7:8]:1");
  CHECK(round_trip("[1:0:3:4:5:6:7:8]:1") == "[1:0:3:4:5:6:7:8]:1");
}

TEST(address_refuses_what_is_not_one) {
  for (const char* bad :
       {"", "1.2.3:4", "1.2.3.4", "256.1.1.1:2", "1.2.3.4:65536", "[::1]", "[1::2::3]:4",
        "[12345::]:1", "sim:1", "sim:x:2", "1.2.3.4:-1", "[1:2:3:4:5:6:7:8:9]:1"}) {
    Address a;
    CHECK_MSG(!parse_address(bad, a), std::string("parsed: ") + bad);
    CHECK(a.family == Family::none);
  }
}

TEST(address_compares_and_hashes_by_what_it_names) {
  CHECK(ipv4(10, 0, 0, 1, 7) == ipv4(10, 0, 0, 1, 7));
  CHECK(ipv4(10, 0, 0, 1, 7) != ipv4(10, 0, 0, 1, 8));
  CHECK(sim_address(3, 1) != ipv4(3, 0, 0, 0, 1));
  CHECK(address_hash(sim_address(3, 1)) == address_hash(sim_address(3, 1)));
  CHECK(address_hash(sim_address(3, 1)) != address_hash(sim_address(4, 1)));
  CHECK(sim_node(sim_address(70000, 2)) == 70000);
  CHECK(to_string(Address{}) == "none");
  CHECK(to_string(loopback_v4(80)) == "127.0.0.1:80");
}
