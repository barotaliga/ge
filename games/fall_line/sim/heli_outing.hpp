// THE BOT'S OUTING ON THE HELICOPTER: from beside its pad, the machine press, flown up the
// mountain to the head of the highest run, held in a low hover over it and dropped off there, and
// down that run's chain to a foot on his skis, while the machine's own pilot flies it home to its
// pad. What proves the helicopter is a way up the mountain, that a skier carries on from where it
// drops him, and that it goes home without him.
//
// - THE RUN DROPPED ON (`heli_drop_run`): the piste whose head is highest, a black one only where
//   there is nothing else.
// - THE PILOT (`HeliOutingPilot`): beside the machine, the press; on it, the helicopter's own
//   pilot (`helicopter/pilot.hpp`) to the run's head at `kCruiseOver` m over the snow, down to
//   `kDropOver` m of skid over it within `kApproach` m of the head, the
//   bot's hands on the four controls through the player's own (the lever, the lean, the steer,
//   the pedal); within `kDropNear` m of the head, slower than `kDropSlow` m/s and lower than
//   `kDropHigh` m, the press again; off it, the bot's own line down the run's chain (bot.hpp).
//
// The pilot remembers nothing; every decision is read off the state.
//
// ge:summary The bot's outing on the helicopter: to the highest run's head, dropped, ski down
// ge:keywords heli-outing helicopter outing drop hover heli_drop_run heli-outing-pilot ski-down
// ge:skill sim helicopter
// ge:test tests/fall_line_heli_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/heli_ride.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

namespace heli_outing {
// The height flown over the snow on the way, m, how near the head he comes down from it, m, and
// the hover the drop is made from: the skids' height over the snow, m.
inline constexpr double kCruiseOver = 45;
inline constexpr double kApproach = 80;
inline constexpr double kDropOver = 1.5;
// The drop: within this of the head, m, slower than this, m/s, the skids lower than this, m.
inline constexpr double kDropNear = 6;
inline constexpr double kDropSlow = 1.5;
inline constexpr double kDropHigh = 3;
}  // namespace heli_outing

// The run the bot is dropped on, or -1 on a map with no ski area.
int32_t heli_drop_run(const ge::mountain::mapgen::Level& level);

// Racer `i` stood at rest beside the machine's seat, out from its right skid, facing its way.
void stand_by_heli(GameState& state, int32_t i);

// A helicopter's controls as the player's: the lever, the lean (the cyclic, forward negative), the
// steer and the pedal.
Controls heli_as_controls(const ge::helicopter::HeliControls& h);

class HeliOutingPilot {
 public:
  HeliOutingPilot(const GameState& state, int32_t run);
  Controls operator()(const GameState& state, int32_t i) const;
  int32_t run() const { return run_; }

 private:
  int32_t run_;
  double head_x_ = 0, head_z_ = 0;
  BotPlan down_;
};

}  // namespace fall_line::sim
