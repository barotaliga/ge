// THE VILLAGE ON THE FREE RIDE: the valley's streets (mountain/village/streets.hpp) and their
// traffic (traffic.hpp) planned once when a free ride is created, and the skier meeting them.
//
// - PLANNED AT CREATE (`plan_village`), after the snowmobile is parked and the helicopter's pad is
//   chosen: the streets laid clear of both (their places passed as the plan's pads), the ground
//   under them baked, the trees felled for them taken out of the run's tree index (so the skier,
//   the machines, the crowd and a thrown body all find them gone), and the traffic planned on
//   them. A map with no village carries none, and nothing below runs.
// - HIM AGAINST THE TRAFFIC (`meet_traffic`): every vehicle where the clock has it
//   (`vehicle_at`, a pure function of the plan and `state.t`) and every car parked is the box of
//   its body he is held out of, unless he is clear over its roof. A vehicle is a wall to him: he
//   alone is put out of it, and the closing speed between them (its own speed along its heading
//   included) traded back at `village::kRestitution`. A closing speed of `village::kThrowSpeed` or
//   more puts him down (a wipeout, as into a trunk, so the body judges the blow), a `traffic`
//   event either way past `village::kEventSpeed`.
//
// Only a racer on his skis, off any lift and any machine. Draws nothing and allocates nothing
// while stepped, so the traffic moves no stream: a run's digests move only where the skier meets
// a vehicle or a felled tree's place.
//
// ge:summary The village on the free ride: streets, felled trees and traffic, and the skier struck
// ge:keywords village plan_village meet_traffic traffic struck felled pads vehicle parked
// ge:keywords skier-against-vehicle run-over
// ge:skill sim
// ge:test tests/fall_line_village_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "fall_line/sim/state.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/streets.hpp"
#include "mountain/village/traffic.hpp"

namespace fall_line::sim {

namespace village {

// HIM AGAINST A VEHICLE: the share of the closing speed traded back (a body against a car's panel
// gives most of it up); the closing speed that puts him down (about 11 km/h: a car is a wall to a
// man on skis); the least closing speed worth a `traffic` event.
inline constexpr double kRestitution = 0.2;
inline constexpr double kThrowSpeed = 3;
inline constexpr double kEventSpeed = 0.5;

}  // namespace village

// THE VILLAGE a free ride carries: its streets, the ground under them, which of the map's trees
// are felled for them, and the traffic on them (none where the streets carry no lanes).
struct Village {
  ge::mountain::village::Streets streets;
  ge::mountain::village::GroundMask ground;
  std::vector<uint8_t> felled;
  int32_t felled_count = 0;
  std::optional<ge::mountain::village::TrafficPlan> traffic;
};

// Plan the village on a free ride's map, clear of the snowmobile's spot and the helicopter's pad,
// and fell its trees out of `state.trees`. Leaves `state.village` empty where the map lays none.
void plan_village(GameState& state);

// Every racer on his skis against the village's traffic, its vehicles where the clock has them.
// Writes `wipeout` and `traffic` events.
void meet_traffic(GameState& state);

}  // namespace fall_line::sim
