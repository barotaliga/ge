// THINKING ON A SCHEDULE: a bot that does not need to decide every step decides every N, each
// on its own step of the N (staggered by its index) so a crowd of bots spreads its thinking
// evenly over the steps instead of all at once, and less often the further it is from the
// player, where nobody watches it closely.
//
// - `thinks(step, index, every)`: whether bot `index` thinks at `step`, every `every` steps
//   (1 or less: every step). A pure function of the step, so a replay thinks on the same steps.
// - `Schedule`: every `near_every` steps within `near_m` of the player, every `far_every` from
//   `far_m` on, and in between the steps' count rising in a straight line, rounded down;
//   `every_at(schedule, distance)`.
//
// ge:summary Thinking on a schedule: every N steps, staggered by index, slower far from the player
// ge:keywords ai think schedule throttle every-n-steps stagger lod distance far near budget
// ge:skill bots
// ge:test tests/ai_think_test.cpp
#pragma once

#include <cstdint>

namespace ge::ai {

// Whether bot `index` thinks at `step`, thinking every `every` steps.
constexpr bool thinks(int64_t step, int64_t index, int64_t every) {
  return every <= 1 || (step + index) % every == 0;
}

struct Schedule {
  int64_t near_every = 1;
  int64_t far_every = 1;
  double near_m = 0;
  double far_m = 0;
};

// The steps between a bot's thoughts at `distance` m from the player.
constexpr int64_t every_at(const Schedule& s, double distance) {
  if (!(distance > s.near_m)) return s.near_every;
  if (!(distance < s.far_m)) return s.far_every;
  const double f = (distance - s.near_m) / (s.far_m - s.near_m);
  return s.near_every + static_cast<int64_t>(f * static_cast<double>(s.far_every - s.near_every));
}

// Whether bot `index` thinks at `step`, `distance` m from the player.
constexpr bool thinks(int64_t step, int64_t index, const Schedule& s, double distance) {
  return thinks(step, index, every_at(s, distance));
}

}  // namespace ge::ai
