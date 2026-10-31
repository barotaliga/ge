// THE MOUNTAIN'S MISTS (docs/rendering.md § Fall Line's fog): the fog the map's air lays at an
// hour, as the renderer draws it. The air is engine/wind's (climate.hpp, mist.hpp): this file only
// says what the mountain's day is and where the fog is asked about.
//
// - THE DAY (plugins/mountain/air's `climate_of`): a winter's day on the map's face. The day's
//   mean falls with the latitude (a degree further from the equator, half a degree colder at the
//   sea) and with the valley's height (the standard lapse); the sky's kind sets the moisture, the
//   cover and the swing (a clear night's is widest), the map's wind its wind, and the map's sun
//   its sunrise and sunset. A FOG sky is the inversion's day: damp, still air under a clear sky,
//   so its valleys fill.
// - THE SEA OF FOG (`MistLook::valley`, render/valley_fog.hpp): the air at the valley's floor, in
//   a hollow as deep as the mountain (to 600 m), where the night's cold pool lies. A radiation or
//   valley fog there fills the valleys LEVEL to its top; it burns off as the morning's sun breaks
//   the pool, and its top drifts on a fifth of the wind (the air under an inversion is nearly
//   still).
// - THE GROUND MIST round the eye (`MistLook::areas`, render/fog.hpp's froxels): the air at the
//   ground under the eye, on the open snow; a bank as deep as the fog the air lays, hugging the
//   ground, carved into patches by the noise and drifting on the wind.
// - THE CLOUD ON THE SUMMIT: the air at the summit; where it lies in its own cloud (a hill fog) a
//   bank round the top, as deep as that fog, seen from inside when the eye is in it.
// - THE FOG OVER ITS WATER (`water_mist`, M51c): the air on a lake's surface asked by any caller
//   that lays one (the generator lays none yet: docs/goals.md M24l), with the day's climate it
//   is handed (the map's, a lab's), so a still, clear dawn fogs the lake and a windy one does not.
//
// Every answer is a pure function of the map, the hour and the eye: nothing is stepped, nothing
// draws a random number.
//
// ge:summary The mountain's mists: the valley's sea of fog, the ground mist, the summit's cloud
// ge:keywords mist fog valley-fog sea-of-fog inversion ground-mist hill-fog summit-cloud burn-off
// ge:keywords climate_of mists_at climate radiation-fog water_mist lake-fog steam-fog
// ge:skill mountain render
// ge:test tests/mountain_mist_look_test.cpp
#pragma once

#include <vector>

#include "mountain/mapgen/types.hpp"
#include "render/fog.hpp"
#include "render/view.hpp"
#include "wind/climate.hpp"
#include "wind/mist.hpp"

namespace ge::mountain::app {

// The share of the map's wind the valley's sea drifts on, and the ground mist's.
inline constexpr double kValleyDrift = 0.2;
inline constexpr double kMistDrift = 0.6;
// How far round the eye the ground mist lies, m (the froxels reach 160 m).
inline constexpr double kMistReach = 300;
// How far round the summit its cloud lies, m.
inline constexpr double kSummitReach = 500;

struct MistLook {
  render::ValleyFog valley;              // the sea in the valleys; density 0, none
  std::vector<render::FogVolume> areas;  // the ground mist round the eye, the summit's cloud
  wind::Mist floor, ground, summit;      // what the air laid at each place
};

// The fog at solar `hour` seen from `eye` (world m).
MistLook mists_at(const mapgen::Level& level, double hour, const double eye[3]);

// A body of open water on the map (a tarn, a lake): its middle and the half sizes of its shore's
// box (world m, x and z), its surface's height and its water's temperature, C.
struct MapWater {
  double centre[2] = {0, 0};
  double half[2] = {50, 50};
  double level = 0;
  double water_c = 4;
};

// THE FOG OVER THE WATER the day's air `climate` lays at solar `hour` (its `water_c` the water's
// own): the air on the surface asked (`wind::mist_at` over open water), a steam fog rising off
// water far warmer than the air, a radiation fog on a clear, still night (a wind mixes it away),
// an advection fog where moist air is blown over colder water; drawn as an area of fog over the
// shore's box (render/mist_fog.hpp), drifting on `kMistDrift` of the map's wind.
struct WaterMist {
  wind::Mist mist;
  render::FogVolume area;  // density 0 when none lies
};
WaterMist water_mist(const mapgen::Level& level, const wind::Climate& climate, double hour,
                     const MapWater& water);

}  // namespace ge::mountain::app
