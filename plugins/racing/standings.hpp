// WHERE A RIDER STANDS IN THE FIELD: the one ordering a HUD's position, a results sheet and the
// simulator all read.
//
// A rider is reduced to a `Standing` (home or not, the clock if home, how far round as one
// number) and the rule is:
// - HOME FIRST, BY THE CLOCK. Two riders home are ordered by time; home beats still out.
// - THEN FURTHER ROUND. Two riders out are ordered by `progress`: checkpoints passed plus the
//   share of the leg to the next (`leg_progress`). A run that is not a course passes its score.
//
// Pure, allocation-free, and it reads nothing of one game's state. Bits:
// tests/racing_standings_test.cpp against tests/golden/racing_standings.txt.
//
// ge:summary The field's order: home first by the clock, then further round
// ge:keywords racing standings place position field order leaderboard crossing sub-tick finish
// ge:skill racing
// ge:test tests/racing_standings_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::racing {

struct Standing {
  bool finished = false;  // home: the flag taken
  double time = 0;        // the clock at the flag, s; read only when finished
  double progress = 0;    // how far round, higher is further
};

// HOW FAR ROUND: `done` checkpoints reached, plus the share of the leg from (from_x, from_z) to
// (next_x, next_z) already covered at (x, z). NOT floored at zero (a grid stands behind the
// start line, and a field read as level there would put the back row first); capped at 0.999
// so a rider a metre from a checkpoint stays behind one who has crossed it. A zero-length (or
// NaN) leg counts as 1 m.
inline double leg_progress(double done, double from_x, double from_z, double next_x, double next_z,
                           double x, double z) {
  double leg = core::hypot(next_x - from_x, next_z - from_z);
  if (leg == 0 || leg != leg) leg = 1;
  const double left = core::hypot(next_x - x, next_z - z);
  return done + core::num::min(0.999, 1 - left / leg);
}

// WHEN WITHIN A STEP A LINE WAS CROSSED (a finish, a checkpoint): `before` and `after` the signed
// distances to it at the step's start and end, positive short of it. The share of the step at
// which it was crossed, in [0, 1] (0: already over it, 1: not yet), taken straight between the
// two; a finish clock is then (steps - 1 + share) * dt, so two riders home in the same step are
// ordered by it instead of tied.
inline double crossing_share(double before, double after) {
  if (!(before > 0)) return 0;
  if (after > 0) return 1;
  return before / (before - after);
}

// Whether `a` stands AHEAD of `b`. Neither ahead of the other is a dead heat.
inline bool is_ahead(const Standing& a, const Standing& b) {
  if (a.finished || b.finished) {
    if (a.finished && b.finished) return a.time < b.time;
    return a.finished;
  }
  return a.progress > b.progress;
}

// THE WHOLE FIELD IN ORDER, best first: writes the entries' indices into `order` (same size).
// Stable, so a dead heat keeps the order the entries came in. An insertion sort: a field is a
// handful of riders, and it allocates nothing. For any field without NaN in a compared field
// `is_ahead` is a strict weak order, so every stable sort agrees on the result.
inline void field_order(std::span<const Standing> field, std::span<uint32_t> order) {
  for (uint32_t i = 0; i < field.size(); i++) {
    uint32_t j = i;
    while (j > 0 && is_ahead(field[i], field[order[j - 1]])) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = i;
  }
}

// ONE RIDER'S PLACE, 1-based: one more than the others ahead of them.
inline int32_t place_among(const Standing& rider, std::span<const Standing> others) {
  int32_t place = 1;
  for (const Standing& other : others) {
    if (is_ahead(other, rider)) place++;
  }
  return place;
}

}  // namespace ge::racing
