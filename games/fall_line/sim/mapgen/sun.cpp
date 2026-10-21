// Port of game5 `engine/mapgen/sun.ts` (`declinationOf`, `sunWindow`, `dealSun`).
//
// ge:summary The day's draws, in the oracle's order
// ge:keywords sun dealSun declinationOf
// ge:oracle game5/engine/mapgen/sun.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#include "mapgen/sun.hpp"

#include "core/js.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace fall_line::mapgen {

namespace js = ge::core::js;
using ge::core::kPi;

double declination_of(double day_of_year) {
  return -23.44 * ge::core::libm::cos((2 * kPi * (day_of_year + 10)) / 365);
}

std::optional<ge::core::HourWindow> sun_window(double latitude, double day_of_year) {
  const auto w = ge::core::daylight_window(latitude, (rules::sun::min_elevation * kPi) / 180,
                                           declination_of(day_of_year));
  if (!w) return std::nullopt;
  const double min = js::max(w->min, rules::sun::hour.min);
  const double max = js::min(w->max, rules::sun::hour.max);
  if (max > min) return ge::core::HourWindow{min, max};
  return std::nullopt;
}

DealtSun deal_sun(ge::core::Rng& rng, Band latitude, Band day_of_year) {
  for (int i = 0; i < 64; i++) {
    const double lat = in_band(rng, latitude);
    const double day = js::round(in_band(rng, day_of_year));
    const auto w = sun_window(lat, day);
    if (!w) continue;
    return {ge::core::range(rng, w->min, w->max), day, lat};
  }
  // The band's own southern edge in its latest week always has a noon.
  return {12, day_of_year.max, latitude.min};
}

}  // namespace fall_line::mapgen
