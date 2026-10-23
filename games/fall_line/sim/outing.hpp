// THE BOT'S OUTING on the free ride: down a piste to its foot, up the lift at that foot and down
// another run off its top. What a skier's day is made of, and what proves the lifts carry him.
//
// - THE OUTING (`outing_from`): the run he starts at the head of, a lift and the run he skis off
//   its top, the cheapest on a graph search (`ai/path.hpp`) over the runs and the lifts: from his
//   run's foot across the snow to every lift whose bottom station is within `kLiftReach` (its
//   cost the walk, m), and off its top onto every run its ramps lead to, the first listed
//   preferred and the way back down the run he came down dearer by `kBackDown`. So it is the
//   lift nearest the foot and the first of its runs not the one he came down, unless that lift
//   leads only back down it and another in reach leads elsewhere.
// - THE WAY TO THE LIFT (`to_lift_line`): the run's chain to its foot, then across the snow to a
//   point `kLineUp` m short of the load zone on the line up, and on up the line through the zone's
//   middle, held under `kWalkSpeed` past the foot so he comes into the zone slow enough to be
//   taken (lift_ride.hpp).
// - THE WAY DOWN FROM THE TOP (`from_top_line`): from where the lift lets him go to the head of
//   the next run's ramp, down the ramp onto the run at its foot's arc, and that run's chain on
//   from there to a foot.
// - THE PILOT (`OutingPilot`): the way to the lift until a lift has let him go, the way down
//   after it; while a lift has him his hands are off the controls.
//
// Lines only: the bot skis them as it skis any line (bot.hpp).
//
// ge:summary The bot's outing: a piste to its foot, the lift there, another run off its top
// ge:keywords outing outing_from to_lift_line from_top_line outing-pilot lift-at-foot walk
// ge:keywords line-up ramp next-run day line-layer
// ge:skill sim
// ge:test tests/fall_line_lift_ride_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

namespace outing {
// How far from a chain's foot a lift's bottom station is still the lift at that foot, m.
inline constexpr double kLiftReach = 150;
// How far short of the load zone's middle, up the line, he lines up to come into it, m.
inline constexpr double kLineUp = 15;
// How far past the zone's middle the line runs on, m (the pursuit aims beyond where he boards).
inline constexpr double kRunOn = 8;
// The bot's speed past the foot, on the way to the load zone, m/s.
inline constexpr double kWalkSpeed = 3;
// The width of a line laid across the snow off any run, m.
inline constexpr double kWalkWidth = 8;
// What an outing's search adds for each ramp listed before a run's off a lift's top, m (the first
// listed preferred, never enough to choose a farther lift), and for skiing back down the run he
// came down rather than another, m (a lift that has only that is taken when no other in reach
// does better by `kBackDown`).
inline constexpr double kRampRank = 1e-6;
inline constexpr double kBackDown = kLiftReach;
}  // namespace outing

// Lays a line through points on the snow: every `kEvery` m between them, its arc, its height on
// the ground and its heading; a point under half a step from the last is dropped.
class LineLayer {
 public:
  static constexpr double kEvery = 2;
  explicit LineLayer(const ge::mountain::mapgen::Level& level) : level_(level) {}
  void to(double x, double z, double width);
  ge::mountain::mapgen::Track done();

 private:
  void push(double x, double z, double s, double width);
  const ge::mountain::mapgen::Level& level_;
  ge::mountain::mapgen::Track t_;
};

struct Outing {
  int32_t first = -1;  // the run he starts at the head of
  int32_t lift = -1;   // the lift at its chain's foot, or -1: no outing from this run
  int32_t next = -1;   // the run he skis off the lift's top
};

// The outing from the head of run `first` on `level`'s lifts.
Outing outing_from(const ge::mountain::mapgen::Level& level,
                   const std::vector<ge::mountain::lifts::LiftPlan>& lifts, int32_t first);

ge::mountain::mapgen::Track to_lift_line(const ge::mountain::mapgen::Level& level,
                                         const std::vector<ge::mountain::lifts::LiftPlan>& lifts,
                                         const Outing& o);
ge::mountain::mapgen::Track from_top_line(const ge::mountain::mapgen::Level& level,
                                          const std::vector<ge::mountain::lifts::LiftPlan>& lifts,
                                          const Outing& o);

// The bot through an outing as racer `i`: the plan for each leg, chosen by how many lifts he has
// ridden.
class OutingPilot {
 public:
  OutingPilot(const GameState& state, const Outing& o);
  Controls operator()(const GameState& state, int32_t i) const;
  const BotPlan& up() const { return up_; }
  const BotPlan& down() const { return down_; }

 private:
  BotPlan up_;
  BotPlan down_;
};

}  // namespace fall_line::sim
