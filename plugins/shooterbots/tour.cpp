// The tour's search: nearest first under the stops-before rule, then or-opt, 2-opt and seeded
// kicks.
//
// ge:summary The tour's search: the order built, bettered by or-opt and 2-opt, kicked and kept
// ge:keywords tour or-opt 2-opt precedence kicks route-order
// ge:skill shooterbots bots
// ge:test tests/shooterbots_tour_test.cpp
#include "shooterbots/tour.hpp"

#include <cstddef>
#include <utility>

#include "core/prng.hpp"

namespace ge::shooterbots {

namespace {

constexpr double kGain = 1e-6;  // a move must gain more than this, s (no circling on rounding)
constexpr int32_t kNone = -1;   // no stop after the last

double leg(std::span<const double> cost, int32_t n, int32_t i, int32_t j) {
  if (j == kNone) return 0;
  const double c =
      cost[static_cast<size_t>(i) * static_cast<size_t>(n + 1) + static_cast<size_t>(j)];
  return c < kTourFar ? c : kTourFar;  // also NaN and infinity: no way
}

// Whether `b` (stop 0 first) holds every `before` and keeps `last` at the end.
bool holds(const std::vector<int32_t>& b, std::vector<int32_t>& pos,
           std::span<const TourBefore> before, int32_t last) {
  if (last > 0 && b.back() != last) return false;
  for (size_t k = 0; k < b.size(); k++) pos[static_cast<size_t>(b[k])] = static_cast<int32_t>(k);
  for (const TourBefore& r : before)
    if (pos[static_cast<size_t>(r.first)] > pos[static_cast<size_t>(r.then)]) return false;
  return true;
}

// Bettered in place: `b` (stop 0 first) by or-opt and 2-opt moves until a round gains nothing.
void improve(Tour& t, std::span<const double> cost, int32_t n, std::span<const TourBefore> before,
             int32_t last, std::vector<int32_t>& b, std::vector<int32_t>& w) {
  const int32_t m = n;                           // the stops after stop 0, b[1..m]
  const int32_t movable = last > 0 ? m - 1 : m;  // `last` never moves
  const auto at_b = [&](int32_t i) { return i <= m ? b[static_cast<size_t>(i)] : kNone; };
  for (int32_t round = 0; round < kTourRounds; round++) {
    t.stats.rounds++;
    bool better = false;
    // Or-opt: the run b[s..e] (one to three stops) taken out and put after b[p].
    for (int32_t len = 1; len <= 3; len++) {
      for (int32_t s = 1; s + len - 1 <= movable; s++) {
        bool moved = false;
        const int32_t e = s + len - 1;
        const int32_t prev = b[static_cast<size_t>(s - 1)], next = at_b(e + 1);
        const int32_t first = b[static_cast<size_t>(s)], end = b[static_cast<size_t>(e)];
        const double out = leg(cost, n, prev, first) + leg(cost, n, end, next) -
                           (next == kNone ? 0 : leg(cost, n, prev, next));
        for (int32_t p = 0; p <= movable && !moved; p++) {
          if (p >= s - 1 && p <= e) continue;
          const int32_t u = b[static_cast<size_t>(p)], v = at_b(p + 1);
          const double in =
              leg(cost, n, u, first) + leg(cost, n, end, v) - (v == kNone ? 0 : leg(cost, n, u, v));
          if (in - out >= -kGain) continue;
          // The order with the run moved, checked against `before`.
          w.clear();
          for (int32_t i = 0; i <= m; i++) {
            if (i >= s && i <= e) continue;
            w.push_back(b[static_cast<size_t>(i)]);
            if (i == p)
              for (int32_t r = s; r <= e; r++) w.push_back(b[static_cast<size_t>(r)]);
          }
          if (!holds(w, t.pos, before, last)) continue;
          b.swap(w);
          t.stats.moves++;
          better = moved = true;
        }
      }
    }
    // 2-opt: b[i..j] turned end for end, its inside weighed both ways.
    for (int32_t i = 1; i < movable; i++) {
      double fwd = 0, rev = 0;
      bool flipped = false;
      for (int32_t j = i + 1; j <= movable && !flipped; j++) {
        fwd += leg(cost, n, b[static_cast<size_t>(j - 1)], b[static_cast<size_t>(j)]);
        rev += leg(cost, n, b[static_cast<size_t>(j)], b[static_cast<size_t>(j - 1)]);
        const int32_t a0 = b[static_cast<size_t>(i - 1)], a1 = b[static_cast<size_t>(i)];
        const int32_t z0 = b[static_cast<size_t>(j)], z1 = at_b(j + 1);
        const double was = leg(cost, n, a0, a1) + fwd + leg(cost, n, z0, z1);
        const double now = leg(cost, n, a0, z0) + rev + leg(cost, n, a1, z1);
        if (now - was >= -kGain) continue;
        w.assign(b.begin(), b.end());
        for (int32_t lo = i, hi = j; lo < hi; lo++, hi--)
          std::swap(w[static_cast<size_t>(lo)], w[static_cast<size_t>(hi)]);
        if (!holds(w, t.pos, before, last)) continue;
        b.swap(w);
        t.stats.flips++;
        better = flipped = true;
      }
    }
    if (!better) break;
  }
}

}  // namespace

double tour_cost(std::span<const double> cost, int32_t n, std::span<const int32_t> order) {
  double c = 0;
  int32_t at = 0;
  for (const int32_t s : order) {
    c += leg(cost, n, at, s);
    at = s;
  }
  return c;
}

void tour_plan(Tour& t, std::span<const double> cost, int32_t n, std::span<const TourBefore> before,
               int32_t last) {
  t.stats = {};
  t.order.clear();
  if (n <= 0) return;
  const size_t sz = static_cast<size_t>(n) + 1;
  t.placed.assign(sz, 0);
  t.pos.assign(sz, 0);
  t.placed[0] = 1;
  // Built nearest first among the stops free to go (every stop before it placed; `last` alone).
  int32_t at = 0;
  for (int32_t k = 0; k < n; k++) {
    int32_t pick = -1;
    double best = 0;
    for (int32_t s = 1; s <= n; s++) {
      if (t.placed[static_cast<size_t>(s)] || (s == last && k < n - 1)) continue;
      bool free = true;
      for (const TourBefore& r : before)
        free = free && !(r.then == s && !t.placed[static_cast<size_t>(r.first)]);
      if (!free) continue;
      const double c = leg(cost, n, at, s);
      if (pick < 0 || c < best) {
        pick = s;
        best = c;
      }
    }
    // A ring of stops each before another: the rest in their own order.
    for (int32_t s = 1; pick < 0 && s <= n; s++)
      if (!t.placed[static_cast<size_t>(s)] && (s != last || k == n - 1)) pick = s;
    t.placed[static_cast<size_t>(pick)] = 1;
    t.order.push_back(pick);
    at = pick;
  }
  t.stats.built = tour_cost(cost, n, t.order);
  std::vector<int32_t>& b = t.trial;
  std::vector<int32_t>& w = t.waiting;
  b.assign(1, 0);
  b.insert(b.end(), t.order.begin(), t.order.end());
  improve(t, cost, n, before, last, b, w);
  t.order.assign(b.begin() + 1, b.end());
  t.stats.cost = tour_cost(cost, n, t.order);
  // Kicks: a run of the best order moved to a place drawn at random, bettered again, kept when
  // quicker (a way out of a local best the moves alone cannot leave).
  const int32_t movable = last > 0 ? n - 1 : n;
  ge::core::Rng rng = ge::core::make_rng(static_cast<uint32_t>(n) * 2654435761u);
  for (int32_t k = 0; k < kTourKicks && movable >= 4; k++) {
    const int32_t len = ge::core::int_in(rng, 1, movable / 2);
    const int32_t s = ge::core::int_in(rng, 1, movable - len + 1), e = s + len - 1;
    const int32_t p = ge::core::int_in(rng, 0, movable - len);
    b.assign(1, 0);
    b.insert(b.end(), t.order.begin(), t.order.end());
    w.clear();
    int32_t placed = 0;
    for (int32_t i = 0; i <= n; i++) {
      if (i >= s && i <= e) continue;
      w.push_back(b[static_cast<size_t>(i)]);
      if (placed++ == p)
        for (int32_t r = s; r <= e; r++) w.push_back(b[static_cast<size_t>(r)]);
    }
    if (!holds(w, t.pos, before, last)) continue;
    b.swap(w);
    improve(t, cost, n, before, last, b, w);
    const double c = tour_cost(cost, n, std::span<const int32_t>(b).subspan(1));
    if (c < t.stats.cost - kGain) {
      t.order.assign(b.begin() + 1, b.end());
      t.stats.cost = c;
      t.stats.kicks++;
    }
  }
}

}  // namespace ge::shooterbots
