// INTO THE WATER: the free ride's tarn (free.hpp's `free_map`, mountain/tarn/tarn.hpp) where its
// ice is too thin to bear a skier, or there is none, is open water over a bowl dug under its rim
// (`lake_water`: the engine's water world of every such lake, built at create). Ice that bears
// him is level snow to his skis, and nothing here touches it.
//
// - ONTO IT (`enter_lake`, after his skis' step): his skis at the surface (`kSurface` over it, or
//   under) inside an open lake's shore. Faster than `kSkimSpeed` across the water he SKIMS (a
//   `skim` event); slower, or down, he goes IN (a `sank` event: `gate` the lake, `value` its ice,
//   m), thin ice breaking under him either way.
// - SKIMMING (`step_on_water`): his skis plane on the surface: held on it, slowed by the water's
//   drag (`kSkimDrag` times the speed squared: from 15 m/s he goes in about 50 m on), turned by
//   his steer (`kSkimTurn`), until he reaches the far shore at speed and is on his skis again, or
//   drops under `kSinkSpeed` and goes in.
// - IN THE WATER: the engine's swimmer in the body regime, dressed as a skier
//   (skier/swim.hpp's `dressed_skier`): he floats on his jacket's air, soaks, takes the cold
//   shock of `mountain::tarn::kTarnWater`, and swims flat out for the shore straight out from the
//   tarn's middle, his feet never under the dug bed. At the shore, or where the water is shallow
//   enough to stand in (`kWade`), he climbs out (an `ashore` event, `value` the seconds in the
//   water) and is stood on the bank facing away from the water. Drowning, at swim failure, or
//   after `kSwimReset` s, he is reset as if he had asked.
//
// The swimmer's state is the racer's (`FreeRide::swim`), so a run in the water replays.
// Allocates nothing in a step.
//
// ge:summary The free ride's tarn: skimming it at speed, through thin ice, swimming out, reset
// ge:keywords tarn lake sank skim ashore swim dressed-skier through-the-ice planing lake_water
// ge:keywords enter_lake step_on_water on_water skim-speed sink-speed skimming swimming
// ge:skill sim
// ge:test tests/fall_line_lake_test.cpp
#pragma once

#include <cstdint>
#include <memory>

#include "fall_line/sim/state.hpp"
#include "water/world.hpp"

namespace fall_line::sim {

inline constexpr double kSurface = 0.05;    // m over the water his skis are on it
inline constexpr double kSkimSpeed = 13;    // m/s across the water to plane on it
inline constexpr double kSinkSpeed = 8;     // m/s: a skimmer slower goes in
inline constexpr double kSkimDrag = 0.012;  // 1/m: the planing skis' drag over the speed squared
inline constexpr double kSkimTurn = 0.6;    // rad/s at full steer, planing
inline constexpr double kSwimReset = 90;    // s in the water before he is stood back on a run
inline constexpr double kPlunge = 1.3;      // m under the surface his feet go in to
inline constexpr double kShore = 1.2;       // m from the shore a swimmer climbs out
inline constexpr double kWade = 0.9;        // m of water a swimmer stands up in and wades out of
inline constexpr double kBank = 2;          // m up the bank he is stood, out of the water

// The engine's water world of `level`'s open lakes (none frozen to bear a skier); null for none.
std::shared_ptr<const ge::water::World> lake_water(
    const std::shared_ptr<const ge::mountain::mapgen::Level>& level);
// The open lake (x, z) is inside the shore of, or -1.
int32_t open_lake_at(const GameState& state, double x, double z);
// Whether racer `i` is skimming the water or in it: his step is `step_on_water`'s.
bool on_water(const GameState& state, int32_t i);
// After racer `i`'s skis' step: onto the water, skimming or in.
void enter_lake(GameState& state, int32_t i);
// One step of racer `i` on or in the water: true when it is time to reset him.
bool step_on_water(GameState& state, int32_t i, double steer, double dt);
// Forget racer `i`'s water (a reset).
void dry_off(GameState& state, int32_t i);

}  // namespace fall_line::sim
