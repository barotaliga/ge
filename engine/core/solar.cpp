// The solar and lunar geometry; see solar.hpp. Statement order and operand order are the ones
// tests/golden/core_solar.txt was cut under: a reordered sum is a different double.
//
// ge:summary The solar and lunar geometry behind solar.hpp, in its fixed operation order
// ge:keywords solar sun moon julian-day sidereal ephemeris refraction
// ge:skill determinism
// ge:test tests/core_solar_test.cpp
#include "core/solar.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/num.hpp"

namespace ge::core {
namespace {

constexpr double kDeg = kPi / 180.0;

// How finely crossings are searched, hours: three minutes of clock.
constexpr double kStep = 1.0 / 20.0;

double clamp_unit(double v) { return v < -1.0 ? -1.0 : v > 1.0 ? 1.0 : v; }

double sin_d(double x) { return libm::sin(x * kDeg); }
double cos_d(double x) { return libm::cos(x * kDeg); }
double tan_d(double x) { return libm::tan(x * kDeg); }
double wrap360(double x) { return num::mod(num::mod(x, 360.0) + 360.0, 360.0); }

// Greenwich mean sidereal time at jd, degrees.
double sidereal_time(double jd) {
  const double t = (jd - 2451545.0) / 36525.0;
  return wrap360(280.46061837 + 360.98564736629 * (jd - 2451545.0) + t * t * 0.000387933);
}

struct Horizontal {
  double elevation;
  double azimuth;
};

// A body at `declination` and local `hour_angle` (degrees) seen from `latitude`.
Horizontal horizontal_of(double declination, double hour_angle, double latitude) {
  const double sin_el = sin_d(latitude) * sin_d(declination) +
                        cos_d(latitude) * cos_d(declination) * cos_d(hour_angle);
  const double elevation = libm::asin(clamp_unit(sin_el));
  const double from_south =
      libm::atan2(sin_d(hour_angle),
                  cos_d(hour_angle) * sin_d(latitude) - tan_d(declination) * cos_d(latitude));
  return Horizontal{elevation, kSouth + from_south};
}

}  // namespace

SunPlace sun_at(double hour, double latitude, double declination) {
  const double lat = latitude * kDeg;
  const double dec = declination * kDeg;
  const double h = (num::mod(num::mod(hour, 24.0) + 24.0, 24.0) - 12.0) * 15.0 * kDeg;
  const double sin_el =
      libm::sin(lat) * libm::sin(dec) + libm::cos(lat) * libm::cos(dec) * libm::cos(h);
  const double elevation = libm::asin(clamp_unit(sin_el));
  // Measured from the south, positive toward the west: negative all morning.
  const double from_south =
      libm::atan2(libm::sin(h), libm::cos(h) * libm::sin(lat) - libm::tan(dec) * libm::cos(lat));
  return SunPlace{elevation, kSouth + from_south, h < 0, hour};
}

std::optional<double> hour_of_elevation(double elevation, bool rising, double latitude,
                                        double declination) {
  const double from = rising ? 0.0 : 12.0;
  double was = sun_at(from, latitude, declination).elevation - elevation;
  // The scan ACCUMULATES h in floating point (h += kStep), and the hours it visits are those
  // bits, not from + i * kStep. It is a while loop because clang-tidy rejects a floating-point
  // `for` counter.
  double h = from + kStep;
  while (h <= from + 12.0 + 1e-9) {
    const double now = sun_at(h, latitude, declination).elevation - elevation;
    if ((rising && was < 0 && now >= 0) || (!rising && was > 0 && now <= 0)) {
      const double f = was / (was - now);  // a twentieth of an hour of a cosine is straight
      return h - kStep + f * kStep;
    }
    was = now;
    h += kStep;
  }
  return std::nullopt;
}

std::optional<HourWindow> daylight_window(double latitude, double min_elevation,
                                          double declination) {
  const double noon = sun_at(12.0, latitude, declination).elevation;
  if (noon < min_elevation) return std::nullopt;
  const auto up = hour_of_elevation(min_elevation, true, latitude, declination);
  const auto down = hour_of_elevation(min_elevation, false, latitude, declination);
  if (!up || !down) return HourWindow{0.0, 24.0};
  return HourWindow{*up, *down};
}

MoonPlace moon_at(double hour, double latitude, double sun_declination, double age) {
  const double phase = num::mod(num::mod(age / kSynodicMonth, 1.0) + 1.0, 1.0);
  const double lag = phase * 24.0;
  const double declination = sun_declination * libm::cos(2.0 * kPi * phase);
  const SunPlace place = sun_at(hour - lag, latitude, declination);
  return MoonPlace{place.elevation, place.azimuth, place.rising, hour,
                   (1.0 - libm::cos(2.0 * kPi * phase)) / 2.0};
}

double julian_day(double year, double month, double day) {
  double y = year;
  double m = month;
  if (m <= 2) {
    y -= 1.0;
    m += 12.0;
  }
  const double a = std::floor(y / 100.0);
  const double b = 2.0 - a + std::floor(a / 4.0);
  return std::floor(365.25 * (y + 4716.0)) + std::floor(30.6001 * (m + 1.0)) + day + b - 1524.5;
}

double julian_day_of_year(double year, double day_of_year) {
  return julian_day(year, 1.0, 1.0) + day_of_year - 1.0;
}

SolarCoordinates solar_coordinates(double jd) {
  const double t = (jd - 2451545.0) / 36525.0;
  const double l0 = wrap360(280.46646 + t * (36000.76983 + t * 0.0003032));
  const double m = 357.52911 + t * (35999.05029 - 0.0001537 * t);
  const double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  const double c = sin_d(m) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
                   sin_d(2.0 * m) * (0.019993 - 0.000101 * t) + sin_d(3.0 * m) * 0.000289;
  const double omega = 125.04 - 1934.136 * t;
  const double longitude = l0 + c - 0.00569 - 0.00478 * sin_d(omega);
  const double eps0 =
      23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
  const double obliquity = eps0 + 0.00256 * cos_d(omega);
  const double declination = libm::asin(clamp_unit(sin_d(obliquity) * sin_d(longitude))) / kDeg;
  const double y = libm::pow(tan_d(obliquity / 2.0), 2.0);
  const double equation_of_time =
      (4.0 / kDeg) *
      (y * sin_d(2.0 * l0) - 2.0 * e * sin_d(m) + 4.0 * e * y * sin_d(m) * cos_d(2.0 * l0) -
       0.5 * y * y * sin_d(4.0 * l0) - 1.25 * e * e * sin_d(2.0 * m));
  return SolarCoordinates{declination, equation_of_time, wrap360(longitude), obliquity};
}

double refraction(double elevation) {
  const double el = elevation / kDeg;
  if (el > 85) return 0.0;
  const double te = libm::tan(elevation);
  double arcsec;
  if (el > 5) {
    arcsec = 58.1 / te - 0.07 / libm::pow(te, 3.0) + 0.000086 / libm::pow(te, 5.0);
  } else if (el > -0.575) {
    arcsec = 1735.0 + el * (-518.2 + el * (103.4 + el * (-12.79 + el * 0.711)));
  } else {
    arcsec = -20.774 / te;
  }
  return (arcsec / 3600.0) * kDeg;
}

double universal_hour_of(double jd0, double hour, double longitude) {
  double utc = hour - longitude / 15.0;
  for (int i = 0; i < 3; i++) {
    utc = hour - longitude / 15.0 - solar_coordinates(jd0 + utc / 24.0).equation_of_time / 60.0;
  }
  return utc;
}

DatedSunPlace sun_on_date(double hour, double latitude, double longitude, double jd0) {
  const double utc = universal_hour_of(jd0, hour, longitude);
  const double declination = solar_coordinates(jd0 + utc / 24.0).declination;
  const SunPlace at = sun_at(hour, latitude, declination);
  return DatedSunPlace{at.elevation + refraction(at.elevation),
                       at.azimuth,
                       at.rising,
                       at.hour,
                       at.elevation,
                       declination};
}

std::optional<double> hour_of_sun_on_date(double elevation, bool rising, double latitude,
                                          double longitude, double jd0) {
  const auto off = [&](double h) {
    return sun_on_date(h, latitude, longitude, jd0).true_elevation - elevation;
  };
  const double from = rising ? 0.0 : 12.0;
  double was = off(from);
  double h = from + kStep;  // accumulated, as in hour_of_elevation
  while (h <= from + 12.0 + 1e-9) {
    const double now = off(h);
    if ((rising && was < 0 && now >= 0) || (!rising && was > 0 && now <= 0)) {
      double lo = h - kStep;
      double hi = h;
      for (int i = 0; i < 16; i++) {
        const double mid = (lo + hi) / 2.0;
        const double m = off(mid);
        if ((rising && m < 0) || (!rising && m > 0)) {
          lo = mid;
        } else {
          hi = mid;
        }
      }
      return (lo + hi) / 2.0;
    }
    was = now;
    h += kStep;
  }
  return std::nullopt;
}

std::optional<HourWindow> sun_window_on_date(double latitude, double longitude, double jd0,
                                             double min_elevation) {
  const auto up = hour_of_sun_on_date(min_elevation, true, latitude, longitude, jd0);
  const auto down = hour_of_sun_on_date(min_elevation, false, latitude, longitude, jd0);
  if (up && down) return HourWindow{*up, *down};
  const double noon = sun_on_date(12.0, latitude, longitude, jd0).true_elevation;
  if (noon < min_elevation) return std::nullopt;
  // Over the floor at noon and never crossing it: a sun that never sets.
  if (!up && !down) return HourWindow{0.0, 24.0};
  // One crossing only: the day the midnight sun begins or ends.
  return HourWindow{up.value_or(0.0), down.value_or(24.0)};
}

MoonOrbit lunar_coordinates(double jd) {
  const double d = jd - 2451543.5;
  const double n = 125.1228 - 0.0529538083 * d;
  const double i = 5.1454;
  const double w = 318.0634 + 0.1643573223 * d;
  const double a = 60.2666;
  const double e = 0.0549;
  const double m = wrap360(115.3654 + 13.0649929509 * d);
  // Kepler's equation, by Newton's steps from the first-order guess.
  double ecc = m + (e / kDeg) * sin_d(m) * (1.0 + e * cos_d(m));
  for (int k = 0; k < 5; k++) {
    ecc = ecc - (ecc - (e / kDeg) * sin_d(ecc) - m) / (1.0 - e * cos_d(ecc));
  }
  const double xv = a * (cos_d(ecc) - e);
  const double yv = a * std::sqrt(1.0 - e * e) * sin_d(ecc);
  const double v = libm::atan2(yv, xv) / kDeg;
  double r = std::sqrt(xv * xv + yv * yv);
  const double xh = r * (cos_d(n) * cos_d(v + w) - sin_d(n) * sin_d(v + w) * cos_d(i));
  const double yh = r * (sin_d(n) * cos_d(v + w) + cos_d(n) * sin_d(v + w) * cos_d(i));
  const double zh = r * sin_d(v + w) * sin_d(i);
  double lon = libm::atan2(yh, xh) / kDeg;
  double lat = libm::atan2(zh, std::sqrt(xh * xh + yh * yh)) / kDeg;
  // The perturbations: evection, variation, the yearly equation and the rest down to 0.01 deg.
  const double ms = 356.047 + 0.9856002585 * d;
  const double ls = ms + 282.9404 + 4.70935e-5 * d;
  const double lm = m + w + n;
  const double dd = lm - ls;
  const double f = lm - n;
  lon += -1.274 * sin_d(m - 2.0 * dd) + 0.658 * sin_d(2.0 * dd) - 0.186 * sin_d(ms) -
         0.059 * sin_d(2.0 * m - 2.0 * dd) - 0.057 * sin_d(m - 2.0 * dd + ms) +
         0.053 * sin_d(m + 2.0 * dd) + 0.046 * sin_d(2.0 * dd - ms) + 0.041 * sin_d(m - ms) -
         0.035 * sin_d(dd) - 0.031 * sin_d(m + ms) - 0.015 * sin_d(2.0 * f - 2.0 * dd) +
         0.011 * sin_d(m - 4.0 * dd);
  lat += -0.173 * sin_d(f - 2.0 * dd) - 0.055 * sin_d(m - f - 2.0 * dd) -
         0.046 * sin_d(m + f - 2.0 * dd) + 0.033 * sin_d(f + 2.0 * dd) + 0.017 * sin_d(2.0 * m + f);
  r += -0.58 * cos_d(m - 2.0 * dd) - 0.46 * cos_d(2.0 * dd);
  lon = wrap360(lon);
  const double ecl = 23.4393 - 3.563e-7 * d;
  const double xe = cos_d(lon) * cos_d(lat);
  const double ye = cos_d(ecl) * sin_d(lon) * cos_d(lat) - sin_d(ecl) * sin_d(lat);
  const double ze = sin_d(ecl) * sin_d(lon) * cos_d(lat) + cos_d(ecl) * sin_d(lat);
  return MoonOrbit{lon, lat, r, wrap360(libm::atan2(ye, xe) / kDeg),
                   libm::atan2(ze, std::sqrt(xe * xe + ye * ye)) / kDeg};
}

DatedMoonPlace moon_on_date(double hour, double latitude, double longitude, double jd0) {
  const double jd = jd0 + universal_hour_of(jd0, hour, longitude) / 24.0;
  const MoonOrbit moon = lunar_coordinates(jd);
  const SolarCoordinates sun = solar_coordinates(jd);
  const double hour_angle =
      wrap360(sidereal_time(jd) + longitude - moon.right_ascension + 180.0) - 180.0;
  const Horizontal geo = horizontal_of(moon.declination, hour_angle, latitude);
  const double parallax = libm::asin(1.0 / moon.distance);
  const double elongation = wrap360(moon.longitude - sun.longitude);
  const double cos_phase = cos_d(moon.longitude - sun.longitude) * cos_d(moon.latitude);
  return DatedMoonPlace{geo.elevation - parallax * libm::cos(geo.elevation),
                        geo.azimuth,
                        hour_angle < 0,
                        hour,
                        (1.0 - cos_phase) / 2.0,
                        (elongation / 360.0) * kSynodicMonth,
                        moon.declination};
}

}  // namespace ge::core
