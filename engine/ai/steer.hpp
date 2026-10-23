// STEERING: the pieces a bot that moves over the ground shares.
//
// - PURE PURSUIT ALONG A LINE (`pursue_along`): the bot's mark walked along a line (the caller's
//   `at(s)`: the point at arc `s` and the way on along the line there) by `step` m at a time while
//   the point is within `reach` of the bot or behind it, so the bot always goes for a point a
//   little ahead on the line; false once the arc passes `end`, the line run out.
// - A FAN OF HEADINGS COSTED (`pick_heading`): the heading held weighed first, then `side`
//   headings either side of the one wanted across `spread` rad, each costed by the caller; the
//   cheapest is taken (the first of equals, so the one held wins a tie).
// - SEPARATION OVER A SPATIAL GRID (`Neighbours`, `add_apart`): the bots near a point found in
//   the grid's cells round it rather than by a pass over all of them, visited in ascending index
//   order (so a sum over them is the same to the bit as the full pass's); each one nearer than
//   `apart` pushes the bot away, harder the nearer. Built once a step: the cell is a bot's reach
//   plus how far one may move before the next build, and a bot that may jump anywhere in a step
//   (carried, put down) is kept out of the cells and visited by every query.
// - THE STUCK CHECK (`Progress`, `caught`): a bot's progress read a window at a time; a window
//   it should have been moving in and moved under `least` m counts as caught, and the windows in
//   a row it was caught are kept, with the side it steps off to and until when (the caller's to
//   set: a man side-steps, a machine swings off).
//
// Nothing here draws a random number; `Neighbours` allocates only when it outgrows its arrays.
//
// ge:summary Steering: pure pursuit along a line, a fan of headings costed, separation, stuck
// ge:keywords ai steer steering pursuit pure-pursuit lookahead fan heading cost separation
// ge:keywords spatial-grid neighbours keep-apart stuck progress detour side-step
// ge:skill bots
// ge:test tests/ai_steer_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/math.hpp"

namespace ge::ai {

// ─── pure pursuit along a line ───────────────────────────────────────────────────────────────

// A point on a line and the way on along it there (a unit vector over the plan).
struct LinePoint {
  double x = 0, z = 0;
  double ux = 0, uz = 0;
};

// THE MARK on a line for a bot at (x, z): walked from arc `s` by `step` (negative to walk the line
// backwards) while it is within `reach` or behind the bot; written to `mark`. False when `s` has
// passed `end` (the line run out), the mark unwritten.
template <class At>
bool pursue_along(double& s, double end, double step, double reach, double x, double z,
                  const At& at, LinePoint& mark) {
  for (;;) {
    if (step < 0 ? s <= end : s >= end) return false;
    const LinePoint p = at(s);
    const double past = (x - p.x) * p.ux + (z - p.z) * p.uz;
    if (core::hypot(p.x - x, p.z - z) > reach && past < 0) {
      mark = p;
      return true;
    }
    s += step;
  }
}

// ─── a fan of headings ───────────────────────────────────────────────────────────────────────

// `side` headings either side of the one wanted, across `spread` rad.
struct Fan {
  int32_t side = 0;
  double spread = 0;
};

// The heading taken and what it cost (the caller's own costing: anything with a `cost`).
template <class Costed>
struct Picked {
  double heading = 0;
  Costed costed{};
};

// THE CHEAPEST HEADING: `held` first, then the fan round `want`, each `cost(heading)`.
template <class F>
auto pick_heading(double held, double want, Fan fan,
                  const F& cost) -> Picked<decltype(cost(held))> {
  Picked<decltype(cost(held))> best{held, cost(held)};
  for (int32_t k = -fan.side; k <= fan.side; k++) {
    const double h = want + fan.spread * k / fan.side;
    const auto c = cost(h);
    if (c.cost < best.costed.cost) best = {h, c};
  }
  return best;
}

// ─── separation ──────────────────────────────────────────────────────────────────────────────

// The push away from the bots too near.
struct Away {
  double x = 0, z = 0;
};

// One bot at (dx, dz) from this one (this one's place minus its) pushes it away by how far inside
// `apart` it stands; none from one outside the square, nor from one on the same spot.
inline void add_apart(Away& away, double dx, double dz, double apart) {
  if (dx >= apart || dx <= -apart || dz >= apart || dz <= -apart) return;
  const double d = core::hypot(dx, dz);
  if (d < apart && d > 0.01) {
    away.x += dx / d * (apart - d);
    away.z += dz / d * (apart - d);
  }
}

// A spatial grid of bots by their place at the build: the bots near a point, in ascending index.
class Neighbours {
 public:
  // Built over bots 0..n-1 at `place(i)` (anything with x and z), in cells `cell` m across;
  // `roams(i)` keeps bot i out of the cells (it is visited by every query).
  template <class Place, class Roams>
  void build(int32_t n, double cell, const Place& place, const Roams& roams);

  // Every bot whose cell is within one of (x, z)'s, and every roaming one, in ascending index,
  // each once: `f(i)`. A superset of the bots within `cell` m of (x, z) along each axis.
  template <class F>
  void for_each_near(double x, double z, const F& f) const;

  int32_t size() const { return n_; }

