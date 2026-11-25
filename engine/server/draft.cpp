// Picking teams in turn, defined: the rules in draft.hpp's header.
//
// ge:summary Two captains pick the players in turn by number: the order, the last one given
// ge:keywords draft pick-order draft_pick draft_start draft_join draft_leave
// ge:skill net
// ge:test tests/server_draft_test.cpp
#include "server/draft.hpp"

namespace ge::server {

namespace {

bool in_range(int slot) { return slot >= 0 && slot < kDraftSeats; }

void stop(Draft& d) {
  d.active = false;
  d.number.fill(0);
}

}  // namespace

void draft_start(Draft& d, int captain_a, int captain_b, const std::vector<int>& pool,
                 uint64_t seed) {
  d = {};
  d.active = true;
  d.captain = {captain_a, captain_b};
  // One in two from the seed's mixed bits (a splitmix step), so neighbouring seeds differ.
  uint64_t z = seed + 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  d.first = static_cast<int>((z ^ (z >> 31)) & 1u);
  int n = 1;
  for (int slot = 0; slot < kDraftSeats; slot++)
    for (int p : pool)
      if (p == slot && p != captain_a && p != captain_b)
        d.number[static_cast<size_t>(slot)] = static_cast<int8_t>(n++);
}

int draft_turn(const Draft& d) {
  if (!d.active) return -1;
  // A, B, B, A, B, A...: the first pick A's, the next two B's, then A on odd picks, B on even.
  const int i = d.picks;
  const bool a = i == 0 || (i >= 3 && i % 2 == 1);
  return a ? d.first : 1 - d.first;
}

int draft_left(const Draft& d) {
  int n = 0;
  for (int8_t v : d.number) n += v != 0;
  return n;
}

DraftRefusal draft_pick(Draft& d, int slot, int number, std::vector<DraftPick>& out) {
  if (!d.active || (slot != d.captain[0] && slot != d.captain[1])) return DraftRefusal::not_captain;
  const int turn = draft_turn(d);
  if (slot != d.captain[static_cast<size_t>(turn)]) return DraftRefusal::not_your_turn;
  int chosen = -1;
  for (int s = 0; s < kDraftSeats && number > 0; s++)
    if (d.number[static_cast<size_t>(s)] == number) chosen = s;
  if (chosen < 0) return DraftRefusal::no_such_player;
  d.number[static_cast<size_t>(chosen)] = 0;
  d.picks++;
  out.push_back({chosen, turn});
  if (draft_left(d) == 1) {
    for (int s = 0; s < kDraftSeats; s++)
      if (d.number[static_cast<size_t>(s)] != 0) {
        out.push_back({s, draft_turn(d)});
        d.number[static_cast<size_t>(s)] = 0;
        d.picks++;
      }
  }
  if (draft_left(d) == 0) stop(d);
  return DraftRefusal::none;
}

int draft_join(Draft& d, int slot) {
  if (!d.active || !in_range(slot) || slot == d.captain[0] || slot == d.captain[1]) return 0;
  int8_t& mine = d.number[static_cast<size_t>(slot)];
  if (mine != 0) return mine;
  for (int n = 1; n <= kDraftSeats; n++) {
    bool taken = false;
    for (int8_t v : d.number) taken = taken || v == n;
    if (!taken) {
      mine = static_cast<int8_t>(n);
      return n;
    }
  }
  return 0;
}

void draft_leave(Draft& d, int slot) {
  if (!d.active || !in_range(slot)) return;
  if (slot == d.captain[0] || slot == d.captain[1]) {
    stop(d);
    d.aborted = true;
    return;
  }
  d.number[static_cast<size_t>(slot)] = 0;
  if (draft_left(d) == 0) stop(d);
}

}  // namespace ge::server
