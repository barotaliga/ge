// THE AIR A PARAMOTOR'S WING FLIES IN: a wind as a soft wing meets it, which is a lot more than a
// skier does. A wing that flies at 55 km/h is moved bodily by a wind of half that, and kept up or
// pushed down by the air's own rise and fall; so:
//
// - THE MEAN WIND aloft: the caller's 10 m wind over open snow (`ParaWind`; `level_wind` reads
//   the map's weather) grown with height by the log law up to the surface layer's top
//   (`spec::air::kTop`), and exposed by the AIR'S altitude up the mountain, not the ground's: the
//   higher he flies, the harder it blows.
// - THE AIR'S RISE AND FALL over the ground (`ParaAir::lift`): the flow follows the slopes it
//   meets, rising at the wind's speed times the slope where it blows up one (RIDGE LIFT, what
//   keeps a soaring wing up) and sinking where it blows down one. Read at two scales (a spur's
//   flank, the whole face), each dying out with height over the ground in the depth its slopes
//   reach. Down a slope the flow SEPARATES: less of the sink is kept smooth, the rest is the LEE's
//   rotor.
// - THE TURBULENCE: eddies frozen into the wind and carried down it, three octaves of them with
//   speeds growing as their size to the third, their strength a share of the mean wind: some in
//   the open air, more near the snow, much more in the lee, more again in a storm. Their vertical
//   part is what changes a wing's angle of attack, and read at both tips (`eddy_up`) it is what
//   rolls a wing and folds one side of it.
//
// Pure over the map, the field, the place and the clock: nothing here draws a random number or
// allocates. With no wind there is no lift, no lee and no eddy: still air.
//
// ge:summary The air a paramotor flies in: the wind aloft, ridge lift, the lee and the eddies
// ge:keywords paramotor air wind log-law exposure ridge-lift lee rotor sink eddies turbulence
// ge:keywords gust para_air_at eddy_up air-field level_wind
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#pragma once

#include "mountain/mapgen/types.hpp"

namespace ge::paramotor {

// The wind 10 m over open snow, m/s, world x and z (the way it blows TO).
struct ParaWind {
  double x = 0;
  double z = 0;
};

// The map's own wind (its weather's mean wind at 10 m, blowing from `wind_from`).
ParaWind level_wind(const mountain::mapgen::Level& level);

// THE EDDIES' FIELD: per component (across, up, along), two waves an octave, each with its own
// direction and phase dealt off the map's seed. Built once a map.
struct EddyWave {
  double kx = 0, ky = 0, kz = 0;  // rad/m
  double omega = 0;               // rad/s
  double phase = 0;               // rad
  double amp = 0;                 // its share of one sigma
};
inline constexpr int kWavesPerComponent = 6;
struct ParaAirField {
  EddyWave waves[3][kWavesPerComponent];
};
ParaAirField make_air_field(double seed);

// The air at a point, as the wing meets it.
struct ParaAir {
  double x = 0, y = 0, z = 0;  // the air's velocity, m/s, its rise `y` included
  double mean = 0;             // the mean wind's horizontal speed there, m/s (before the eddies)
  double lift = 0;             // the air's rise off the ground's slopes, m/s (before the eddies)
  double rough = 0;            // the eddies' strength, m/s (one sigma)
  double lee = 0;              // how far into a lee the point lies, 0..1
};

// THE AIR at (x, y, z) at time `t`: the mean wind there, its rise off the slopes and the eddies.
ParaAir para_air_at(const mountain::mapgen::Level& level, const ParaAirField& field,
                    const ParaWind& wind, double t, double x, double y, double z);

// The eddies' vertical part alone at another point of the wing, in air of strength `rough` (the
// last `para_air_at`'s), m/s: what one tip meets that the other does not.
double eddy_up(const mountain::mapgen::Level& level, const ParaAirField& field,
               const ParaWind& wind, double t, double x, double y, double z, double rough);

}  // namespace ge::paramotor
