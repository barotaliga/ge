// A classic server's challenges (challenges.hpp).
//
// ge:summary A classic server's challenges checked by host and number, and used up
// ge:keywords challenge_check challenge_used
// ge:skill classic net
// ge:test tests/classic_challenges_test.cpp
#include "classic/protocol/challenges.hpp"

#include <algorithm>

namespace ge::classic {

ChallengeCheck challenge_check(const ChallengeBook& b, const net::Address& from, int32_t value) {
  bool host = false;
  for (const ChallengeBook::Entry& e : b.entries) {
    if (!net::same_host(e.address, from)) continue;
    if (e.value == value) return ChallengeCheck::ok;
    host = true;
  }
  return host ? ChallengeCheck::bad : ChallengeCheck::none;
}

void challenge_used(ChallengeBook& b, const net::Address& from, int32_t value) {
  std::erase_if(b.entries, [&](const ChallengeBook::Entry& e) {
    return net::same_host(e.address, from) && e.value == value;
  });
}

}  // namespace ge::classic
