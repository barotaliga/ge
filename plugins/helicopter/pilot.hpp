// THE BOT'S HANDS ON THE HELICOPTER: the same four controls a pilot has and nothing more, flown
// by a pilot who knows the machine's physics. What the scenarios stage their flights with, what a
// game's pilot flies a machine home on, and what proves the bare physics can be flown at all.
//
// A CASCADE, as a pilot flies: where it is to be, the velocity to get there (slowed in time,
// `pilot::kBrake`), the acceleration that wants, the disc's tilt that gives it (the drag at speed
// leant into), the cyclic that tilts the disc there against its damping and its flapback; the
// height over the snow ahead, the climb, the collective that lifts for it through what the rotor
// can give (`thrust_most`); the way it is going, the heading, the pedals against the torque and
// the fin's weathervane.
//
// WHY IT FLIES SO (ai/why.hpp), when it is handed a recorder: the `way` decision (what holds its
// speed down: the cruise, the climb to clear what is ahead, the stop at the aim or the closing on
// it, each scored by how slow it asks so the slowest wins and the margin is how near the next
// one is), then the `height` decision (climbing to the snow ahead, sinking to it, holding, or
// settling onto the aim), with what it read: the distance, the snow ahead, the speed and climb
// wanted. Recording decides nothing.
//
// ge:summary The bot's hands on the helicopter: fly to an aim at a height, and land there
// ge:keywords helicopter pilot bot heli_pilot aim hover cruise climb land cascade cyclic
// ge:keywords collective pedals terrain-following why decision trace
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#pragma once

#include "ai/why.hpp"
#include "helicopter/heli.hpp"

namespace ge::helicopter {

// Where the bot flies: to (x, z), `height` m over the snow, and with `land` down onto the snow
// there, set down facing `face` rad over the last stretch when `has_face`.
struct HeliAim {
  double x = 0;
  double z = 0;
  double height = 45;
  bool land = false;
  bool has_face = false;
  double face = 0;
};

// The bot's controls this step, flying `h` to `aim`; its decisions recorded on `why` when given.
HeliControls heli_pilot(const HeliState& h, const HeliWorld& world, const HeliLoad& load,
                        const HeliAim& aim, ai::Why* why = nullptr);

// THE HANDS: the four controls that fly the machine at the velocity (`vx`, `vz`) over the snow,
// turning at `turn` rad/s and climbing at `climb` m/s.
HeliControls heli_hands(const HeliState& h, const HeliWorld& world, const HeliLoad& load, double vx,
                        double vz, double turn, double climb);

}  // namespace ge::helicopter
