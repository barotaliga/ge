// THE WIND ALOFT, as the cloud feels it: how the wind measured 10 m over the ground grows and
// turns with height, and how the evening winds it up a few hundred metres over the ground. Plain
// functions of a height and a sun, read by every sky that carries its layers on the wind (the
// mountain's broken skies, a classic map's night).
//
// - THE GROWTH: the surface mean grown with height on a log law over the boundary layer,
//   U(h) = U10 * (1 + 1.2 ln(1 + h / 600)), floored at the speed a kind of cloud is seen to move at
//   and held under a jet stream's 45 m/s.
// - THE VEER: the wind turns with height toward the isobars (the Ekman spiral), clockwise in the
//   north and the other way in the south, 0.35 ln(1 + h / 1000) rad.
// - THE EVENING'S LOW-LEVEL JET: once the sun is down the ground cools, the air over it stops
//   mixing with the air aloft, and the wind a few hundred metres up speeds up past its day's value
//   (up to about 1.7 times it at 300 to 500 m). `dusk_of` says how far dusk has fallen, `jet_at`
//   what it does to the wind at a height.
//
// ge:summary The wind aloft: its growth and veer with height, the evening's low-level jet
// ge:keywords wind aloft wind_aloft veer_aloft veer ekman low-level-jet jet_at dusk dusk_of
// ge:keywords boundary-layer log-law cloud drift
// ge:skill water render
// ge:test tests/wind_aloft_test.cpp
#pragma once

namespace ge::wind {

// The wind at `h` m over the ground from a surface mean of `surface` m/s, floored at `least` and
// held under 45 m/s.
double wind_aloft(double surface, double h, double least);
// How far the wind has turned by `h` m over the ground at `latitude` degrees, rad (positive
// clockwise).
double veer_aloft(double h, double latitude);

// How far dusk has fallen with the sun `elevation` rad over the horizon: 0 with it 6 degrees up
// or higher, 1 with it 6 degrees down or lower.
double dusk_of(double elevation);
// The low-level jet's factor on the wind at `h` m over the ground at dusk `dusk`.
double jet_at(double h, double dusk);

}  // namespace ge::wind
