// The cold's marks: the deposit's rates, the glaze, the breath's mixing line, Stefan's ice and
// Gold's bearing, the snow's feel, density and glide.
//
// ge:summary The cold's marks: the formulas behind cold.hpp
// ge:keywords frost dew rime glaze breath mixing-line ice stefan gold snow-feel glide density
// ge:skill water determinism
// ge:test tests/wind_cold_test.cpp
#include "wind/cold.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {

namespace lm = core::libm;

constexpr double kDepositPerK = 0.03;  // mm/h a kelvin under the dew or frost point
constexpr double kRimeRate = 0.2;      // mm/h in a thick freezing fog in a 5 m/s wind
constexpr double kFrostMelt = 0.3;     // mm/h a kelvin over 0 C
constexpr double kBreathC = 34;        // C, exhaled air (saturated)
constexpr double kIceRoot = 0.024;     // m per root kelvin-day (Stefan)
constexpr double kIceMelt = 0.006;     // m a kelvin-day of thaw
constexpr double kGoldA = 1.75;        // kg/cm2 (Gold's A)

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double smooth(double t) {
  const double s = clamp01(t);
  return s * s * (3 - 2 * s);
}

// The most liquid water, g/m3, a mix of breath and the air at `temp_c` holding vapour `e` hPa
// condenses (0 when no mix is supersaturated).
double breath_water(double temp_c, double e) {
  const double e_breath = saturation_hpa(kBreathC);
  double most = 0;
  for (int k = 1; k <= 64; ++k) {
    const double f = k / 64.0;  // the breath's share of the mix
    const double t = temp_c + f * (kBreathC - temp_c);
    const double excess = e + f * (e_breath - e) - saturation_hpa(t);
    // hPa to g/m3: e / (R_v T), R_v = 461.5 J/(kg K).
    most = std::max(most, excess * 100 / (461.5 * (t + 273.15)) * 1000);
  }
  return most;
}

}  // namespace

const char* deposit_name(Deposit d) {
  switch (d) {
    case Deposit::none:
      return "none";
    case Deposit::dew:
      return "dew";
    case Deposit::frost:
      return "frost";
    case Deposit::rime:
      return "rime";
  }
  return "none";
}

Settling deposit_on(const Thermal& air, double surface_c, const Mist& mist, double wind) {
  if (mist.freezing && mist.strength >= kNoMist && wind > 1 && surface_c < 0)
    return {Deposit::rime, kRimeRate * mist.strength * std::min(wind / 5, 2.0)};
  if (surface_c < 0 && surface_c < air.frost_c)
    return {Deposit::frost, kDepositPerK * (air.frost_c - surface_c)};
  if (surface_c < air.dew_c && air.dew_c > 0)
    return {Deposit::dew, kDepositPerK * (air.dew_c - std::max(surface_c, 0.0))};
  return {};
}

double frost_loss(const Thermal& air, double surface_c) {
  if (surface_c > 0)
    return kFrostMelt * surface_c + kDepositPerK * std::max(surface_c - air.frost_c, 0.0);
  return kDepositPerK * std::max(surface_c - air.frost_c, 0.0);
}

double glaze_on(double surface_c, double rain) { return clamp01(rain) * smooth(-surface_c / 2); }

double breath_of(const Thermal& air) {
  // A cloud too thin to see condenses in warm, damp air too; what shows is the water past what a
  // 10 C, near-saturated air's mix holds, against a -10 C winter's.
  const double faint = breath_water(10, saturation_hpa(dew_point_of(10, 0.95)));
  const double reference = breath_water(-10, saturation_hpa(dew_point_of(-10, 0.8)));
  const double water = breath_water(air.temp_c, saturation_hpa(air.dew_c));
  return std::max(0.0, water - faint) / (reference - faint);
}

double ice_grown(double h0, double mean_c, double days) {
  const double h = std::max(h0, 0.0);
  if (mean_c < 0) return std::sqrt(h * h + kIceRoot * kIceRoot * -mean_c * std::max(days, 0.0));
  return std::max(0.0, h - kIceMelt * mean_c * std::max(days, 0.0));
}

double ice_bears(double h) {
  const double cm = std::max(h, 0.0) * 100;
  return kGoldA * cm * cm;
}

const char* feel_name(Feel f) {
  switch (f) {
    case Feel::powder:
      return "powder";
    case Feel::squeaky:
      return "squeaky";
    case Feel::soft:
      return "soft";
    case Feel::sticky:
      return "sticky";
    case Feel::slush:
      return "slush";
    case Feel::crust:
      return "crust";
  }
  return "soft";
}

Feel snow_feel(double snow_c, double air_c, double fresh, bool thawed) {
  if (snow_c > -0.05 && air_c > 0) return Feel::slush;
  if (fresh >= 0.05) {
    if (air_c < -6) return Feel::powder;
    return air_c > -2 ? Feel::sticky : Feel::soft;
  }
  if (thawed && snow_c < -0.5) return Feel::crust;
  if (squeaks(snow_c)) return Feel::squeaky;
  return snow_c > -1 && air_c > -1 ? Feel::sticky : Feel::soft;
}

double fresh_density(double air_c) {
  if (air_c <= 0) return 67.92 + 51.25 * lm::exp(air_c / 2.59);
  return std::min(200.0, 119.17 + 20 * std::sqrt(air_c));
}

double glide_of(double snow_c, double wet) {
  const double t = std::min(snow_c, 0.0);
  double mu = 0.025;
  if (t < -3) {
    mu += 0.00018 * (-3 - t) * (-3 - t);
  } else {
    mu += 0.01 * smooth((t + 3) / 3);
  }
  return std::min(0.3, mu + 0.03 * clamp01(wet));
}

}  // namespace ge::wind
