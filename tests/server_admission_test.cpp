// engine/server/admission: sv_iplimit counts the seats one IPv4 host or one IPv6 /48 holds (a
// seat being freed a half), never loopback or link-local IPv6, and refuses past the limit.
//
// ge:summary Test: sv_iplimit per IPv4 host and IPv6 /48, freed seats a half, exempt addresses
// ge:keywords test sv_iplimit admission admit_refusal ipv6-48 loopback link-local
// ge:skill net testing
#include <vector>

#include "check.hpp"
#include "server/admission.hpp"

using namespace ge::server;
namespace gn = ge::net;

namespace {

gn::Address at(const char* text) {
  gn::Address a;
  CHECK_MSG(gn::parse_address(text, a), text);
  return a;
}

}  // namespace

TEST(an_ipv4_host_is_refused_once_it_holds_the_limit) {
  std::vector<HeldSeat> seats{{at("198.51.100.7:27901")}, {at("198.51.100.7:27902")}};
  const gn::Address next = at("198.51.100.7:27903");
  CHECK(admit_refusal(3, next, seats).empty());
  seats.push_back({at("198.51.100.7:27904")});
  CHECK(admit_refusal(3, next, seats) == "Too many connections from your IP address.");
  CHECK(admit_refusal(0, next, seats).empty());                      // no limit
  CHECK(admit_refusal(3, at("198.51.100.8:27901"), seats).empty());  // another host
}

TEST(a_seat_being_freed_counts_a_half) {
  const std::vector<HeldSeat> seats{
      {at("198.51.100.7:1")}, {at("198.51.100.7:2")}, {at("198.51.100.7:3"), true}};
  CHECK(admit_refusal(3, at("198.51.100.7:4"), seats).empty());  // 2.5 of 3
  const std::vector<HeldSeat> two_freeing{{at("198.51.100.7:1")},
                                          {at("198.51.100.7:2")},
                                          {at("198.51.100.7:3"), true},
                                          {at("198.51.100.7:5"), true}};
  CHECK(!admit_refusal(3, at("198.51.100.7:4"), two_freeing).empty());  // 3 of 3
}

TEST(ipv6_counts_by_its_48_network_and_exempts_loopback_and_link_local) {
  const std::vector<HeldSeat> seats{{at("[2001:db8:1:aa::1]:1")}, {at("[2001:db8:1:bb::2]:2")}};
  CHECK(admit_refusal(2, at("[2001:db8:1:cc::3]:3"), seats) ==
        "Too many connections from your IPv6 network.");
  CHECK(admit_refusal(2, at("[2001:db8:2::3]:3"), seats).empty());  // another /48
  const std::vector<HeldSeat> local{{at("127.0.0.1:1")}, {at("127.0.0.1:2")}};
  CHECK(admit_refusal(1, at("127.0.0.1:3"), local).empty());
  const std::vector<HeldSeat> link{{at("[fe80::1]:1")}};
  CHECK(admit_refusal(1, at("[fe80::2]:2"), link).empty());
}
