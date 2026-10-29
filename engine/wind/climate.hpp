// THE AIR'S TEMPERATURE AND MOISTURE (docs/climate.md): how warm and how damp the air is at a
// place and an hour, as pure functions of a `Climate` (data a world sets once, or a sky picked by
// hand changes) and a `Site` (where the question is asked). Nothing is stepped and nothing draws a
// random number, so every machine, replay and lockstep peer reads the same air. The fog, the snow
// line, the frost, the breath and the ice on water (mist.hpp, cold.hpp) all read it.
//
// - THE DAY'S CURVE (`air_at`): the low at sunrise, a sine rising through the day to the high two
//   hours past solar noon, then an exponential fall through the night back to the next sunrise's
//   low (the two-part diurnal model of agricultural meteorology: a sine by day, b = 2.2 by
//   night). `mean_c` is the midpoint of the low and the high; their gap is `range_c` under a clear,
//   calm sky, shrinking under cloud (to 0.3 of it overcast) and in wind (half at 8 m/s), and
//   indoors by the share of the sky a site sees.
// - BY HEIGHT: the air cools `lapse` K a metre up (the standard atmosphere's 6.5 K/km).
// - THE NIGHT'S COLD POOL: on a clear, calm night the air in low ground (`Site::hollow`, how far a
//   site lies below the ground round it) drains in from the slopes and cools 0.02 K a metre of
//   hollow, up to 12 K, built over the first three hours after sunset and broken within two after
//   sunrise. That is why a valley's floor is the coldest place on a still night.
// - THE DEW POINT: the day's moisture is a dew point, held through the day while the temperature
//   swings (the humidity at the day's mean fixes it; the relative humidity then rises at night and
//   falls by day, as it does), dropping 1.8 K a kilometre up. Open water (`Site::water_m`, the
//   distance to it) and wet ground (`Site::wet`: puddles, melting snow) evaporate into the air,
//   raising the dew point toward the wet surface's own temperature, never past the air's: air
//   that cools to its dew point holds the rest as fog.
// - NEAR WATER: a lake tempers the air over it toward its own temperature (`water_c`), within a
//   few hundred metres of its shore. What water and wet ground lend the air fades with the height
//   over them (`Site::above`, 1/e in 50 m): the surface layer.
// - THE MAGNUS FORMULA for the vapour pressure over water (6.1094 exp(17.625 T / (T + 243.04))
//   hPa) and over ice (6.1121 exp(22.587 T / (T + 273.86))), giving the dew point, the frost
//   point and the relative humidity; the wet-bulb temperature from the psychrometric balance at
//   the height's pressure, which the snow's phase reads (`rain_share_of`: a dry air's
//   evaporation chills a fall, so snow reaches ground well above 0 C when the air is dry).
//
// Hours are solar hours, 0..24, wrapped. Temperatures are C, heights m.
//
// ge:summary The air's temperature and moisture: the day's curve, lapse, cold pools, dew point
// ge:keywords climate temperature humidity dew-point frost-point wet-bulb relative-humidity
// ge:keywords diurnal lapse-rate inversion cold-pool valley magnus psychrometric vapour-pressure
// ge:keywords air_at surface_at freezing_level snow_level rain_share_of saturation_hpa
// ge:keywords dew_point_of wet_bulb_of pressure_at site climate
// ge:skill water determinism
// ge:test tests/wind_climate_test.cpp
#pragma once

namespace ge::wind {

inline constexpr double kDewLapse = 0.0018;  // K/m: the dew point's fall upward

// A place's air for a day: what a world (or a sky picked by hand) sets.
struct Climate {
  double mean_c = 0;       // C: the midpoint of the day's low and high, 2 m over open ground
  double range_c = 10;     // K: the low-to-high swing under a clear, calm sky
  double ref_y = 0;        // m: the height `mean_c` is given at
  double lapse = 0.0065;   // K/m: how fast the air cools upward
  double humidity = 0.75;  // relative humidity at the day's mean, 0..1: fixes the dew point
  double cloud = 0.3;      // the sky's cover, 0 clear .. 1 overcast
  double wind = 3;         // m/s, the mean 10 m over open ground
  double sunrise = 6;      // solar hours
  double sunset = 18;      // solar hours
  double water_c = 4;      // C: open water's surface (a lake lags the air by weeks)
};

// Where the air is asked about.
struct Site {
  double y = 0;          // m, world height
  double above = 2;      // m over the ground or the water under it (what they lend fades upward)
  double hollow = 0;     // m: how far below the ground round it the site lies (a valley's floor)
  double water_m = 1e9;  // m to the nearest open water (0 over it)
  double wet = 0;        // the ground's water, 0 dry .. 1 standing (puddles, melting snow)
  double snow = 0;       // the ground's snow cover, 0 .. 1
  double sky = 1;        // the share of the sky the site sees: 1 in the open, 0 indoors
  double warmth = 0;     // K a local heat adds (a fire, a lava pool), the game's
};

// The air at a site and an hour.
struct Thermal {
  double temp_c = 0;      // the air, C
  double dew_c = 0;       // its dew point, C (never above temp_c)
  double frost_c = 0;     // its frost point, C: where it saturates over ice
  double rh = 0;          // relative humidity over water, 0..1
  double wet_bulb_c = 0;  // C
};

// The vapour pressure at saturation over water and over ice, hPa.
double saturation_hpa(double temp_c);
double saturation_ice_hpa(double temp_c);
// The dew point of air at `temp_c` and relative humidity `rh` (0..1].
double dew_point_of(double temp_c, double rh);
// The wet-bulb temperature, C, at a pressure, hPa (the psychrometric balance, solved).
double wet_bulb_of(double temp_c, double rh, double pressure_hpa = 1013.25);
// The air's pressure at a height, hPa (an 8.4 km scale height).
double pressure_at(double y);

// The share of the day's swing that reaches a site: cloud, wind and a roof all damp it.
double swing_of(const Climate& c, const Site& s);
// How strongly the night is cooling the low ground at an hour: 0 by day or under cloud and wind,
// toward 1 on a clear, still night three hours after sunset.
double night_chill(const Climate& c, double hour);

// THE AIR at a site and an hour.
Thermal air_at(const Climate& c, const Site& s, double hour);

// The ground's surface, C: colder than the air on a clear night (up to 4 K, radiating to the sky),
// warmer by day in the sun (up to 5 K on bare ground); snow is never above 0. Frost, dew, black
// ice and the snow's feel read it.
double surface_at(const Climate& c, const Site& s, double hour);

// THE PHASE OF A FALL by the wet bulb: 0 snow under -0.5 C wet-bulb, rain over 2.5 C, a smooth S
// between (half and half at 1 C). Dry air lets snow fall through air well above freezing.
double rain_share_of(const Thermal& air);

// Where the open air (no hollow, no water) is at 0 C at an hour, m; very low when it is never
// freezing, very high when it always is. precip.hpp's `rain_share_at` reads it as its freezing
// level.
double freezing_level(const Climate& c, double hour);
// Where a fall turns from snow to rain at an hour: the height whose wet bulb is 1 C, m.
double snow_level(const Climate& c, double hour);

}  // namespace ge::wind
