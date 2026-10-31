// The air's temperature and moisture: the day's curve, the cold pool, the dew point and the
// moisture sources, Magnus, the wet bulb, the surface, the freezing and snow levels.
//
// ge:summary The air's temperature and moisture: the formulas behind climate.hpp
// ge:keywords climate temperature dew-point wet-bulb magnus psychrometric diurnal cold-pool
// ge:skill water determinism
// ge:test tests/wind_climate_test.cpp
#include "wind/climate.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {

namespace lm = core::libm;

constexpr double kPi = 3.14159265358979323846;
constexpr double kPeakLag = 2;        // h: the high comes this long after solar noon
constexpr double kNightDecay = 2.2;   // the night's exponential fall, over the night's length
constexpr double kPoolPerM = 0.02;    // K of the night's cold pool a metre of hollow
constexpr double kPoolMost = 12;      // K
constexpr double kLakeReach = 300;    // m: a lake tempers the air this far (1/e)
constexpr double kLakeTemper = 0.3;   // the share of the gap to the water's temperature over it
constexpr double kDampReach = 150;    // m: a lake moistens the air this far (1/e)
constexpr double kDampShare = 0.7;    // the share of the gap to a wet surface's saturation taken
constexpr double kSurfaceLayer = 50;  // m: a surface's warmth and damp fade upward this fast (1/e)
constexpr double kNightCool = 4;      // K: a clear, still night's surface under the air
constexpr double kSunWarm = 5;        // K: a clear noon's bare ground over the air

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double smooth(double t) {
  const double s = clamp01(t);
  return s * s * (3 - 2 * s);
}

double wrap24(double h) {
  const double w = std::fmod(h, 24.0);
  return w < 0 ? w + 24 : w;
}

double day_length(const Climate& c) { return std::clamp(wrap24(c.sunset - c.sunrise), 1.0, 23.0); }

// Hours since sunrise, 0..24.
double since_sunrise(const Climate& c, double hour) { return wrap24(hour - c.sunrise); }

// The day's curve: the midpoint `mean` and the swing `amp` at an hour.
double curve(const Climate& c, double mean, double amp, double hour) {
  const double len = day_length(c);
  const double low = mean - 0.5 * amp;
  const double d = since_sunrise(c, hour);
  const double span = len + 2 * kPeakLag;
  if (d <= len) return low + amp * lm::sin(kPi * d / span);
  const double at_set = low + amp * lm::sin(kPi * len / span);
  const double night = 24 - len;
  const double tail = lm::exp(-kNightDecay);
  const double fall = (lm::exp(-kNightDecay * (d - len) / night) - tail) / (1 - tail);
  return low + (at_set - low) * fall;
}

// The ground's surface given the air over it.
double surface_from(const Climate& c, const Site& s, double hour, double air) {
  if (s.water_m <= 0) return c.water_c;
  const double clear = 1 - clamp01(c.cloud);
  const double open = clamp01(s.sky);
  const double len = day_length(c);
  const double d = since_sunrise(c, hour);
  double t = air;
  // Radiating to the sky from an hour after sunset, damped by the wind's mixing, and fading over
  // the two hours after sunrise as the sun comes onto it (a surface does not jump at dawn).
  const double cooled = kNightCool * clear * clear * open / (1 + c.wind / 4);
  if (d <= len) {
    t += kSunWarm * lm::sin(kPi * d / len) * clear * open * (1 - clamp01(s.snow));
    t -= cooled * (1 - smooth(d / 2));
  } else {
    t -= cooled * smooth(d - len);
  }
  const double snow = clamp01(s.snow);
  return t + (std::min(t, 0.0) - t) * snow;
}

// The vapour pressure, hPa, at which air at `e` hPa saturates over ice: the frost point.
double frost_point_of(double e) {
  const double g = lm::log(e / 6.1121);
  return 273.86 * g / (22.587 - g);
}

}  // namespace

double saturation_hpa(double temp_c) {
  return 6.1094 * lm::exp(17.625 * temp_c / (temp_c + 243.04));
}

double saturation_ice_hpa(double temp_c) {
  return 6.1121 * lm::exp(22.587 * temp_c / (temp_c + 273.86));
}

double dew_point_of(double temp_c, double rh) {
  const double g = lm::log(std::clamp(rh, 1e-4, 1.0)) + 17.625 * temp_c / (243.04 + temp_c);
  return 243.04 * g / (17.625 - g);
}

double pressure_at(double y) { return 1013.25 * lm::exp(-y / 8434); }

