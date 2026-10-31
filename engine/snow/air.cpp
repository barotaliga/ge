// The snow's air: the layer's weather off the world's climate, new snow's density, the feel.
//
// ge:summary Definitions for the snow's air: air_c, air_schedule, weather_of, feel_at
// ge:keywords snow air weather climate fresh_density feel squeak glide
// ge:skill snow water
// ge:test tests/snow_air_test.cpp
#include "snow/air.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"

namespace ge::snow {
namespace {

constexpr double kDay = 86400;

// Day k's value of `f(climate, hour of day)`, the days meeting round midnight.
template <typename F>
double blended(const SnowAir& a, double t, F f) {
  if (a.days.empty()) return 0;
  const double h = a.start_hour + t / 3600;
  const int last = static_cast<int>(a.days.size()) - 1;
  const double k = std::floor(h / 24);
  const double hd = h - 24 * k;
  const int day = std::clamp(static_cast<int>(k), 0, last);
  const double here = f(a.days[day], hd);
  const double half = kDayBlendHours / 2;
  if (hd > 24 - half && day < last && k >= 0) {
    const double s = core::smoothstep(24 - half, 24 + half, hd);
    return core::lerp(here, f(a.days[day + 1], hd), s);
  }
  if (hd < half && day > 0 && k <= last) {
    const double s = core::smoothstep(-half, half, hd);
    return core::lerp(f(a.days[day - 1], hd), here, s);
  }
  return here;
}

}  // namespace

double air_c(const SnowAir& a, double t) {
  return blended(a, t, [&](const wind::Climate& c, double hour) {
    return wind::air_at(c, a.site, hour).temp_c;
  });
}

double snow_c(const SnowAir& a, double t) {
  wind::Site s = a.site;
  s.snow = 1;
  const double v = blended(
      a, t, [&](const wind::Climate& c, double hour) { return wind::surface_at(c, s, hour); });
  return std::min(v, 0.0);
}

Schedule air_schedule(const SnowAir& a, double seconds, double step) {
  Schedule s;
  const int n = std::max(1, static_cast<int>(std::ceil(seconds / step)));
  for (int i = 0; i <= n; i++) {
    const double t = std::min(seconds, i * step);
    s.t.push_back(t);
    s.v.push_back(air_c(a, t));
  }
  return s;
}

Schedule wind_schedule(const SnowAir& a, double seconds) {
  Schedule s;
  if (a.days.empty()) return s;
  const int last = static_cast<int>(a.days.size()) - 1;
  for (int k = 0; k <= last; k++) {
    // Day k's wind from its midnight (or the layer's start) to the next.
    const double from = std::max(0.0, (24 * k - a.start_hour) * 3600);
    const double to = (24 * (k + 1) - a.start_hour) * 3600 - 1;
    if (to < 0 && k < last) continue;
    if (from > seconds) break;
    s.t.push_back(from);
    s.v.push_back(a.days[k].wind);
    if (k < last && to > from) {
      s.t.push_back(to);
      s.v.push_back(a.days[k].wind);
    }
  }
  return s;
}

double fresh_density_over(const Schedule& fall, const Schedule& air, double t0, double t1) {
  if (t1 <= t0) return wind::fresh_density(value_at(air, t0));
  // Midpoints of 10-minute slices: the schedules are straight lines between their points, and
  // the density bends with the air, so a fine slice is the integral to a fraction of a kg/m3.
  const int n = std::max(1, static_cast<int>(std::ceil((t1 - t0) / 600)));
  const double dt = (t1 - t0) / n;
  double mass = 0, weighted = 0, plain = 0;
  for (int i = 0; i < n; i++) {
    const double t = t0 + (i + 0.5) * dt;
    const double rho = wind::fresh_density(value_at(air, t));
    const double m = std::max(value_at(fall, t), 0.0) * dt;
    mass += m;
    weighted += m * rho;
    plain += rho;
  }
  return mass > 0 ? weighted / mass : plain / n;
}

Weather weather_of(const SnowAir& a, const Schedule& fall, double seconds) {
  Weather w;
  w.fall = fall;
  w.air = air_schedule(a, seconds);
  w.wind = wind_schedule(a, seconds);
  w.fresh_density = fresh_density_over(w.fall, w.air, 0, seconds);
  return w;
}

SnowFeel feel_at(const SnowAir& a, const Weather& w, double t) {
  SnowFeel f;
  f.air_c = value_at(w.air, t);
  f.snow_c = snow_c(a, t);
  f.fresh = integral(w.fall, std::max(0.0, t - kDay), t) / w.fresh_density;
  // Thawed since it last froze: the air was above freezing at some moment, and less new snow
  // than makes the column powder again has fallen since.
  const double thaw = last_above(w.air, 0, t);
  f.thawed =
      thaw > -1e299 && integral(w.fall, std::max(thaw, 0.0), t) / w.fresh_density < w.powder_over;
  // Melting snow is wet: a share growing with the air over freezing, 4 C a slush.
  f.wet = f.snow_c > -0.05 && f.air_c > 0 ? core::clamp(f.air_c / 4, 0.0, 1.0) : 0;
  f.kind = wind::snow_feel(f.snow_c, f.air_c, f.thawed ? 0 : f.fresh, f.thawed);
  f.glide = wind::glide_of(f.snow_c, f.wet);
  f.squeak = wind::squeaks(f.snow_c);
  return f;
}

}  // namespace ge::snow
