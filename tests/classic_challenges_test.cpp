// plugins/classic/protocol/challenges.hpp: a classic server's challenges. One an address, the
// same one asked again, found by the host and the number when the port moved, refused as none or
// bad in the servers' two words, used up once, and bounded under a flood.
//
// ge:summary Test: the classic challenges: one an address, port moved, none or bad, used, bounded
// ge:keywords test challenge getchallenge port-moved flood bounded
// ge:skill classic net testing
#include "classic/protocol/challenges.hpp"

#include <cstdint>

#include "check.hpp"

namespace {

namespace cl = ge::classic;
namespace gn = ge::net;

int32_t counter = 100;
int32_t next() { return ++counter; }

TEST(one_challenge_an_address_and_the_same_one_asked_again) {
  cl::ChallengeBook b;
  const gn::Address a = gn::ipv4(10, 0, 0, 7, 27001), c = gn::ipv4(10, 0, 0, 7, 27002);
  const int32_t first = cl::challenge_issue(b, a, next);
  CHECK(cl::challenge_issue(b, a, next) == first);        // a late answer: asked again, the same
  const int32_t other = cl::challenge_issue(b, c, next);  // a second player behind one router
  CHECK(other != first && b.entries.size() == 2);
  CHECK(cl::challenge_check(b, a, first) == cl::ChallengeCheck::ok);
  CHECK(cl::challenge_check(b, c, other) == cl::ChallengeCheck::ok);
}

TEST(a_port_that_moved_finds_its_challenge_by_host_and_number) {
  cl::ChallengeBook b;
  const int32_t n = cl::challenge_issue(b, gn::ipv4(10, 0, 0, 7, 27001), next);
  CHECK(cl::challenge_check(b, gn::ipv4(10, 0, 0, 7, 40000), n) == cl::ChallengeCheck::ok);
  CHECK(cl::challenge_check(b, gn::ipv4(10, 0, 0, 7, 40000), n + 1) == cl::ChallengeCheck::bad);
  CHECK(cl::challenge_check(b, gn::ipv4(10, 0, 0, 8, 27001), n) == cl::ChallengeCheck::none);
}

TEST(a_used_challenge_is_gone) {
  cl::ChallengeBook b;
  const gn::Address a = gn::ipv4(10, 0, 0, 7, 27001);
  const int32_t n = cl::challenge_issue(b, a, next);
  cl::challenge_used(b, a, n);
  CHECK(cl::challenge_check(b, a, n) == cl::ChallengeCheck::none);
}

TEST(a_flood_of_asks_costs_a_fixed_table) {
  cl::ChallengeBook b;
  b.most = 64;
  const gn::Address first = gn::ipv4(10, 0, 0, 1, 1);
  const int32_t n = cl::challenge_issue(b, first, next);
  for (int i = 0; i < 1000; i++)
    cl::challenge_issue(
        b, gn::ipv4(192, 168, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i), 5000), next);
  CHECK(b.entries.size() == 64);
  CHECK(cl::challenge_check(b, first, n) == cl::ChallengeCheck::none);  // the oldest replaced
}

}  // namespace