double wet_bulb_of(double temp_c, double rh, double pressure_hpa) {
  // The psychrometric balance: the vapour the air holds is what a wet bulb at Tw saturates less
  // the heat its evaporation drew, e = e_s(Tw) - A p (T - Tw), A = 6.6e-4 (1 + 0.00115 Tw) / K.
  const double e = saturation_hpa(temp_c) * std::clamp(rh, 1e-4, 1.0);
  const auto balance = [&](double tw) {
    return saturation_hpa(tw) - 6.6e-4 * (1 + 0.00115 * tw) * pressure_hpa * (temp_c - tw) - e;
  };
  double lo = temp_c - 40, hi = temp_c;
  for (int k = 0; k < 60; ++k) {
    const double mid = 0.5 * (lo + hi);
    (balance(mid) > 0 ? hi : lo) = mid;
  }
  return 0.5 * (lo + hi);
}

double swing_of(const Climate& c, const Site& s) {
  return (1 - 0.7 * clamp01(c.cloud)) / (1 + std::max(c.wind, 0.0) / 8) *
         (0.15 + 0.85 * clamp01(s.sky));
}

double night_chill(const Climate& c, double hour) {
  const double clear = 1 - clamp01(c.cloud);
  const double calm = 1 / (1 + (c.wind / 2.5) * (c.wind / 2.5));
  const double len = day_length(c);
  const double d = since_sunrise(c, hour);
  const double built = d > len ? smooth((d - len) / 3) : 1 - smooth(d / 2);
  return clear * clear * calm * built;
}

Thermal air_at(const Climate& c, const Site& s, double hour) {
  double t = curve(c, c.mean_c, c.range_c * swing_of(c, s), hour);
  t -= c.lapse * (s.y - c.ref_y);
  t -= std::min(kPoolMost, kPoolPerM * std::max(s.hollow, 0.0)) * night_chill(c, hour) *
       clamp01(s.sky);
  // What a surface lends the air fades with the height over it.
  const double layer = lm::exp(-std::max(s.above - 2, 0.0) / kSurfaceLayer);
  const double lake = lm::exp(-std::max(s.water_m, 0.0) / kLakeReach) * layer;
  t += (c.water_c - t) * kLakeTemper * lake;
  t += s.warmth;

  double dew = dew_point_of(c.mean_c, c.humidity) - kDewLapse * (s.y - c.ref_y);
  // Wet surfaces evaporate into the air toward their own saturation, never past the air's.
  const auto moisten = [&](double source_c, double weight) {
    const double top = std::min(source_c, t);
    if (top > dew) dew += (top - dew) * kDampShare * clamp01(weight);
  };
  moisten(c.water_c, lm::exp(-std::max(s.water_m, 0.0) / kDampReach) * layer);
  moisten(surface_from(c, s, hour, t), s.wet * layer);
  dew = std::min(dew, t);

  Thermal out;
  out.temp_c = t;
  out.dew_c = dew;
  const double e = saturation_hpa(dew);
  out.rh = clamp01(e / saturation_hpa(t));
  out.frost_c = std::min(frost_point_of(e), t);
  out.wet_bulb_c = wet_bulb_of(t, out.rh, pressure_at(s.y));
  return out;
}

double surface_at(const Climate& c, const Site& s, double hour) {
  return surface_from(c, s, hour, air_at(c, s, hour).temp_c);
}

double rain_share_of(const Thermal& air) { return smooth((air.wet_bulb_c + 0.5) / 3); }

double freezing_level(const Climate& c, double hour) {
  const double at_ref = curve(c, c.mean_c, c.range_c * swing_of(c, Site{}), hour);
  if (c.lapse <= 0) return at_ref > 0 ? 1e9 : -1e9;
  return c.ref_y + at_ref / c.lapse;
}

double snow_level(const Climate& c, double hour) {
  const auto wet_bulb = [&](double y) {
    Site s;
    s.y = y;
    return air_at(c, s, hour).wet_bulb_c - 1;
  };
  double lo = c.ref_y - 10000, hi = c.ref_y + 10000;
  if (wet_bulb(lo) <= 0) return lo;  // snow to the lowest ground
  if (wet_bulb(hi) > 0) return hi;   // rain to the highest
  for (int k = 0; k < 60; ++k) {
    const double mid = 0.5 * (lo + hi);
    (wet_bulb(mid) > 0 ? lo : hi) = mid;
  }
  return 0.5 * (lo + hi);
}

}  // namespace ge::wind
