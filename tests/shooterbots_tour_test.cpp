// THE TOUR: the quickest order through stops. Against every order of a few stops it is never
// more than a tenth worse; it holds every stop that must come first and keeps the exit last; it
// beats the nearest stop each time where that leaves a straggler behind; a stop with no way to it
// still has its place; the same table gives the same order to the bit.
//
// ge:summary Tests: the tour's order against brute force, stops before others, the exit last
// ge:keywords test tour tsp or-opt 2-opt precedence brute-force straggler
// ge:skill shooterbots testing
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "ai/path.hpp"
#include "check.hpp"
#include "core/prng.hpp"
#include "shooterbots/tour.hpp"

namespace sb = ge::shooterbots;

namespace {

// Stops at points on a plane (stop 0 the start), each leg the distance (Manhattan, so no root),
// a leg down `slope` cheaper than up: a table that differs each way.
std::vector<double> table(const std::vector<double>& x, const std::vector<double>& y,
                          double slope = 0) {
  const size_t n = x.size();
  std::vector<double> c(n * n);
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++) {
      const double d = std::abs(x[i] - x[j]) + std::abs(y[i] - y[j]);
      c[i * n + j] = d + (y[j] > y[i] ? slope * (y[j] - y[i]) : 0);
    }
  return c;
}

bool order_holds(const std::vector<int32_t>& order, const std::vector<sb::TourBefore>& before,
                 int32_t last) {
  std::vector<size_t> pos(order.size() + 1);
  for (size_t k = 0; k < order.size(); k++) pos[static_cast<size_t>(order[k])] = k;
  for (const sb::TourBefore& r : before)
    if (pos[static_cast<size_t>(r.first)] > pos[static_cast<size_t>(r.then)]) return false;
  return last == 0 || order.back() == last;
}

double brute(const std::vector<double>& c, int32_t n, const std::vector<sb::TourBefore>& before,
             int32_t last) {
  std::vector<int32_t> order;
  for (int32_t s = 1; s <= n; s++) order.push_back(s);
  double best = -1;
  do {
    if (!order_holds(order, before, last)) continue;
    const double v = sb::tour_cost(c, n, order);
    if (best < 0 || v < best) best = v;
  } while (std::next_permutation(order.begin(), order.end()));
  return best;
}

}  // namespace

TEST(the_tour_is_within_a_tenth_of_the_best_order_of_a_few_stops) {
  ge::core::Rng rng = ge::core::make_rng(11);
  sb::Tour t;
  double worst = 1;
  for (int trial = 0; trial < 40; trial++) {
    const int32_t n = 7;
    std::vector<double> x, y;
    for (int32_t k = 0; k <= n; k++) {
      x.push_back(ge::core::next(rng) * 1000);
      y.push_back(ge::core::next(rng) * 1000);
    }
    const std::vector<double> c = table(x, y, trial % 2 ? 1.5 : 0);
    std::vector<sb::TourBefore> before;
    if (trial % 3 == 0) before = {{5, 2}, {3, 6}};
    const int32_t last = trial % 4 == 0 ? 4 : 0;
    sb::tour_plan(t, c, n, before, last);
    CHECK(static_cast<int32_t>(t.order.size()) == n);
    CHECK_MSG(order_holds(t.order, before, last), "every stop before another, the exit last");
    const double best = brute(c, n, before, last);
    CHECK(t.stats.cost == sb::tour_cost(c, n, t.order) && t.stats.cost <= t.stats.built);
    worst = std::max(worst, t.stats.cost / best);
  }
  std::printf("worst against the best order %.3f\n", worst);
  CHECK_MSG(worst <= 1.10, "the tour is within a tenth of the best order");
}

TEST(the_tour_leaves_no_straggler_where_the_nearest_stop_would) {
  // A row of stops, one just behind the start: the nearest each time runs east, back west past
  // the start for the one behind, then all the way east again; the tour takes it first.
  std::vector<double> x = {0, 10, 20, 30, -15, 100}, y(x.size(), 0);
  const std::vector<double> c = table(x, y);
  sb::Tour t;
  sb::tour_plan(t, c, 5, {});
  std::printf("built %.0f bettered %.0f\n", t.stats.built, t.stats.cost);
  CHECK(t.stats.built == 30 + 45 + 115);
  CHECK(t.stats.cost == 15 + 115);
  CHECK(t.order.front() == 4 && t.order.back() == 5);
}

TEST(a_stop_with_no_way_keeps_its_place_and_the_same_table_gives_the_same_order) {
  std::vector<double> x = {0, 100, 200, 300, 400}, y(x.size(), 0);
  std::vector<double> c = table(x, y);
  for (size_t i = 0; i < 5; i++) c[i * 5 + 2] = ge::ai::kUnreached;  // stop 2: no way in
  sb::Tour a, b;
  sb::tour_plan(a, c, 4, {});
  sb::tour_plan(b, c, 4, {});
  CHECK(a.order == b.order);
  CHECK_BITS(a.stats.cost, b.stats.cost);
}

TEST(a_ring_of_stops_each_before_another_still_gives_every_stop) {
  std::vector<double> x = {0, 1, 2, 3}, y(x.size(), 0);
  const std::vector<double> c = table(x, y);
  sb::Tour t;
  sb::tour_plan(t, c, 3, std::vector<sb::TourBefore>{{1, 2}, {2, 3}, {3, 1}});
  std::vector<int32_t> sorted = t.order;
  std::sort(sorted.begin(), sorted.end());
  CHECK((sorted == std::vector<int32_t>{1, 2, 3}));
}
