// THE BOT: Fall Line's pilot, the player's stand-in in every drive lab and every rival. It flies
// the skier's pilot (`skier/pilot.hpp`: the speed plan off the skier's limits, the pure pursuit
// of a lane, the brake and the tuck, and their `line` and `speed` decisions) and keeps only what
// is Fall Line's:
//
// - THE COURSE (`plan_bot`): the plan is laid along the course's piste, its kickers' lips by
//   their arc. On a course whose SLALOM GATES stand off the line (a ski area's, R28), the plan
//   first lays a WEAVE through them (`PilotPlan::line`: from each gate's offset to the next's on a
//   smoothstep, so it passes every gate parallel to the piste) and the lane is held about that
//   weave, inside the gate's panels by `kLaneMargin`; the weave's own bends are what the speed
//   plan slows for. Each racer flies it as his `Rider` (his lane right of the line, his pace); the
//   player's rider is pace 1 on the centreline: the plain bot.
// - A missed gate is taken again: the bot asks for the reset.
// - ITS `mode` DECISION, recorded when it is handed a `ge::ai::Why` (`ge why`) before the pilot's:
//   ride, or why not (on a lift, arrived, finished, down, missed).
// - ON THE FREE RIDE (`plan_free`) the line is the chain of runs from the head of his run down to
//   a foot (`run_chain`, free.hpp), every kicker within a run's width of it planned for at its
//   nearest point; at the foot it brakes to a stop. On a line laid to a lift (`to_lift`) it skis on
//   past the foot to the line's end, and a lift that has him is left to carry him.
//
// ge:summary The bot: the skier's pilot flown down the course, its weave, or the free ride's chain
// ge:keywords bot ai plan_bot bot_controls weave slalom gates course rival rider lane pace
// ge:keywords plan_free free-ride chain kicker lip to-lift why mode decisions
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "ai/why.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/pilot.hpp"
#include "skier/skis.hpp"

namespace fall_line::sim {

// The pilot's numbers (`skier/pilot.hpp`), under the names Fall Line's tools read them by.
namespace bot = ge::skier::pilot;

// The skier's pilot's plan (`skier/pilot.hpp`), and what Fall Line adds to it.
struct BotPlan : ge::skier::PilotPlan {
  // A line laid to a lift's load zone (outing.hpp): skied on past a run's foot, slowing to its end.
  bool to_lift = false;
  template <class Io>
  friend void describe(Io& io, BotPlan& v) {
    io(static_cast<ge::skier::PilotPlan&>(v));
    io(v.to_lift);
  }
};

BotPlan plan_bot(const ge::mountain::mapgen::Level& level, const ge::skier::SkiSpec& spec);
// The free ride's plan: from the head of `run` down to a foot.
BotPlan plan_free(const ge::mountain::mapgen::Level& level, int32_t run,
                  const ge::skier::SkiSpec& spec);

// The free ride's plan along any line the bot is to ski (an outing's legs, outing.hpp), held
// under `slow` m/s from `slow_from` m along it.
BotPlan plan_line(const ge::mountain::mapgen::Level& level, ge::mountain::mapgen::Track line,
                  const ge::skier::SkiSpec& spec, double slow_from = INFINITY, double slow = 0);

// What the bot does this step as racer `i`, riding as his `Rider`. With `why` (ai/why.hpp) it
// records its decisions there; it decides the same with it or without. `lead_s`: how late its
// hands press what it decides (hands.hpp's `BotHands::lead`); it steers from where it will be
// then, as a person who knows their own reaction time looks further ahead.
Controls bot_controls(const GameState& state, int32_t i, const BotPlan& plan,
                      ge::ai::Why* why = nullptr, double lead_s = 0);

}  // namespace fall_line::sim
