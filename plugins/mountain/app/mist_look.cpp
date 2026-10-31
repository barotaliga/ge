// The mountain's mists: its day of air, and the fog that air lays in the valley, round the eye and
// on the summit.
//
// ge:summary The mountain's mists: the formulas behind mist_look.hpp
// ge:keywords mist fog valley-fog ground-mist summit-cloud climate_of mists_at
// ge:skill mountain render
// ge:test tests/mountain_mist_look_test.cpp
#include "mountain/app/mist_look.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "mountain/air/air.hpp"
#include "render/mist_fog.hpp"

namespace ge::mountain::app {

namespace {

namespace lm = core::libm;
namespace mg = mapgen;

// The way the map's wind blows TO, a unit (x, z).
void wind_toward(const mg::Weather& w, double out[2]) {
  out[0] = -lm::sin(w.wind_from);
  out[1] = -lm::cos(w.wind_from);
}

wind::Site open_snow(double y) {
  wind::Site s;
  s.y = y;
  s.above = 2;
  s.snow = 1;
  return s;
}

// A bank of `mist` lying on the ground round (x, z), `reach` m out.
render::FogVolume bank(const wind::Mist& mist, double x, double z, double reach,
                       const double drift[2]) {
  render::MistPlace at;
  at.centre[0] = x;
  at.centre[1] = z;
  at.half[0] = at.half[1] = reach;
  at.hug_ground = true;
  at.round = true;
  at.drift[0] = drift[0];
  at.drift[1] = drift[1];
  return render::mist_volume(mist, at);
}

}  // namespace

MistLook mists_at(const mg::Level& level, double hour, const double eye[3]) {
  MistLook out;
  const wind::Climate c = air::climate_of(level);
  const mg::Mountain& m = level.mountain;
  double toward[2];
  wind_toward(level.weather, toward);
  // THE VALLEY'S FLOOR, in a hollow as deep as the mountain: the night's cold pool.
  wind::Site floor = open_snow(m.base.y);
  floor.hollow = std::min(m.vertical, 600.0);
  out.floor = wind::mist_at(c, floor, hour);
  const double calm[2] = {kValleyDrift * c.wind * toward[0], kValleyDrift * c.wind * toward[1]};
  out.valley = render::mist_valley(out.floor, m.base.y, calm);
  const double drift[2] = {kMistDrift * c.wind * toward[0], kMistDrift * c.wind * toward[1]};
  // THE GROUND under the eye, on the open snow.
  out.ground = wind::mist_at(c, open_snow(level.ground_at(eye[0], eye[2])), hour);
  if (out.ground.kind != wind::MistKind::none)
    out.areas.push_back(bank(out.ground, eye[0], eye[2], kMistReach, drift));
  // THE SUMMIT, in its own cloud.
  out.summit = wind::mist_at(c, open_snow(m.summit.y), hour);
  if (out.summit.kind == wind::MistKind::hill)
    out.areas.push_back(bank(out.summit, m.summit.x, m.summit.z, kSummitReach, drift));
  return out;
}

WaterMist water_mist(const mg::Level& level, const wind::Climate& climate, double hour,
                     const MapWater& water) {
  WaterMist out;
  wind::Climate c = climate;
  c.water_c = water.water_c;
  // On the surface itself: open water under the eye, nothing wetter.
  wind::Site s;
  s.y = water.level;
  s.above = 2;
  s.water_m = 0;
  s.wet = 1;
  out.mist = wind::mist_at(c, s, hour);
  double toward[2];
  wind_toward(level.weather, toward);
  render::MistPlace at;
  at.centre[0] = water.centre[0];
  at.centre[1] = water.centre[1];
  at.half[0] = water.half[0];
  at.half[1] = water.half[1];
  at.floor_y = water.level;
  // A box over the shore's (its corners on the shore, where the bank lies too), at its full
  // density from the water up.
  at.edge = 1;
  at.drift[0] = kMistDrift * c.wind * toward[0];
  at.drift[1] = kMistDrift * c.wind * toward[1];
  out.area = render::mist_volume(out.mist, at);
  return out;
}

}  // namespace ge::mountain::app
