// Steering and choosing (engine/ai/steer.hpp, choose.hpp): pursuit keeps its mark ahead on the
// line and runs out at its end; the fan takes the cheapest heading and the one held on a tie; the
// grid visits every bot the full pass would, in the same order, so the push apart is the same to
// the bit; the stuck check counts the windows a bot barely moved; the selector takes the best and
// records every option; entering a mode records the move and starts its clock again.
//
// ge:summary Tests: pursuit, the fan, the grid against the full pass, the stuck check, choosing
// ge:keywords test ai steer pursuit fan neighbours separation grid stuck progress choose enter
// ge:skill bots testing
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "ai/choose.hpp"
#include "ai/steer.hpp"
#include "ai/why.hpp"
#include "check.hpp"
#include "core/prng.hpp"

namespace ai = ge::ai;

TEST(pursuit_keeps_its_mark_ahead_on_the_line_and_runs_out_at_its_end) {
  // The line z = 0 from x = 0 to x = 100, walked forwards.
  const auto at = [](double s) { return ai::LinePoint{s, 0, 1, 0}; };
  double s = 0;
  ai::LinePoint mark;
  CHECK(ai::pursue_along(s, 100, 1, 5, 20, 1, at, mark));
  CHECK(mark.x > 20 && ge::core::hypot(mark.x - 20, mark.z - 1) > 5);
  CHECK(mark.x < 27);
  // Near the end there is no mark ahead: run out.
  double s2 = 90;
  CHECK(!ai::pursue_along(s2, 100, 1, 5, 98, 0, at, mark));
}

namespace {

struct Cost {
  double cost = 0;
};

}  // namespace

TEST(the_fan_takes_the_cheapest_heading_and_the_one_held_on_a_tie) {
  const ai::Fan fan{4, 1.0};
  // Cheapest at 0.5 rad off the way wanted.
  const auto p = ai::pick_heading(0.2, 0, fan, [](double h) { return Cost{std::fabs(h - 0.5)}; });
  CHECK(p.heading == 0.5 && p.costed.cost == 0);
  const auto tie = ai::pick_heading(0.3, 0, fan, [](double) { return Cost{1}; });
  CHECK(tie.heading == 0.3);
}

TEST(the_grid_visits_whom_the_full_pass_would_in_its_order_so_the_push_is_the_same) {
  struct P {
    double x, z;
  };
  ge::core::Rng rng = ge::core::make_rng(7);
  std::vector<P> bots;
  for (int i = 0; i < 300; i++) {
    const double x = ge::core::next(rng) * 200;
    bots.push_back({x, ge::core::next(rng) * 200});
  }
  ai::Neighbours near;
  constexpr double kApart = 2.5;
  near.build(
      static_cast<int32_t>(bots.size()), kApart + 1,
      [&](int32_t i) { return bots[static_cast<size_t>(i)]; },
      [](int32_t i) { return i % 50 == 0; });
  CHECK(near.size() == 300);
  int32_t visits = 0;
  for (size_t k = 0; k < bots.size(); k++) {
    const P b = bots[k];
    ai::Away all;
    for (const P& o : bots) ai::add_apart(all, b.x - o.x, b.z - o.z, kApart);
    ai::Away grid;
    int32_t last = -1;
    near.for_each_near(b.x, b.z, [&](int32_t i) {
      CHECK(i > last);
      last = i;
      visits++;
      const P o = bots[static_cast<size_t>(i)];
      ai::add_apart(grid, b.x - o.x, b.z - o.z, kApart);
    });
    CHECK(std::memcmp(&all, &grid, sizeof all) == 0);
  }
  // Far fewer visits than the full pass's 300 a bot.
  CHECK(visits < 300 * 30);
}

TEST(the_stuck_check_counts_the_windows_a_bot_barely_moved) {
  ai::Progress p;
  ai::restart(p, 0, 0, 0);
  CHECK(!ai::caught(p, 0.1, 0, 1, 4, 1.5));  // the window not yet over
  CHECK(ai::caught(p, 0.2, 0, 4, 4, 1.5) && p.slow == 1);
  CHECK(ai::caught(p, 0.5, 0, 8, 4, 1.5) && p.slow == 2);
  CHECK(!ai::caught(p, 10, 0, 12, 4, 1.5) && p.slow == 0);
  p.until = 15;
  CHECK(ai::detouring(p, 14) && !ai::detouring(p, 15));
}

namespace {

enum class Mode : uint8_t { walk, run };
const char* mode_name(Mode m) { return m == Mode::walk ? "walk" : "run"; }

}  // namespace

TEST(the_selector_takes_the_best_and_records_every_option_and_a_mode_change_its_move) {
  const double scores[] = {1, 3, 3, 2};
  const auto score = [&](int32_t i) { return scores[i]; };
  const auto name = [](int32_t i) {
    static const char* const kNames[] = {"a", "b", "c", "d"};
    return kNames[i];
  };
  CHECK(ai::pick_best(4, score, nullptr, "pick", name, "best") == 1);
  CHECK(ai::pick_best(0, score, nullptr, "pick", name, "best") == -1);
  ai::Why why;
  ai::Why* w = why.follow(3, 10);
  CHECK(ai::pick_best(4, score, w, "pick", name, "best") == 1);
  CHECK(why.decisions().size() == 1);
  const ai::Decision& d = why.decisions()[0];
  CHECK(why.options(d).size() == 4 && std::strcmp(d.chosen, "b") == 0 && d.margin == 0);
  Mode m = Mode::walk;
  double held = 2.5;
  ai::enter(m, held, Mode::run, w, mode_name, "late");
  CHECK(m == Mode::run && held == 0);
  CHECK(why.decisions().size() == 2 && why.fact_of(why.decisions()[1], "held") == 2.5);
  ai::enter(m, held, Mode::walk, nullptr, mode_name, "tired");
  CHECK(m == Mode::walk && why.decisions().size() == 2);
}
