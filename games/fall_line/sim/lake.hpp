// THROUGH THE ICE: the free ride's tarn (free.hpp's `free_map`, mountain/tarn/tarn.hpp) where its
// ice is too thin to bear a skier, or there is none, is open water over a bowl dug under its rim.
// A skier whose skis go `kUnder` m under its surface inside the shore is IN THE WATER: a `sank`
// event the step he goes in (`gate` the lake's index in `Level::lakes`, `value` its ice, m), the
// water's drag on him (`kWaterDrag`, per second), and after `kSoakReset` s in it he is reset as if
// he had asked. A tarn frozen to bear him is level snow to his skis and holds nothing here.
//
// Allocates nothing; reads only the map's lakes.
//
// ge:summary The free ride's tarn: a skier through thin ice sinks, is slowed and is reset
// ge:keywords tarn lake thin-ice sank soaked through-the-ice water-drag soak_reset step_lake
// ge:skill sim
// ge:test tests/fall_line_lake_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

inline constexpr double kUnder = 0.3;      // m under the surface his skis are in the water
inline constexpr double kWaterDrag = 4;    // per s: the water's hold on him
inline constexpr double kSoakReset = 2.5;  // s in the water before he is stood back on a run

// The lake racer `i` is in the water of, or -1.
int32_t lake_under(const GameState& state, int32_t i);
// One step of racer `i` in the free ride's water: true when it is time to reset him.
bool step_lake(GameState& state, int32_t i, double dt);

}  // namespace fall_line::sim
