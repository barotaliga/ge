// THE ROTOR'S WASH: the air a helicopter's rotor drives down through its disc and out along the
// snow, as a pure function of where the machine is and what it is lifting (`HeliState::thrust`).
// A game's skier feels it as any other air (a skier by a machine lifting off is shoved off the
// pad), and a renderer reads the same field to blow the snow.
//
// MOMENTUM THEORY says a disc of area A lifting T in air of density rho drives its flow down
// through itself at the induced velocity v_i = sqrt(T / 2 rho A), and on to twice that in the
// far wake. Over the snow the column turns and spreads radially as a thin OUTWASH WALL JET whose
// peak (measured round light helicopters at 1.7 to 2.1 v_i) stands about `wash::kCore` rotor radii
// out and falls off as 1/r beyond; the jet is tens of centimetres deep near its peak and thickens
// as it spreads, mixes away a dozen radii out, and is gone from the snow once the rotor is more
// than `wash::kReach` rotor diameters up.
//
// ge:summary The rotor's wash: the downwash column and the outwash wall jet along the snow
// ge:keywords helicopter wash downwash outwash wall-jet induced-velocity momentum-theory wash_at
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#pragma once

#include "core/quat.hpp"
#include "helicopter/heli.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::helicopter {

// The hover's induced velocity under a thrust of `thrust` N, m/s.
double induced_of(double thrust);

// THE WASH at (x, y, z) off `h`, m/s, world frame: down through the disc and its column, and out
// along the snow in the wall jet. Zero with the rotor lifting nothing, or crashed.
core::Vec3 wash_at(const mountain::mapgen::Level& level, const HeliState& h, double x, double y,
                   double z);

}  // namespace ge::helicopter
