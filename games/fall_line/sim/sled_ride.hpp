// THE FREE RIDE'S SNOWMOBILE: a deep-snow mountain sled (`plugins/snowmobile`) waiting parked at
// the bottom of the mountain, the player's way up the faces the lifts never reach.
//
// - THE SPOT (`sled_spot`): a level patch of open snow beside the village, `kBeside` m from it if
//   it can, clear of every trunk, every lift's line and bottom station and the wind tunnels'
//   lanes, parked facing up the mountain. A map with no ski area parks it beside the piste's
//   foot. A pure function of the map.
// - TAKEN (`Controls::machine`, the machine press): a skier on his skis, off any lift, within
//   `kBoardReach` m of a machine standing still and no faster than `kBoardFastest` m/s racks his
//   skis on it and stands on its boards (a `sled` event, `board`); a machine lying on its side is
//   stood back on its belt first. The engine starts.
// - RIDDEN on the skier's own controls (`sled_controls`): the tuck the thumb throttle, the brake
//   the brake lever, the steer the bars and `Controls::lean` his weight. The skier is held on the
//   boards (`boards_at`), facing its way and moving with it; his skis, the run under them and the
//   run's own resets wait. His reset stands the machine back on its belt where it is (`right`).
// - HOPPED OFF (the machine press again): stood beside it on its left, the rack's side, with its
//   way and a little out (and up, if it was moving); slower than `kTurnDown` m/s he steps off
//   facing down the fall line. The machine is left where it stands, its engine idling until
//   `kIdleFor` s, to be taken again (`hop`).
// - THROWN (snowmobile/sled.hpp's judgement: a trunk, rolled or looped, a hard or crooked
//   landing, a wall): the skier put down off the boards with the way the machine had (a
//   `wipeout`, and a `sled` event, `crash`), his skis still on its rack; the machine lies where it
//   comes to rest. When the run's down reset would stand him up he is put back on it instead,
//   the machine stood back on its belt (`right`).
// - LEFT TO ITSELF the machine settles on its parking brake, stepped like any body until it is at
//   rest, then ASLEEP: not stepped (its numbers do not move) until a rider takes it.
//
// - STOLEN: an army snowmobile left standing empty (army_sled.hpp) is taken the same way, the
//   nearest machine the press reaches, and is his from then on (`GameState::stolen`): a
//   `stole_snowmobile` offence (wanted.hpp) and an `army_sled` event, `stolen`. He rides, hops off
//   and is thrown off it as off the free ride's own. Stealing another hands the one he left back
//   to the army, standing empty where it is.
//
// Only the player (racer 0) rides one. Pure over the map and the state: draws nothing, and a step
// allocates nothing.
//
// ge:summary The free ride's snowmobile: the spot, boarding, riding, hopping off, thrown, remount
// ge:keywords sled snowmobile free-ride sled_spot place_sled step_sled_ride board hop crash
// ge:keywords remount right machine-press parked ridden down asleep sled_controls rack
// ge:keywords steal stolen army-sled ridden sled_holds
// ge:skill sim snowmobile
// ge:test tests/fall_line_sled_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "fall_line/sim/state.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"
#include "snowmobile/sled.hpp"

namespace fall_line::sim {

namespace sled_ride {
// The spot's search: its grid, m, how far out from the village, m, and the distance from the
// village it would rather be at, m.
inline constexpr double kStep = 6;
inline constexpr double kSearch = 360;
inline constexpr double kBeside = 50;
// The room kept clear round the spot, m, and the steepest the snow under it may lean (rise over
// run).
inline constexpr double kPadRadius = 4.5;
inline constexpr double kPadSlope = 0.14;
// The gaps kept from a lift's line, its bottom station and a tunnel's lane, m.
inline constexpr double kLineGap = 12;
inline constexpr double kStationGap = 26;
inline constexpr double kTunnelGap = 10;
// BOARDING: within this of the boards' middle, m, no faster than this, m/s (taken at a skier's
// cruise, never asked to stop dead beside it), the machine no faster than `kStill`, m/s.
inline constexpr double kBoardReach = 5;
inline constexpr double kBoardFastest = 12;
inline constexpr double kStill = 1;
// THE HOP OFF: out to the machine's left, m/s, and up when it is moving faster than `kHopMoving`.
inline constexpr double kHopOut = 1.6;
inline constexpr double kHopUp = 1.4;
inline constexpr double kHopMoving = 3;
// How far to the machine's left the skier stands off it, m.
inline constexpr double kStandOff = 1.1;
// Slower than this he steps off facing down the fall line, m/s.
inline constexpr double kTurnDown = 3;
// How long the engine idles with nobody on it before it is shut off, s.
inline constexpr double kIdleFor = 25;
// Asleep: at rest under this, m/s and rad/s, for this long, s.
inline constexpr double kSleepSpeed = 0.05;
inline constexpr double kSleepAfter = 1;
// At create, the machine is settled on its spot for this long, s.
inline constexpr double kSettle = 2;
}  // namespace sled_ride

// Where the machine is parked: the snow under its middle and the way it faces.
struct SledSpot {
  double x = 0, z = 0;
  double heading = 0;
};

SledSpot sled_spot(const ge::mountain::mapgen::Level& level,
                   const std::vector<ge::mountain::lifts::LiftPlan>& lifts,
                   const ge::skier::TreeIndex& trees);

// The free ride's machine parked on its spot, settled into the snow and asleep (create time).
void place_sled(GameState& state);

// WHAT THE RIDER'S HANDS DO, off his own controls: the tuck the throttle, the brake the brake,
// the steer the bars, the lean his weight.
ge::snowmobile::SledControls sled_controls(const Controls& c);

// The machine's world: the map, its trees, no wind, the assist on.
ge::snowmobile::SledWorld sled_world(const GameState& state);

// THE MACHINE'S STEP, before racer `i`'s skis: ridden, parked or lying where he was thrown, and
// the skier taken on, held on its boards or let go. True while he rides it: the step is the
// machine's, and his skis, his run and the run's resets wait.
bool step_sled_ride(GameState& state, int32_t i, const Controls& c);

// Whether the machine press takes racer `i` on now.
bool sled_within(const GameState& state, int32_t i);

// The machine he is on the boards of: the free ride's own or the one he stole; none on his skis.
FreeSled* ridden(GameState& state);
const FreeSled* ridden(const GameState& state);

// Whether racer `i` is on a machine's boards.
inline bool on_sled(const GameState& state, int32_t i) {
  return i == 0 && state.sled.present && (state.sled.rider || state.stolen.rider);
}

// Whether racer `i` is on a machine or thrown off one its reset would put him back on.
inline bool sled_holds(const GameState& state, int32_t i) {
  return i == 0 && state.sled.present &&
         (state.sled.rider || state.sled.thrown || state.stolen.rider || state.stolen.thrown);
}

// THE RUN'S DOWN RESET while the machine waits for him: racer `i`, thrown off it, put back on it
// where it lies. False when he was not thrown off it (the free ride's own reset is the caller's).
bool remount_sled(GameState& state, int32_t i);

const char* sled_phase_name(SledPhase p);

}  // namespace fall_line::sim
