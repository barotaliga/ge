// Tests: picking teams in turn (engine/server/draft.hpp). The numbers in slot order; the order A,
// B, B, A, B, A; the wrong captain, a pick out of turn and a number not in the pool refused; the
// last player given at once; a late joiner's lowest free number; a captain who leaves ends it.
//
// ge:summary Tests: the draft's numbers, its order A, B, B, A, the last one given, the abort
// ge:keywords test draft pick-order captain-picks
// ge:skill net testing
#include <vector>

#include "check.hpp"
#include "server/draft.hpp"

namespace sv = ge::server;

TEST(draft_numbers_the_pool_and_picks_a_b_b_a) {
  sv::Draft d;
  sv::draft_start(d, 0, 1, {2, 3, 4, 5, 6, 7}, 7);
  CHECK(d.active);
  CHECK(d.number[2] == 1 && d.number[7] == 6 && d.number[0] == 0 && d.number[1] == 0);
  const int a = d.first, b = 1 - d.first;
  std::vector<sv::DraftPick> out;
  CHECK(sv::draft_pick(d, 9, 1, out) == sv::DraftRefusal::not_captain);
  CHECK(sv::draft_pick(d, d.captain[static_cast<size_t>(b)], 1, out) ==
        sv::DraftRefusal::not_your_turn);
  const int ca = d.captain[static_cast<size_t>(a)], cb = d.captain[static_cast<size_t>(b)];
  CHECK(sv::draft_pick(d, ca, 9, out) == sv::DraftRefusal::no_such_player);
  CHECK(sv::draft_pick(d, ca, 1, out) == sv::DraftRefusal::none);
  CHECK(out.size() == 1 && out[0].slot == 2 && out[0].captain == a);
  CHECK(sv::draft_pick(d, ca, 2, out) == sv::DraftRefusal::not_your_turn);
  CHECK(sv::draft_pick(d, cb, 1, out) == sv::DraftRefusal::no_such_player);  // taken
  CHECK(sv::draft_pick(d, cb, 2, out) == sv::DraftRefusal::none);
  CHECK(sv::draft_pick(d, cb, 3, out) == sv::DraftRefusal::none);
  CHECK(sv::draft_pick(d, ca, 4, out) == sv::DraftRefusal::none);
  CHECK(sv::draft_pick(d, cb, 5, out) == sv::DraftRefusal::none);
  // One left (number 6): given to the captain whose turn it is (A's) at once, and it is over.
  CHECK(out.size() == 6);
  CHECK(out[5].slot == 7 && out[5].captain == a);
  CHECK(!d.active);
}

TEST(draft_either_captain_may_pick_first_by_the_seed) {
  bool seen[2] = {false, false};
  for (uint64_t seed = 0; seed < 16; seed++) {
    sv::Draft d;
    sv::draft_start(d, 0, 1, {2, 3, 4}, seed);
    seen[d.first] = true;
  }
  CHECK(seen[0] && seen[1]);
}

TEST(draft_a_late_joiner_and_a_captain_who_leaves) {
  sv::Draft d;
  sv::draft_start(d, 0, 1, {2, 3, 4}, 1);
  sv::draft_leave(d, 3);
  CHECK(d.number[3] == 0);
  CHECK(sv::draft_join(d, 6) == 2);
  CHECK(sv::draft_join(d, 6) == 2);
  CHECK(sv::draft_join(d, 0) == 0);
  CHECK(sv::draft_left(d) == 3);
  sv::draft_leave(d, 1);
  CHECK(!d.active);
  CHECK(d.aborted);
  CHECK(sv::draft_left(d) == 0);
}
