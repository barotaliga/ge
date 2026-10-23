// The quality ladder's fit: the costs, the machine's scale, the rows given up first and the exact
// knapsack over the rest.
//
// ge:summary The quality ladder's fit: the costs, the scale, the knapsack over the rows
// ge:keywords render quality-ladder fit_ladder knapsack machine_scale
// ge:skill render perf
// ge:test tests/render_ladder_test.cpp
#include "render/ladder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ge::render {

namespace {

// The grain the costs are summed in: hundredths of a millisecond.
constexpr double kGrain = 0.01;

int top(const LadderRow& row) { return static_cast<int>(row.stops.size()) - 1; }

// A cost in grains, rounded up, so a picture said to fit does.
int64_t grains(double ms) { return static_cast<int64_t>(std::ceil(ms / kGrain - 1e-9)); }

}  // namespace

double ladder_cost(std::span<const LadderRow> rows, const LadderPick& pick) {
  double c = 0;
  for (size_t r = 0; r < rows.size(); ++r) c += rows[r].stops[static_cast<size_t>(pick[r])].cost_ms;
  return c;
}

double ladder_worth(std::span<const LadderRow> rows, const LadderPick& pick) {
  double w = 0;
  for (size_t r = 0; r < rows.size(); ++r) w += rows[r].stops[static_cast<size_t>(pick[r])].worth;
  return w;
}

double machine_scale(std::span<const LadderRow> rows, const LadderPick& drawn, double floor_ms,
                     double timed_ms) {
  const double table = floor_ms + ladder_cost(rows, drawn);
  return table > 0 ? timed_ms / table : 1.0;
}

LadderFit fit_ladder(std::span<const LadderRow> rows, double budget_ms, double floor_ms,
                     double scale) {
  LadderFit fit;
  fit.pick.resize(rows.size());
  for (size_t r = 0; r < rows.size(); ++r) fit.pick[r] = top(rows[r]);
  const double room = budget_ms - floor_ms * scale;
  auto finish = [&](bool fits) {
    fit.cost_ms = ladder_cost(rows, fit.pick) * scale;
    fit.worth = ladder_worth(rows, fit.pick);
    fit.fits = fits;
    return fit;
  };
  if (grains(ladder_cost(rows, fit.pick) * scale) <= grains(room)) return finish(true);

  // The rows given up first, in their order, a stop at a time, every other row at its top.
  std::vector<size_t> first;
  for (size_t r = 0; r < rows.size(); ++r)
    if (rows[r].first > 0) first.push_back(r);
  std::stable_sort(first.begin(), first.end(),
                   [&](size_t a, size_t b) { return rows[a].first < rows[b].first; });
  for (size_t r : first) {
    while (fit.pick[r] > 0) {
      --fit.pick[r];
      if (grains(ladder_cost(rows, fit.pick) * scale) <= grains(room)) return finish(true);
    }
  }

  // The rest: the multiple-choice knapsack over the room the first rows' bottoms leave. best[b] is
  // the most worth the rows so far reach for at most b grains; choice[k][b] the stop row k took.
  // Every row's cheapest stop costs nothing, so every b is reached.
  std::vector<size_t> rest;
  int64_t spent = 0;
  for (size_t r = 0; r < rows.size(); ++r) {
    if (rows[r].first > 0)
      spent += grains(rows[r].stops[0].cost_ms * scale);
    else
      rest.push_back(r);
  }
  const int64_t budget = grains(room) - spent;
  if (budget < 0) {
    for (size_t r : rest) fit.pick[r] = 0;
    return finish(false);
  }
  const auto width = static_cast<size_t>(budget) + 1;
  std::vector<double> best(width, 0.0), next(width);
  std::vector<std::vector<int>> choice(rest.size(), std::vector<int>(width, -1));
  for (size_t k = 0; k < rest.size(); ++k) {
    const LadderRow& row = rows[rest[k]];
    for (size_t b = 0; b < width; ++b) {
      double most = -1;
      int took = -1;
      for (size_t s = 0; s < row.stops.size(); ++s) {
        const int64_t c = grains(row.stops[s].cost_ms * scale);
        if (c > static_cast<int64_t>(b)) continue;
        const double w = best[b - static_cast<size_t>(c)] + row.stops[s].worth;
        // Ties keep the cheaper stop (the first found).
        if (w > most + 1e-9) {
          most = w;
          took = static_cast<int>(s);
        }
      }
      next[b] = most;
      choice[k][b] = took;
    }
    std::swap(best, next);
  }
  auto b = static_cast<int64_t>(width - 1);
  for (size_t k = rest.size(); k-- > 0;) {
    const int s = choice[k][static_cast<size_t>(b)];
    fit.pick[rest[k]] = s;
    b -= grains(rows[rest[k]].stops[static_cast<size_t>(s)].cost_ms * scale);
  }
  return finish(true);
}

}  // namespace ge::render
