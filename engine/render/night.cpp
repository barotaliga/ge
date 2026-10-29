// The night's numbers (render/night.hpp says what each one is and where it comes from).
//
// ge:summary The moon's light, disc and phase, the key light's choice and the dark sky's glow
// ge:keywords render night moon phase-law surface-surge twilight key-light set_night eclipse
// ge:keywords set_moons key_moon other-moons orbit libration
// ge:skill render
// ge:test tests/render_night_test.cpp
#include "render/night.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

#include "render/moon.hpp"
#include "render/moons.hpp"

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

// The epoch other worlds' moons are placed from (jd; render/moons.hpp's epoch_deg).
constexpr double kMoonEpoch = 2451545.0;

// The umbra's light at its axis against full sunlight; ten times it at its edge.
constexpr double kUmbraLight = 1.0e-4;

void normalise(double v[3]) {
  const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  for (int k = 0; k < 3; ++k) v[k] /= l;
}
void cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}
double dot(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

}  // namespace

Moon moon_of(const core::MoonSky& sky) {
  Moon m{sky.apparent_elevation, sky.azimuth, sky.lit, sky.distance_km};
  const core::Vec3d* from[] = {&sky.pole, &sky.prime, &sky.east, &sky.shadow};
  double* to[] = {m.pole, m.prime, m.east, m.shadow};
  for (int i = 0; i < 4; ++i) {
    to[i][0] = from[i]->x;
    to[i][1] = from[i]->y;
    to[i][2] = from[i]->z;
  }
  m.umbra = sky.umbra;
  m.penumbra = sky.penumbra;
  return m;
}

ShadowLight shadow_light(double s, double umbra, double penumbra) {
  if (penumbra <= 0 || s >= penumbra) return {};
  // Across the penumbra the Earth's limb crosses the sun's disc: t from -1 (all of it hidden) to
  // 1 (none), the share left the circle's segment past the limb.
  const double t = std::clamp(2 * (s - umbra) / (penumbra - umbra) - 1, -1.0, 1.0);
  const double sun = 1 - (std::acos(t) - t * std::sqrt(1 - t * t)) / kPi;
  const double red = kUmbraLight * std::pow(10.0, std::min(s, umbra) / umbra);
  return {sun, (1 - sun) * red};
}

