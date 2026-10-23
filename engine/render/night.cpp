// The night's numbers (render/night.hpp says what each one is and where it comes from).
//
// ge:summary The moon's light, disc and phase, the key light's choice and the dark sky's glow
// ge:keywords render night moon phase-law surface-surge twilight key-light set_night
// ge:skill render
// ge:test tests/render_night_test.cpp
#include "render/night.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace ge::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

// The moon's sky's light on a level face beside its own direct light, a share of it.
constexpr double kMoonSky = 0.15;
// The sun's elevation under which no eye on a mountain sees it (a horizon seen from 4 km dips
// two degrees), rad.
constexpr double kSunGone = -3.0 * kDeg;

// The surge's table: every 2.5 degrees of phase angle from full to new.
constexpr double kSurgeStep = 2.5 * kDeg;
constexpr int kSurgeRows = 73;

// The disc's light under Lommel-Seeliger's law with the sun `a` rad off the eye, on a grid over
// the unit disc: the sum of mu0 / (mu0 + mu) where the sun lights it.
double lommel_seeliger(double a) {
  constexpr int n = 40;
  double sum = 0;
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const double u = (x + 0.5) / n * 2 - 1;
      const double v = (y + 0.5) / n * 2 - 1;
      const double r2 = u * u + v * v;
      if (r2 >= 1) continue;
      const double mu = std::sqrt(1 - r2);
      const double mu0 = u * std::sin(a) + mu * std::cos(a);
      if (mu0 > 0) sum += mu0 / (mu0 + mu);
    }
  }
  return sum;
}

std::array<double, kSurgeRows> surge_table() {
  std::array<double, kSurgeRows> t{};
  const double full = lommel_seeliger(0);
  for (int k = 0; k < kSurgeRows; ++k) {
    const double a = std::min(k * kSurgeStep, kPi - 1e-3);
    const double lit = lommel_seeliger(a);
    t[k] = lit > 0 ? phase_law(a) * full / lit : 1;
  }
  return t;
}

}  // namespace

double phase_law(double i) {
  const double d = std::min(180.0, std::fabs(i) / kDeg);
  return std::pow(10.0, -0.4 * (0.026 * d + 4e-9 * d * d * d * d));
}

double phase_angle(double lit) { return std::acos(std::clamp(2 * lit - 1, -1.0, 1.0)); }

double moon_lux(double i, double km) {
  const double near = kMoonMeanKm / km;
  return kMoonLux * phase_law(i) * near * near;
}

double moon_radius(double km) { return std::asin(kMoonRadiusKm / km); }

double surface_surge(double i) {
  static const std::array<double, kSurgeRows> table = surge_table();
  const double x = std::min(std::fabs(i), kPi) / kSurgeStep;
  const int k = std::min(kSurgeRows - 2, static_cast<int>(x));
  return table[k] + (table[k + 1] - table[k]) * (x - k);
}

double twilight_lux(double elevation) {
  // A sun up gives a level face thousands of lux; no moon competes with it.
  if (elevation >= 0) return 1e5;
  // log10 lx a degree under, from the horizon down: the sky tables' own twilight (the `night`
  // lab's `twilight` table holds them together), falling 1.7 decades a degree past the last.
  static constexpr double kLog[] = {3.023, 2.825,  2.541,  2.232,  1.873,  1.432,  0.861,  0.429,
                                    0.022, -0.535, -1.224, -1.706, -2.229, -2.948, -3.807, -5.5};
  constexpr int kLast = static_cast<int>(std::size(kLog)) - 1;
  const double x = -elevation / kDeg;
  if (x >= kLast) return std::pow(10.0, kLog[kLast] - 1.7 * (x - kLast));
  const int k = static_cast<int>(x);
  return std::pow(10.0, kLog[k] + (kLog[k + 1] - kLog[k]) * (x - k));
}

double moonlight_on_ground(double lux, double elevation) {
  if (elevation <= 0) return 0;
  // Kasten and Young's air mass, and a clear mountain night's extinction of 0.2 magnitudes an
  // air mass in the eye's band.
  const double deg = elevation / kDeg;
  const double air = 1 / (std::sin(elevation) + 0.50572 * std::pow(deg + 6.07995, -1.6364));
  return lux * std::sin(elevation) * std::pow(10.0, -0.4 * 0.2 * air);
}

double pre_exposure(const FrameView& view) {
  const Night& n = view.night;
  const double sun = n.moon_key ? n.sun_elevation : view.sun_elevation;
  if (sun >= 0) return 1;
  const double moonlight =
      moonlight_on_ground(n.moon_illuminance * kSunLux, n.moon_elevation) * (1 + kMoonSky);
  // The scene's own lights count as the illuminance that would light a white face to them.
  const double lux =
      std::max(twilight_lux(sun) + moonlight + n.dark_light * kSunLux, view.adapt_cd * kPi);
  return std::max(1.0, 0.25 * kPi * kSunLux / std::max(lux, 1e-4));
}

void set_night(FrameView& view, const Moon& moon, double latitude_deg, double sidereal_deg) {
  Night& n = view.night;
  const double i = phase_angle(moon.lit);
  const double r = moon_radius(moon.distance_km);
  const double near = kMoonMeanKm / moon.distance_km;
  // A full disc's mean luminance at this distance, the sun's units a steradian.
  const double disc = kMoonLux * near * near / kSunLux / (kPi * r * r);
  n.moon_elevation = moon.elevation;
  n.moon_azimuth = moon.azimuth;
  n.moon_radius = r;
  n.moon_surface = disc * surface_surge(i);
  n.earthshine = disc * kEarthshine * (1 - std::clamp(moon.lit, 0.0, 1.0));
  n.sun_elevation = view.sun_elevation;
  n.sun_azimuth = view.sun_azimuth;
  n.sun_illuminance = view.sun_illuminance;
  n.sidereal = sidereal_deg * kDeg;
  n.latitude = latitude_deg * kDeg;
  // By day the moon's light and its sky are nothing beside the day's: none of it is drawn but
  // its disc, pale in the blue.
  if (view.sun_elevation >= 0) return;
  const double lux = moon_lux(i, moon.distance_km);
  const double moonlight = moonlight_on_ground(lux, moon.elevation);
  n.moon_illuminance = lux / kSunLux;
  n.dark_sky = kDarkSkyCdm2 / kSunLux;
  n.dark_light = kDarkSkyLux / kSunLux;
  // The key: the moon once the sun is gone from every eye (even a peak's, whose horizon dips),
  // and the moon is up; the twilight's sky lights on beside it.
  n.moon_key = moon.elevation > 0 && view.sun_elevation < kSunGone;
  n.moon_haze = moonlight * kMoonSky > twilight_lux(view.sun_elevation);
  if (n.moon_key) {
    view.sun_elevation = moon.elevation;
    view.sun_azimuth = moon.azimuth;
    view.sun_illuminance = n.moon_illuminance;
  } else {
    n.other_light = moonlight / kSunLux;
  }
}

}  // namespace ge::render