 private:
  static constexpr int32_t kBuckets = 1024;  // cells hashed into buckets: a power of two
  int32_t bucket_of(int64_t ci, int64_t cj) const {
    const auto h = static_cast<uint64_t>(ci) * 0x9E3779B97F4A7C15ull ^
                   static_cast<uint64_t>(cj) * 0xC2B2AE3D27D4EB4Full;
    return static_cast<int32_t>((h >> 32) & (kBuckets - 1));
  }
  int64_t cell_of(double v) const { return static_cast<int64_t>(v / cell_ + 1e6) - 1000000; }

  int32_t n_ = 0;
  double cell_ = 1;
  std::vector<int32_t> start_;   // kBuckets + 1: where each bucket's bots begin in `items_`
  std::vector<int32_t> items_;   // the bucketed bots, each bucket's in ascending index
  std::vector<int32_t> roams_;   // the roaming bots, ascending
  std::vector<int32_t> bucket_;  // each bot's bucket, -1 roaming (the build's scratch)
};

template <class Place, class Roams>
void Neighbours::build(int32_t n, double cell, const Place& place, const Roams& roams) {
  n_ = n;
  cell_ = cell;
  start_.assign(kBuckets + 1, 0);
  bucket_.resize(static_cast<size_t>(n));
  roams_.clear();
  for (int32_t i = 0; i < n; i++) {
    if (roams(i)) {
      bucket_[static_cast<size_t>(i)] = -1;
      roams_.push_back(i);
      continue;
    }
    const auto p = place(i);
    const int32_t b = bucket_of(cell_of(p.x), cell_of(p.z));
    bucket_[static_cast<size_t>(i)] = b;
    start_[static_cast<size_t>(b) + 1]++;
  }
  for (int32_t b = 0; b < kBuckets; b++) {
    start_[static_cast<size_t>(b) + 1] += start_[static_cast<size_t>(b)];
  }
  items_.resize(static_cast<size_t>(start_[kBuckets]));
  // Filled in index order, so each bucket's run is ascending; `start_` is walked back after.
  for (int32_t i = 0; i < n; i++) {
    const int32_t b = bucket_[static_cast<size_t>(i)];
    if (b >= 0) items_[static_cast<size_t>(start_[static_cast<size_t>(b)]++)] = i;
  }
  for (int32_t b = kBuckets; b > 0; b--) {
    start_[static_cast<size_t>(b)] = start_[static_cast<size_t>(b) - 1];
  }
  start_[0] = 0;
}

template <class F>
void Neighbours::for_each_near(double x, double z, const F& f) const {
  // The runs to merge: the (up to nine, each once) buckets round the point's cell, and the roams.
  struct Run {
    const int32_t* at;
    const int32_t* end;
  };
  Run runs[10];
  int32_t count = 0;
  int32_t seen[9];
  int32_t buckets = 0;
  const int64_t ci = cell_of(x);
  const int64_t cj = cell_of(z);
  for (int64_t dj = -1; dj <= 1; dj++) {
    for (int64_t di = -1; di <= 1; di++) {
      const int32_t b = bucket_of(ci + di, cj + dj);
      bool dup = false;
      for (int32_t k = 0; k < buckets; k++) dup = dup || seen[k] == b;
      if (dup) continue;
      seen[buckets++] = b;
      const int32_t* base = items_.data();
      const int32_t lo = start_[static_cast<size_t>(b)];
      const int32_t hi = start_[static_cast<size_t>(b) + 1];
      if (lo < hi) runs[count++] = Run{base + lo, base + hi};
    }
  }
  if (!roams_.empty()) runs[count++] = Run{roams_.data(), roams_.data() + roams_.size()};
  // Merged in ascending index: each bot is in exactly one run, so none is visited twice.
  for (;;) {
    int32_t pick = -1;
    for (int32_t k = 0; k < count; k++) {
      if (runs[k].at == runs[k].end) continue;
      if (pick < 0 || *runs[k].at < *runs[pick].at) pick = k;
    }
    if (pick < 0) return;
    f(*runs[pick].at++);
  }
}

// ─── the stuck check ─────────────────────────────────────────────────────────────────────────

// A bot's progress: where it was at the window's start and when, how many windows in a row it
// was caught, and the side it steps off to and until when.
struct Progress {
  double x = 0, z = 0, t = 0;
  int32_t slow = 0;
  int32_t side = 1;
  double until = 0;
};

// The window starts again at (x, z) at `t`, nothing caught.
inline void restart(Progress& p, double x, double z, double t) {
  p.x = x;
  p.z = z;
  p.t = t;
  p.slow = 0;
}

// THE WINDOW READ at `t` with the bot at (x, z), once `window` s have passed since it began:
// caught (true, `slow` one more) when it moved under `least` m in it; `slow` back to 0 when it
// did not. The next window begins here either way.
inline bool caught(Progress& p, double x, double z, double t, double window, double least) {
  if (t - p.t < window) return false;
  const double moved = core::hypot(x - p.x, z - p.z);
  p.x = x;
  p.z = z;
  p.t = t;
  if (moved < least) {
    p.slow++;
    return true;
  }
  p.slow = 0;
  return false;
}

// Whether it is stepping off to its side at `t`.
inline bool detouring(const Progress& p, double t) { return p.until > t; }

}  // namespace ge::ai
