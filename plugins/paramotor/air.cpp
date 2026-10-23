// The air a paramotor's wing flies in (air.hpp).
//
// ge:summary The paramotor's air: the log law, the exposure, ridge lift, the lee, the eddies
// ge:keywords paramotor air wind ridge-lift lee eddies para_air_at eddy_up make_air_field
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#include "paramotor/air.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/rules.hpp"
#include "paramotor/spec.hpp"

namespace ge::paramotor {

namespace {

namespace mg = mountain::mapgen;
namespace lm = core::libm;
namespace num = core::num;
namespace A = spec::air;

// A share in [0, 1) dealt off the seed and a salt.
double dealt(double seed, double salt) { return core::hash2(salt, 7, seed); }

// The log law's share of the 10 m wind at `height` m over the snow.
double profile_at(double height) {
  const double z0 = A::kRoughness;
  return lm::log(num::max(2 * z0, height) / z0) / lm::log(A::kRef / z0);
}

// One component of the eddies at a point, in units of sigma: frozen into the wind and carried
// down it.
double eddy(const ParaAirField& f, int c, const ParaWind& w, double t, double x, double y,
            double z) {
  const double px = x - w.x * t;
  const double pz = z - w.z * t;
  double sum = 0;
  for (const EddyWave& e : f.waves[c]) {
    sum += e.amp * lm::sin(e.kx * px + e.ky * y + e.kz * pz + e.omega * t + e.phase);
  }
  return sum;
}

}  // namespace

ParaWind level_wind(const mg::Level& level) {
  const double from = level.weather.wind_from;
  return ParaWind{-lm::sin(from) * level.weather.wind, -lm::cos(from) * level.weather.wind};
}

ParaAirField make_air_field(double seed) {
  ParaAirField f;
  constexpr int kOctaves = 3;
  constexpr int kPer = kWavesPerComponent / kOctaves;
  // An eddy's speed grows as its size to the third (Kolmogorov); the amplitudes normed so each
  // component's variance is one.
  double raw[kOctaves];
  double sum = 0;
  for (int o = 0; o < kOctaves; o++) {
    raw[o] = lm::cbrt(A::kWavelengths[o]);
    sum += raw[o] * raw[o];
  }
  const double norm = std::sqrt(2 / (kPer * sum));
  for (int c = 0; c < 3; c++) {
    for (int o = 0; o < kOctaves; o++) {
      for (int k = 0; k < kPer; k++) {
        const double salt = 101 + c * 37 + o * 7 + k;
        const double az = dealt(seed, salt) * core::kTau;
        const double el = (dealt(seed, salt + 500) - 0.5) * 0.9;
        const double n = core::kTau / A::kWavelengths[o];
        EddyWave& e = f.waves[c][o * kPer + k];
        e.kx = lm::cos(el) * lm::sin(az) * n;
        e.ky = lm::sin(el) * n;
        e.kz = lm::cos(el) * lm::cos(az) * n;
        // An eddy turns over in about the time the wind takes to cross it a few times.
        e.omega = n * A::kChurn;
        e.phase = dealt(seed, salt + 900) * core::kTau;
        e.amp = raw[o] * norm;
      }
    }
  }
  return f;
}

ParaAir para_air_at(const mg::Level& level, const ParaAirField& field, const ParaWind& wind,
                    double t, double x, double y, double z) {
  ParaAir out;
  const double agl = num::max(1.0, y - level.ground_at(x, z));
  // THE MEAN WIND: the 10 m wind up the log law to the layer's top, exposed by the air's own
  // altitude up the mountain.
  const mg::Mountain& m = level.mountain;
  const double up = m.vertical > 0 ? core::clamp((y - m.base.y) / m.vertical, 0.0, 1.3) : 0.5;
  const double exposed = A::kValley + (A::kSummit - A::kValley) * up;
  const double k = profile_at(num::min(agl, A::kTop)) * exposed;
  const double wx = wind.x * k;
  const double wz = wind.z * k;
  const double u = core::hypot(wx, wz);
  // THE RISE AND FALL: the flow along the slopes it meets.
  double lift = 0;
  double lee = 0;
  for (const A::Ridge& r : A::kRidges) {
    const double gx =
        (level.ground_at(x + r.span, z) - level.ground_at(x - r.span, z)) / (2 * r.span);
    const double gz =
        (level.ground_at(x, z + r.span) - level.ground_at(x, z - r.span)) / (2 * r.span);
    const double along = core::clamp(wx * gx + wz * gz, -u * A::kSteepest, u * A::kSteepest);
    const double fade = lm::exp(-agl / r.depth);
    if (along >= 0) {
      lift += r.share * along * fade;
    } else {
      // Down a slope the flow tears off it: part of it a smooth sink, the rest the rotor.
      lift += r.share * along * A::kSeparate * fade;
      lee = num::max(lee, core::clamp(-along / num::max(1.0, u) / A::kLeeSlope, 0.0, 1.0) * fade);
    }
  }
  // THE EDDIES: their strength off the mean wind, the snow, the lee and the storm.
  const double storm = level.weather.kind == mg::WeatherKind::storm ? A::kStorm : 1.0;
  const double rough =
      u * storm * (A::kOpen + A::kGround * lm::exp(-agl / A::kGroundDepth) + A::kLee * lee);
  const double flat = core::clamp(agl / A::kFlat, 0.0, 1.0);
  out.x = wx + (rough > 0 ? rough * eddy(field, 0, wind, t, x, y, z) : 0.0);
  out.y = lift + (rough > 0 ? rough * flat * eddy(field, 1, wind, t, x, y, z) : 0.0);
  out.z = wz + (rough > 0 ? rough * eddy(field, 2, wind, t, x, y, z) : 0.0);
  out.mean = u;
  out.lift = lift;
  out.rough = rough;
  out.lee = lee;
  return out;
}

double eddy_up(const mg::Level& level, const ParaAirField& field, const ParaWind& wind, double t,
               double x, double y, double z, double rough) {
  if (rough <= 0) return 0;
  const double agl = num::max(1.0, y - level.ground_at(x, z));
  return rough * core::clamp(agl / A::kFlat, 0.0, 1.0) * eddy(field, 1, wind, t, x, y, z);
}

}  // namespace ge::paramotor
