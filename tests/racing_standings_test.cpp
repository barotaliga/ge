// plugins/racing/standings against tests/golden/racing_standings.txt: legs to the bit, and whole
// fields' order and every rider's place, dead heats included; a finish inside a step ordered
// by when in it the line was crossed.
//
// ge:summary Golden test: racing/standings, bit for bit
// ge:keywords test golden standings
// ge:skill testing racing
#include "racing/standings.hpp"

#include <vector>

#include "golden.hpp"

using ge::test::as_double;
namespace r = ge::racing;

TEST(standings_match_golden) {
  const auto rows = ge::test::golden_rows("tests/golden/racing_standings.txt");
  int legs = 0, fields = 0;
  for (const auto& row : rows) {
    std::vector<double> v;
    v.reserve(row.size());
    for (const uint64_t w : row) v.push_back(as_double(w));
    if (v[0] == 0) {
      legs++;
      CHECK(v.size() == 9);
      CHECK_BITS_ANY_NAN(r::leg_progress(v[1], v[2], v[3], v[4], v[5], v[6], v[7]), v[8]);
      continue;
    }
    fields++;
    const auto n = size_t(v[1]);
    CHECK(v.size() == 2 + 3 * n + 2 * n);
    std::vector<r::Standing> field;
    for (size_t k = 0; k < n; k++) field.push_back({v[2 + 3 * k] != 0, v[3 + 3 * k], v[4 + 3 * k]});
    std::vector<uint32_t> order(n);
    r::field_order(field, order);
    for (size_t k = 0; k < n; k++) CHECK(double(order[k]) == v[2 + 3 * n + k]);
    for (size_t k = 0; k < n; k++) {
      std::vector<r::Standing> others;
      for (size_t j = 0; j < n; j++) {
        if (j != k) others.push_back(field[j]);
      }
      CHECK(double(r::place_among(field[k], others)) == v[2 + 4 * n + k]);
    }
  }
  CHECK(legs == 406 && fields == 400);
}

TEST(standings_a_finish_inside_a_step_is_ordered_by_its_share) {
  CHECK(r::crossing_share(0.5, -0.5) == 0.5);
  CHECK(r::crossing_share(0.0, -1.0) == 0.0);  // over the line already
  CHECK(r::crossing_share(1.0, 0.25) == 1.0);  // not over it yet
  // Two riders over the line in step 100 at 72 Hz: the one that crossed earlier in it is ahead.
  const double dt = 1.0 / 72;
  r::Standing a{.finished = true, .time = (99 + r::crossing_share(0.1, -0.4)) * dt};
  r::Standing b{.finished = true, .time = (99 + r::crossing_share(0.3, -0.2)) * dt};
  CHECK(r::is_ahead(a, b) && !r::is_ahead(b, a));
}
