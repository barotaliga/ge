// The cold's marks on the map's things: the frost's night, the road's glaze, the lake's winter.
//
// ge:summary The cold's marks on the map: the integrations behind marks.hpp
// ge:keywords frost glaze black-ice lake-ice marks integration quarter-hour
// ge:skill mountain
// ge:test tests/mountain_marks_test.cpp
#include "mountain/air/marks.hpp"

#include <algorithm>

#include "core/num.hpp"
#include "mountain/air/air.hpp"
#include "wind/cold.hpp"
#include "wind/mist.hpp"

namespace ge::mountain::air {

namespace {

constexpr double kStep = 0.25;       // h: one step of the integration
constexpr int kFrostSteps = 96;      // the 24 hours before
constexpr int kGlazeSteps = 48;      // the 12 hours before
constexpr double kGlazeBuild = 0.5;  // h of steady rain on a frozen road to glaze it across
constexpr double kGlazeMelt = 1.0;   // h a kelvin over 0 C takes to melt it

double wrapped(double hour) {
  const double h = core::num::mod(hour, 24.0);
  return h < 0 ? h + 24 : h;
}

}  // namespace

double frost_on(const mapgen::Level& level, const wind::Site& site, double hour) {
  const wind::Climate c = climate_of(level);
  double mm = 0;
  for (int i = kFrostSteps - 1; i >= 0; --i) {
    const double h = wrapped(hour - kStep * i);
    const wind::Thermal air = wind::air_at(c, site, h);
    const double surface = wind::surface_at(c, site, h);
    const wind::Settling laid = wind::deposit_on(air, surface, wind::mist_at(c, site, h), c.wind);
    const bool ice = laid.kind == wind::Deposit::frost || laid.kind == wind::Deposit::rime;
    mm += kStep * ((ice ? laid.rate : 0) - wind::frost_loss(air, surface));
    mm = std::max(mm, 0.0);
  }
  return mm;
}

double frost_cover(double mm) { return std::clamp(mm / kFrostHides, 0.0, 1.0); }

double glaze_at(const mapgen::Level& level, const wind::Site& site, double t, double hour) {
  const wind::Climate c = climate_of(level);
  double glaze = 0;
  for (int i = kGlazeSteps - 1; i >= 0; --i) {
    const double back = kStep * i;
    const double h = wrapped(hour - back);
    const double road = wind::surface_at(c, site, h);
    const Fall fall = fall_at(level, t - back * 3600, site.y, h);
    glaze += kStep * wind::glaze_on(road, fall.rain) * std::min(fall.fall * 4, 1.0) / kGlazeBuild;
    if (road > 0) glaze -= kStep * road / kGlazeMelt;
    glaze = std::clamp(glaze, 0.0, 1.0);
  }
  return glaze;
}

double glaze_at(const mapgen::Level& level, const wind::Site& site, double t) {
  return glaze_at(level, site, t, hour_at(level, t));
}

double lake_ice(const mapgen::Level& level, double y, double days) {
  const wind::Climate c = climate_of(level);
  return wind::ice_grown(0, c.mean_c - c.lapse * (y - c.ref_y), days);
}

}  // namespace ge::mountain::air
