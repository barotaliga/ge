// The free ride's tarn: in the water, slowed, and reset (lake.hpp).
//
// ge:summary lake_under and step_lake: the skier in the tarn's water
// ge:keywords tarn lake sank soaked water-drag step_lake lake_under
// ge:skill sim
// ge:test tests/fall_line_lake_test.cpp
#include "fall_line/sim/lake.hpp"

#include "mountain/tarn/tarn.hpp"

namespace fall_line::sim {

int32_t lake_under(const GameState& state, int32_t i) {
  const auto& lakes = state.level->lakes;
  const ge::skier::SkierState& s = state.racers[static_cast<size_t>(i)].skier;
  for (size_t k = 0; k < lakes.size(); ++k) {
    const ge::mountain::mapgen::MapLake& lake = lakes[k];
    if (lake.depth > 0 && s.y < lake.level - kUnder && ge::mountain::tarn::on_lake(lake, s.x, s.z))
      return static_cast<int32_t>(k);
  }
  return -1;
}

bool step_lake(GameState& state, int32_t i, double dt) {
  FreeRide& f = state.racers[static_cast<size_t>(i)].free;
  const int32_t k = lake_under(state, i);
  if (k < 0) {
    f.soaked = 0;
    return false;
  }
  if (f.soaked == 0) {
    state.events.push_back(
        GameEvent{EventKind::sank, i, k, state.level->lakes[static_cast<size_t>(k)].ice});
  }
  f.soaked += dt;
  ge::skier::SkierState& s = state.racers[static_cast<size_t>(i)].skier;
  const double keep = 1 / (1 + kWaterDrag * dt);
  s.vx *= keep;
  s.vy *= keep;
  s.vz *= keep;
  return f.soaked >= kSoakReset;
}

}  // namespace fall_line::sim
