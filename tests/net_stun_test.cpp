// engine/net/stun.hpp: a Binding request answered with the address it came from, IPv4, IPv6 and a
// simulated node, read back; foreign and mismatched messages refused.
//
// ge:summary Test: STUN binding request and answer, the mapped address read back
// ge:keywords test net stun binding xor-mapped-address
// ge:skill net testing
#include "net/stun.hpp"

#include "check.hpp"

using namespace ge::net;

namespace {

bool round_trip(const Address& from, Address& mapped) {
  const uint8_t id[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  uint8_t req[kStunRequestSize];
  if (stun_request(req, id) != kStunRequestSize || !is_stun(req, sizeof req)) return false;
  uint8_t out[kStunAnswerMax];
  const size_t n = stun_answer(req, sizeof req, from, out, sizeof out);
  return n > 0 && is_stun(out, n) && stun_read_answer(out, n, id, mapped);
}

}  // namespace

TEST(stun_answers_the_address_a_request_came_from) {
  Address mapped;
  const Address v4 = ipv4(203, 0, 113, 7, 40001);
  CHECK(round_trip(v4, mapped) && mapped == v4);
  Address v6;
  CHECK(parse_address("[2001:db8::5]:3478", v6));
  CHECK(round_trip(v6, mapped) && mapped == v6);
  const Address sim = sim_address(1234, 40000);
  CHECK(round_trip(sim, mapped) && mapped == sim);
}

TEST(stun_answer_is_xored_and_matches_the_rfc_layout) {
  const uint8_t id[12] = {};
  uint8_t req[kStunRequestSize];
  stun_request(req, id);
  CHECK(req[0] == 0 && req[1] == 1 && req[2] == 0 && req[3] == 0);
  CHECK(req[4] == 0x21 && req[5] == 0x12 && req[6] == 0xa4 && req[7] == 0x42);
  uint8_t out[kStunAnswerMax];
  const size_t n = stun_answer(req, sizeof req, ipv4(192, 0, 2, 1, 32853), out, sizeof out);
  CHECK(n == 32 && out[0] == 0x01 && out[1] == 0x01 && out[3] == 12);
  CHECK(out[20] == 0x00 && out[21] == 0x20 && out[25] == 0x01);
  // The port XORed with the cookie's top half: 32853 ^ 0x2112 = 0xa147.
  CHECK(out[26] == 0xa1 && out[27] == 0x47);
  // 192.0.2.1 ^ 0x2112a442 = 0xe112a643.
  CHECK(out[28] == 0xe1 && out[29] == 0x12 && out[30] == 0xa6 && out[31] == 0x43);
}

TEST(stun_refuses_what_is_not_its_answer) {
  const uint8_t id[12] = {9}, other[12] = {8};
  uint8_t req[kStunRequestSize];
  stun_request(req, id);
  uint8_t out[kStunAnswerMax];
  Address mapped;
  const size_t n = stun_answer(req, sizeof req, ipv4(1, 2, 3, 4, 5), out, sizeof out);
  CHECK(!stun_read_answer(out, n, other, mapped));        // another transaction
  CHECK(!stun_read_answer(req, sizeof req, id, mapped));  // a request, not an answer
  CHECK(stun_answer(out, n, ipv4(1, 2, 3, 4, 5), out, sizeof out) == 0);   // answers no answer
  CHECK(stun_answer(req, sizeof req, ipv4(1, 2, 3, 4, 5), out, 20) == 0);  // too small
  const uint8_t junk[20] = {'g', 'e', 'R', 'V'};
  CHECK(!is_stun(junk, sizeof junk));
  CHECK(!is_stun(req, sizeof req - 1));
}
