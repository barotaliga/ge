// THE LIFT RIDE: on the free ride, a skier who comes into a lift's LOAD ZONE at its bottom
// station is carried to its top. The carriers run whether anyone rides them or not
// (`mountain/lifts/lift_loop.hpp`: a pure function of the clock); the ride only says which one
// has him.
//
// - BOARDING: on the snow, on his feet, inside the zone (`in_load_zone`: near enough its middle,
//   no faster than its `fastest`, his skis near enough up the line), he is stood at the zone's
//   middle at rest facing up the line, WAITING (a `boarded` event).
// - TAKEN by the first carrier to pass the load point on its way up (`carrier_passing`), he is
//   CARRIED: placed each step under it (`carrier_place`) at its speed, on the snow below it (his
//   height on the rope is the renderer's). The skier's own step, the run under his skis and the
//   run's own resets (down, stuck) wait while a lift has him.
// - LET GO where the lift lets a rider go (`off_at`: a chair's unload, a cabin's door, a drag's
//   let-go), stood at rest where `mapgen::let_go_of` puts him (an `unloaded` event): a new
//   descent, so his foot is to be reached again.
// - His reset lets go of the lift and stands him back on the run he was last on (free.hpp).
//
// Pure over the map, the lift plans and the clock: draws nothing and allocates nothing.
//
// ge:summary The lift ride: boarding in the load zone, waiting, carried, let go at the top
// ge:keywords lift-ride board_lift step_lift load-zone boarded unloaded carried waiting let-go
// ge:keywords chair gondola drag ride
// ge:skill sim
// ge:test tests/fall_line_lift_ride_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

// Board racer `i` onto a lift whose load zone he is in, if any: after his step on the snow.
void board_lift(GameState& state, int32_t i);
// Racer `i`'s step while a lift has him, in place of his step on the snow.
void step_lift(GameState& state, int32_t i);
// Stand racer `i` waiting in lift `lift`'s load zone (a `boarded` event).
void wait_for_lift(GameState& state, int32_t i, int32_t lift);
// Whether a lift has racer `i`.
inline bool on_lift(const GameState& state, int32_t i) {
  return state.racers[static_cast<size_t>(i)].free.lift.phase != LiftPhase::none;
}

}  // namespace fall_line::sim