Eclipse eclipse_of(const double toward[3], double radius, const double shadow[3], double umbra,
                   double penumbra) {
  Eclipse e;
  if (penumbra <= 0 || radius <= 0) return e;
  const double centre = std::acos(std::clamp(dot(toward, shadow), -1.0, 1.0));
  if (centre - radius >= penumbra) return e;
  // Two directions across the disc, then a grid over it.
  double a[3], b[3];
  const double up[3] = {0, 1, 0}, side[3] = {1, 0, 0};
  cross(toward, std::fabs(toward[1]) < 0.9 ? up : side, a);
  normalise(a);
  cross(toward, a, b);
  constexpr int n = 24;
  double sum = 0, peak = 0;
  int count = 0;
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const double u = (x + 0.5) / n * 2 - 1, v = (y + 0.5) / n * 2 - 1;
      if (u * u + v * v >= 1) continue;
      double d[3];
      for (int k = 0; k < 3; ++k) d[k] = toward[k] + (a[k] * u + b[k] * v) * radius;
      normalise(d);
      const double s = std::acos(std::clamp(dot(d, shadow), -1.0, 1.0));
      const ShadowLight sl = shadow_light(s, umbra, penumbra);
      const double light = sl.sun + sl.air;
      sum += light;
      peak = std::max(peak, light);
      ++count;
    }
  }
  e.mean = sum / count;
  e.peak = peak;
  return e;
}

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
  // The lamps the eye has adapted to count as the sky's light does, or a lit room at night would
  // be stored past what a half float holds (a white face under 185 lx at a moonless night's scale
  // is 4.6e5) and its brightest pixels lost.
  const double lux = twilight_lux(sun) + moonlight + n.dark_light * kSunLux + view.lamp_lux;
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
  // The face: as placed, or the mean near side square to the eye, its pole toward the sky's.
  double toward[3];
  look_dir(moon.azimuth, moon.elevation, toward);
  if (dot(moon.pole, moon.pole) > 0.5) {
    for (int k = 0; k < 3; ++k) {
      n.moon_pole[k] = moon.pole[k];
      n.moon_prime[k] = moon.prime[k];
      n.moon_east[k] = moon.east[k];
    }
  } else {
    const core::Vec3d p = core::equatorial_to_world({0, 0, 1}, sidereal_deg, latitude_deg);
    double pole[3] = {p.x, p.y, p.z};
    const double along = dot(pole, toward);
    for (int k = 0; k < 3; ++k) {
      pole[k] -= toward[k] * along;
      n.moon_prime[k] = -toward[k];
    }
    normalise(pole);
    for (int k = 0; k < 3; ++k) n.moon_pole[k] = pole[k];
    // East of the prime meridian seen from the eye: to the left of the pole's up, as the sky's
    // east is (the world's axes are left-handed, so the cross product turns the other way).
    cross(n.moon_prime, n.moon_pole, n.moon_east);
  }
  n.albedo_scale = moon_albedo_scale();
  // The Earth's shadow on it.
  for (int k = 0; k < 3; ++k) n.shadow[k] = moon.shadow[k];
  n.umbra = moon.umbra;
  n.penumbra = moon.penumbra;
  const Eclipse eclipse = eclipse_of(toward, r, moon.shadow, moon.umbra, moon.penumbra);
  n.eclipse_peak = eclipse.peak;
  n.sun_elevation = view.sun_elevation;
  n.sun_azimuth = view.sun_azimuth;
  n.sun_illuminance = view.sun_illuminance;
  n.sidereal = sidereal_deg * kDeg;
  n.latitude = latitude_deg * kDeg;
  key_moon(view, moon.elevation, moon.azimuth, moon_lux(i, moon.distance_km) * eclipse.mean);
}

void key_moon(FrameView& view, double elevation, double azimuth, double lux) {
  Night& n = view.night;
  n.moon_elevation = elevation;
  n.moon_azimuth = azimuth;
  // By day the moon's light and its sky are nothing beside the day's: none of it is drawn but
  // its disc, pale in the blue.
  if (view.sun_elevation >= 0) return;
  const double moonlight = moonlight_on_ground(lux, elevation);
  n.moon_illuminance = lux / kSunLux;
  n.dark_sky = kDarkSkyCdm2 / kSunLux;
  n.dark_light = kDarkSkyLux / kSunLux;
  // The key: the moon once the sun is gone from every eye (even a peak's, whose horizon dips),
  // and the moon is up; the twilight's sky lights on beside it.
  n.moon_key = elevation > 0 && view.sun_elevation < kSunGone;
  n.moon_haze = moonlight * kMoonSky > twilight_lux(view.sun_elevation);
  if (n.moon_key) {
    view.sun_elevation = elevation;
    view.sun_azimuth = azimuth;
    view.sun_illuminance = n.moon_illuminance;
  } else {
    n.other_light = moonlight / kSunLux;
  }
}

