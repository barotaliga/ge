// A CLASSIC SERVER'S CHALLENGES: the number a client asks for (`getchallenge`) and brings back in
// its `connect`, so a connect from a forged address is refused. Every classic face keeps them the
// same way: QuakeWorld's server and Quake II's, and our TV node's faces for both.
//
// ONE AN ADDRESS, ITS PORT TOO: clients sharing a host (players behind one router, bots from one
// machine) do not replace each other's before they connect. An address that asks again gets the
// number it was given: a client whose first answer came late and asked again still connects with
// the first. FOUND BY THE HOST AND THE NUMBER: a connect whose port moved between the two asks (a
// router that picked another) still finds its challenge. BOUNDED: `most` kept, the oldest given
// out replaced, so a flood of asks from forged addresses costs a fixed table, never memory.
//
// The number itself is each game's own (its width and its stream are on the wire); `fresh` makes
// it.
//
// ge:summary A classic server's challenges: one an address, found by host and number, bounded
// ge:keywords challenge getchallenge connect challenge-book no-challenge bad-challenge nat
// ge:keywords port-moved same-host flood bounded
// ge:skill classic net
// ge:test tests/classic_challenges_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/address.hpp"

namespace ge::classic {

struct ChallengeBook {
  struct Entry {
    net::Address address;
    int32_t value = 0;
    int64_t made = 0;  // the order it was given out in (1 the first)
  };
  std::vector<Entry> entries;
  size_t most = 1024;  // kept at once; the oldest replaced past it
  int64_t made = 0;
};

enum class ChallengeCheck {
  ok,    // the host's challenge with that number
  none,  // the host was given none ("No challenge for address.")
  bad,   // the host has challenges, none with that number ("Bad challenge.")
};

// The challenge `from` was given, or a new one from `fresh()` (called only then).
template <class Fresh>
int32_t challenge_issue(ChallengeBook& b, const net::Address& from, Fresh&& fresh) {
  size_t oldest = 0;
  for (size_t i = 0; i < b.entries.size(); i++) {
    if (b.entries[i].address == from) return b.entries[i].value;
    if (b.entries[i].made < b.entries[oldest].made) oldest = i;
  }
  ChallengeBook::Entry e{from, static_cast<int32_t>(fresh()), ++b.made};
  if (b.entries.size() < b.most)
    b.entries.push_back(e);
  else if (!b.entries.empty())
    b.entries[oldest] = e;
  return e.value;
}

// A connect's challenge checked: by the host, then the number.
ChallengeCheck challenge_check(const ChallengeBook& b, const net::Address& from, int32_t value);
// The host's challenge with `value` taken out (a server whose challenges are used once).
void challenge_used(ChallengeBook& b, const net::Address& from, int32_t value);

}  // namespace ge::classic
