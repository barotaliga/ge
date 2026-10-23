// Thinking on a schedule (engine/ai/think.hpp): every N steps each bot thinks once, on its own
// step of the N, so a crowd's thinking spreads over the steps; every step at 1 or less; and the
// steps between thoughts rise from the near count to the far one with the distance.
//
// ge:summary Tests: thinks() spreads a crowd over the steps; every_at by the distance
// ge:keywords test ai think schedule stagger every_at distance
// ge:skill bots testing
#include <cstdint>
#include <vector>

#include "ai/think.hpp"
#include "check.hpp"

namespace ai = ge::ai;

TEST(every_n_steps_each_bot_thinks_once_and_a_crowd_spreads_over_the_steps) {
  constexpr int64_t kEvery = 4;
  std::vector<int> per_step(kEvery, 0);
  for (int64_t bot = 0; bot < 8; bot++) {
    int thoughts = 0;
    for (int64_t step = 0; step < kEvery; step++) {
      if (ai::thinks(step, bot, kEvery)) {
        thoughts++;
        per_step[static_cast<size_t>(step)]++;
      }
    }
    CHECK(thoughts == 1);
  }
  for (const int n : per_step) CHECK(n == 2);
  for (int64_t step = 0; step < 5; step++) CHECK(ai::thinks(step, 3, 1) && ai::thinks(step, 3, 0));
}

TEST(the_steps_between_thoughts_rise_with_the_distance_from_the_player) {
  constexpr ai::Schedule s{2, 12, 50, 250};
  CHECK(ai::every_at(s, 0) == 2 && ai::every_at(s, 50) == 2);
  CHECK(ai::every_at(s, 150) == 7);
  CHECK(ai::every_at(s, 250) == 12 && ai::every_at(s, 1e6) == 12);
  int64_t last = 0;
  for (int i = 0; i < 30; i++) {
    const double d = 10.0 * i;
    CHECK(ai::every_at(s, d) >= last);
    last = ai::every_at(s, d);
  }
  CHECK(ai::thinks(10, 0, s, 0) && !ai::thinks(11, 0, s, 0));
}
