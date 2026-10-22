// THE BOT'S OUTING ON THE SNOWMOBILE: from beside the machine on its spot, the machine press, up
// the mountain on it along a run (from the run's foot to near its head), stopped and hopped off
// there, and down that run's chain to a foot on his skis. What proves the machine is a way up the
// mountain and that a skier carries on from where he leaves it.
//
// - THE RUN CLIMBED (`sled_climb_run`): the green or blue piste whose own end is nearest the
//   machine's spot (any piste where there is neither), the easiest faces the machine climbs.
// - THE WAY UP (`sled_up_line`): from the spot to that end, then up the run's line to `kStopDown`
//   m below its head.
// - THE PILOT (`SledOutingPilot`): beside the machine, the press; on it, the snowmobile's own
//   pilot (`snowmobile/pilot.hpp`) to a point `kAimAhead` m further up the line than the furthest
//   he has reached, at `kCruise` m/s; within `kStopShort` of the line's end the brake, and once it
//   has stopped the press again; thrown, hands off until he is put back on it; off it, the bot's
//   own line down the run's chain (bot.hpp).
//
// The pilot remembers how far up the line he has come (the line can run back near itself);
// nothing else, and nothing of the run's state.
//
// ge:summary The bot's outing on the snowmobile: up a run from its spot, hop off, ski down
// ge:keywords sled-outing snowmobile outing climb up-line pilot hop ski-down sled_climb_run
// ge:keywords sled_up_line sled-outing-pilot
// ge:skill sim snowmobile
// ge:test tests/fall_line_sled_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

namespace sled_outing {
// How far below the climbed run's head the way up ends, m.
inline constexpr double kStopDown = 15;
// The aim ahead of the furthest point reached, m, and the cruise, m/s.
inline constexpr double kAimAhead = 30;
inline constexpr double kCruise = 14;
// How far short of the way's end he brakes, m, and under what speed he hops off, m/s.
inline constexpr double kStopShort = 12;
inline constexpr double kHopUnder = 0.5;
// How far from the line a nearest point still counts as progress along it, m.
inline constexpr double kOnLine = 30;
}  // namespace sled_outing

// The run the bot climbs from the machine's spot, or -1 on a map with no ski area.
int32_t sled_climb_run(const ge::mountain::mapgen::Level& level, const SledSpot& spot);

// The way up: the spot, the run's end, then its line up to `kStopDown` below its head.
ge::mountain::mapgen::Track sled_up_line(const ge::mountain::mapgen::Level& level,
                                         const SledSpot& spot, int32_t run);

// Racer `i` stood at rest beside the machine, on its left, facing its way.
void stand_by_sled(GameState& state, int32_t i);

class SledOutingPilot {
 public:
  SledOutingPilot(const GameState& state, int32_t run);
  Controls operator()(const GameState& state, int32_t i) const;
  int32_t run() const { return run_; }
  const ge::mountain::mapgen::Track& up() const { return up_; }

 private:
  int32_t run_;
  ge::mountain::mapgen::Track up_;
  BotPlan down_;
  mutable double reached_ = 0;
};

}  // namespace fall_line::sim
