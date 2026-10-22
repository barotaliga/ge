// THE BOT'S HANDS ON THE SNOWMOBILE: the same four controls a rider has (the thumb, the brake,
// the bars, the weight) and nothing more, riding to an aim: the bars toward its bearing, the thumb
// pinned on a climb and eased on the flat to a cruise, the weight forward on a steep face to keep
// the skis down and the nose from coming over and back on the way down, and round the trunks in
// its way, slowing for them. What the scenarios and the labs stage their rides with, and what a
// game's stand-in does on the boards.
//
// ge:summary The bot's hands on the snowmobile: ride to an aim, climb, dodge trunks
// ge:keywords snowmobile pilot bot sled_pilot aim cruise climb dodge trunks throttle bars
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#pragma once

#include "snowmobile/sled.hpp"

namespace ge::snowmobile {

// Where the bot rides: to (x, z), at `cruise` m/s at most on the flat.
struct SledAim {
  double x = 0;
  double z = 0;
  double cruise = 14;
};

// How near the aim counts as there, m: the bot brakes to a stop inside it.
inline constexpr double kAimReach = 6;

// The bot's controls this step, riding `s` to `aim`.
SledControls sled_pilot(const SledState& s, const SledWorld& world, const SledAim& aim);

}  // namespace ge::snowmobile
