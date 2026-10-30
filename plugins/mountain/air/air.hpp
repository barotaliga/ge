// THE AIR OVER THE SNOW AT RUN TIME: the wind and the falling snow at a moment, the mean R19 dealt
// (mapgen/weather.hpp: `Level::weather`) breathing in gusts and squalls. Both are PURE FUNCTIONS
// of (level, t): the gusts and the squalls are slow sines whose phases are hashed off the map's
// seed, so the same map at the same second has the same air on every machine and in every replay,
// and nothing here draws from a run's generator.
//
// - THE WIND (`wind_at`, the engine's `wind::Field` the map deals, `field_of`): the dealt 10 m mean
// times one plus three gusts (6.3, 17.9 and 53 s
//   periods, 0.14, 0.2 and 0.16 of the mean), veering up to 0.22 rad either way on a 41 s swing.
//   IT NEVER BLOWS UP THE MOUNTAIN: a bearing that would carry the air against +z (the fall line)
//   is mirrored across the slope (`downhill_from`), so an even deal round the compass becomes an
//   even deal over the half that blows across or down it. The fold is taken here, at run time, so
//   no map a seed builds moves.
// - THE FALL (`fall_at`): 0 (nothing) .. 1 (a blizzard at its worst), the dealt `snowfall` times
//   one plus three squalls (37, 113 and 347 s; 0.35, 0.4 and 0.25 of it) breathing by the sky's
//   kind (a flurry comes and goes to nothing, a steady fall barely wavers, a storm surges), and a
//   storm's gusts lifting snow off the ground into the air. Out of a sky that does not snow, 0.
//   With it, how far one can see through it (`visibility_in`: 45 m at a fall of 1, opening as
//   the inverse square of the fall, 20 km at none).
// - SNOW OR RAIN (`fall_at`'s height): under the map's snow line (`Weather::snowline`, raised only
//   by a sky picked by hand) the fall turns to rain over engine/wind/precip.hpp's melting band,
//   its `rain` share; one sees through rain as precip.hpp's `visibility_of` says (far further than
//   through snow), a sleet blended between in the logarithm. Above the line, or with no height,
//   nothing moves: a dealt map's air keeps its bits.
//
// - A THUNDERSTORM (`storm_of`, `Weather::thunder` above 0, a sky picked by hand): engine/wind's
//   multicell storm laid off the map's seed, its first cell mature at the run's start and drifting
//   at `kSteer` along the map's own (folded) wind so its centre passes over the mountain's middle
//   `kOverhead` s in, new cells rising on its forward-right flank for an hour, the cloud's base
//   at least `kBaseOverSummit` over the summit. With a place, the
//   storm's gust front is added to the wind (`wind_at(level, storm, t, x, z)`) and its rain to the
//   fall (`fall_at(level, storm, t, x, y, z)`, the two as independent chances), the rain's phase
//   read off the freezing level the map's snow line gives, LOWERED under a downdraft (the rain's
//   melting cools the air it falls through) by up to `kCooled` m at a severe core: graupel and
//   sleet on a summit the summer's rain otherwise reaches. Without a storm both are the
//   placeless functions' bits.
//
// Readers: the frame's falling snow and the particles' drift (games/fall_line/app/frame.hpp). The
// skier's drag against it, the sound's wind and the new snow a fall lays are later readers of the
// same two functions.
//
// ge:summary The wind and the falling snow at a moment: gusts, veer, squalls, visibility
// ge:keywords air wind gust veer downhill_from wind_at field_of fall_at snowfall squall visibility
// ge:keywords visibility_in weather blizzard flurries storm rain sleet snowline aloft
// ge:keywords thunderstorm storm_of thunder gust-front outflow downdraft freezing-level cooled_at
// ge:skill mountain
// ge:test tests/mountain_air_test.cpp
#pragma once

#include "mountain/mapgen/types.hpp"
#include "wind/field.hpp"
#include "wind/storm.hpp"

namespace ge::mountain::air {

// The air at 10 m over open ground at a moment.
struct Wind {
  double x = 0, z = 0;  // the air's velocity, m/s, world frame (heading 0 along +z)
  double speed = 0;     // m/s
  double gust = 0.5;    // the gust over the mean: 0 a lull .. 1 the strongest the map gets
};

// The snow in the air at a moment.
struct Fall {
  double fall = 0;            // 0 (nothing) .. 1 (a blizzard at its worst)
  double visibility = 20000;  // how far one sees through it, m
  double rain = 0;            // the share of it that is rain where it was asked: 0 snow .. 1 rain
};

// A height above every snow line: the fall as it is aloft, all snow.
inline constexpr double kAloft = 1e9;

// The bearing a wind FROM `from` really blows from: mirrored across the slope when it would blow
// up it, unchanged otherwise. Continuous through the fold.
double downhill_from(double from);

// The map's wind as the engine's field (engine/wind): the dealt mean and bearing, the three gusts
// and the veer with their phases hashed off the map's seed, the downhill fold, a snowfield's
// roughness. `wind_at` is the field's open air at 10 m, bit for bit what it was before the field.
wind::Field field_of(const mapgen::Level& level);

Wind wind_at(const mapgen::Level& level, double t);
// The fall at time `t` s and world height `y` m.
Fall fall_at(const mapgen::Level& level, double t, double y = kAloft);

// THE STORM over the map: `Weather::thunder`'s cells (none: `cells` 0).
inline constexpr double kSteer = 9;             // m/s, the cells' drift
inline constexpr double kOverhead = 240;        // s into the run, the first cell over the middle
inline constexpr double kCooled = 1200;         // m, the freezing level's fall under a severe core
inline constexpr double kBaseOverSummit = 300;  // m: the storm's cloud base at least this over it
wind::Storm storm_of(const mapgen::Level& level);

// The wind at 10 m over open ground at (x, z) with `storm`'s gust front added.
Wind wind_at(const mapgen::Level& level, const wind::Storm& storm, double t, double x, double z);
// The fall at (x, y, z) with `storm`'s rain in it.
Fall fall_at(const mapgen::Level& level, const wind::Storm& storm, double t, double x, double y,
             double z);
// How far the freezing level stands under `storm`'s downdrafts at (x, z), m below the map's.
double cooled_at(const wind::Storm& storm, double t, double x, double z);

// How far one sees through a fall of `fall`, m.
double visibility_in(double fall);

}  // namespace ge::mountain::air
