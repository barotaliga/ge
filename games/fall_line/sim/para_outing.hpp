// THE BOT'S OUTING UNDER THE PARAMOTOR: begun on the summit with the rig ready over him, off the
// top on full throttle until the wing lifts him, flown straight down the mountain to a gentle
// stretch of the run he began on, spiralled down over it, glided in along it and landed on his
// skis, the rig come off him as the wing settles on the snow, and down the rest of the run's chain
// to a foot. What proves the paramotor is a way off the top and down the mountain, and that a skier
// carries on from where it sets him down.
//
// - WHERE HE LANDS (`para_landing`): a STRETCH of the summit run's chain (free.hpp's
//   `run_chain`) `kStretch` m long that falls no steeper than `kGentle` anywhere along it (a
//   wing that glides near 4 comes down onto it; onto a steeper run it never would), the first
//   one from `kLandShare` of the way down the chain's fall; the chain's last `kStretch` m where
//   there is none.
// - THE PILOT (`ParaOutingPilot`), the paramotor's own bot (`paramotor/pilot.hpp`) under the rig
//   through the player's own controls (the tuck, the brake, the steer, the lean):
//   - OFF THE SUMMIT and AWAY: to the stretch's head at `kHold` m over the snow ahead;
//   - OVER IT, within `kOver` m: the throttle shut and a toggle held, spiralling down to
//     `kFinal` m over the snow (a steep mountain leaves a wing far above it);
//   - THE FINAL: under `kFinal` m over the stretch, the throttle shut, along the run toward a
//     point `kLead` m ahead of him on it, flaring onto his skis at the snow;
//   - once the rig is off him, the bot's own line down the run's chain (bot.hpp).
//
// The pilot remembers nothing; every decision is read off the state.
//
// ge:summary The bot's outing under the paramotor: off the summit, flown, landed, skied down
// ge:keywords para-outing paramotor outing para_landing para-outing-pilot land ski-down summit
// ge:skill sim paramotor
// ge:test tests/fall_line_para_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/para_ride.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"
#include "paramotor/para.hpp"

namespace fall_line::sim {

namespace para_outing {
// THE STRETCH: from how far down the chain's fall it is looked for (share of it from the head),
// how long it is, m, the steepest it falls anywhere (rise over run, over `kGradeOver` m).
inline constexpr double kLandShare = 0.5;
inline constexpr double kStretch = 300;
inline constexpr double kGentle = 0.14;
inline constexpr double kGradeOver = 30;
// The height he holds over the snow on the way, m; how near the stretch's head he spirals down,
// m, with the toggle held how far; the height the final is flown from, m (room to roll out of a
// spiral that sinks near 13 m/s at its steepest), and how far ahead along the run it aims, m.
inline constexpr double kHold = 60;
inline constexpr double kOver = 110;
inline constexpr double kSpiral = 0.5;
inline constexpr double kFinal = 40;
inline constexpr double kLead = 120;
// How far off the run's line, m, the final is still flown along it.
inline constexpr double kBeside = 40;
}  // namespace para_outing

// Where the bot lands on run `run`'s chain: the stretch's head and end, m down the chain.
struct ParaLanding {
  double x = 0, z = 0;  // the stretch's head
  double s = 0;
  double end = 0;
};
ParaLanding para_landing(const ge::mountain::mapgen::Level& level, int32_t run);

// The rig's controls as the player's: the throttle the tuck, the brakes, the toggles the steer and
// the speed bar the lean (forward, negative, pushes it).
Controls para_as_controls(const ge::paramotor::ParaControls& p);

class ParaOutingPilot {
 public:
  explicit ParaOutingPilot(const GameState& state);
  Controls operator()(const GameState& state, int32_t i) const;
  const ParaLanding& landing() const { return landing_; }

 private:
  ParaLanding landing_;
  ge::mountain::mapgen::Track chain_;
  BotPlan down_;
};

}  // namespace fall_line::sim