void set_moons(FrameView& view, double jd) {
  Night& n = view.night;
  const MoonSet& set = view.moons;
  n.moon_count = 0;
  // The real sun, whichever light keys.
  const double sun_el = n.moon_key ? n.sun_elevation : view.sun_elevation;
  const double sun_az = n.moon_key ? n.sun_azimuth : view.sun_azimuth;
  double sun[3];
  look_dir(sun_az, sun_el, sun);
  const double sidereal_deg = n.sidereal / kDeg, latitude_deg = n.latitude / kDeg;
  const auto world = [&](const double e[3], double out[3]) {
    const core::Vec3d w = core::equatorial_to_world({e[0], e[1], e[2]}, sidereal_deg, latitude_deg);
    out[0] = w.x;
    out[1] = w.y;
    out[2] = w.z;
  };
  const std::span<const MoonKind> kinds = moon_kinds();
  const double days = jd - kMoonEpoch;
  for (int k = 0; k < set.count && n.moon_count < kMaxMoons; ++k) {
    const MoonPick& pick = set.moons[k];
    if (pick.kind < 0 || pick.kind >= static_cast<int>(kinds.size())) continue;
    const MoonKind& kind = kinds[static_cast<size_t>(pick.kind)];
    SkyMoon& m = n.moons[n.moon_count++];
    m.kind = pick.kind;
    // Its orbit: a circle tilted by its inclination to the celestial equator, its node at the
    // equinox; `u` its angle along it from the node.
    const double u = (kind.epoch_deg + 360.0 * std::fmod(days / kind.period_days, 1.0)) * kDeg;
    const double inc = kind.inclination_deg * kDeg;
    const double a1[3] = {1, 0, 0};
    const double a2[3] = {0, std::cos(inc), std::sin(inc)};
    const double normal[3] = {0, -std::sin(inc), std::cos(inc)};
    double e[3];
    for (int c = 0; c < 3; ++c) e[c] = std::cos(u) * a1[c] + std::sin(u) * a2[c];
    world(e, m.toward);
    world(normal, m.pole);
    // Its spin: locked, its 0 N 0 E toward its planet (so toward the eye) rocking by its optical
    // libration; free, turning at its own rate from the epoch.
    double phi = u + kPi + 2 * kind.eccentricity * std::sin(u);
    if (pick.spin_hours > 0) phi = 2 * kPi * std::fmod(days * 24.0 / pick.spin_hours, 1.0);
    for (int c = 0; c < 3; ++c) e[c] = std::cos(phi) * a1[c] + std::sin(phi) * a2[c];
    world(e, m.prime);
    m.elevation = std::asin(std::clamp(m.toward[1], -1.0, 1.0));
    m.azimuth = std::atan2(m.toward[0], m.toward[2]);
    // Its size: the long semi-axis's (and the haze over it) as drawn; its light by its mean one.
    const double most = std::max({kind.axes_km[0], kind.axes_km[1], kind.axes_km[2]});
    m.radius = std::asin(std::min(most * (1 + kind.haze) / kind.height_km, 1.0)) * pick.size;
    const double r = moon_kind_radius(kind) * pick.size;
    // Its phase: the angle at it between the sun and the eye.
    m.phase = std::acos(std::clamp(-dot(sun, m.toward), -1.0, 1.0));
    m.surface = kind.albedo / kPi * (surface_surge(m.phase) * (1 - kind.lambert) + kind.lambert);
    m.lux = kSunLux * kind.albedo * r * r * phase_law(m.phase);
  }
  // The farther first, so a nearer one is drawn over it.
  std::stable_sort(n.moons, n.moons + n.moon_count, [&](const SkyMoon& a, const SkyMoon& b) {
    return kinds[static_cast<size_t>(a.kind)].height_km >
           kinds[static_cast<size_t>(b.kind)].height_km;
  });
  if (set.earth) return;
  // No Earth's moon: its disc goes, and the others light the night in its place.
  if (n.moon_key) {
    view.sun_elevation = n.sun_elevation;
    view.sun_azimuth = n.sun_azimuth;
    view.sun_illuminance = n.sun_illuminance;
  }
  n.moon_key = false;
  n.moon_haze = false;
  n.moon_illuminance = 0;
  n.other_light = 0;
  n.moon_radius = 0;
  n.earthshine = 0;
  n.eclipse_peak = 1;
  n.umbra = n.penumbra = 0;
  const SkyMoon* best = nullptr;
  double lux = 0;
  for (int k = 0; k < n.moon_count; ++k) {
    const SkyMoon& m = n.moons[k];
    if (m.elevation <= 0) continue;
    lux += m.lux;
    if (!best ||
        moonlight_on_ground(m.lux, m.elevation) > moonlight_on_ground(best->lux, best->elevation))
      best = &m;
  }
  if (!best) {
    key_moon(view, -1.0, 0, 0);
    return;
  }
  // Its surface's brightness is the one a cloud's glow round the key is seen beside.
  n.moon_surface = best->surface;
  key_moon(view, best->elevation, best->azimuth, lux);
}

}  // namespace ge::render
