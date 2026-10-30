// WHAT FALLS THROUGH THE AIR (docs/rendering.md § Precipitation): snow, sleet and rain, from none
// through light, moderate and heavy to a storm, as pure functions a world's weather and its
// picture both read. Nothing here is stepped but the wet ground (`step_wet`), whose state a game
// keeps, and nothing draws from a run's generator.
//
// - THE ONE DIAL: a fall's `intensity`, 0 (nothing) .. 1 (the worst a sky gives), and its `rain`
//   share, 0 (dry snow) .. 1 (rain), sleet and wet snow between. A world's sky sets the two; the
//   renderer, the sound and the ground's snow read the same pair.
// - THE RATE (`rate_of`): the liquid-equivalent rate, mm/h, exponential in the intensity so each
//   step of the dial is a like step to the eye: rain 0.64 * 232.6^i (2.5 mm/h at 0.25, 50 at 0.8,
//   148 at 1, a cloudburst); snow 0.04 * 250^i (10 mm/h of water at 1, near a metre of new snow a
//   day). A mix is the two blended in the logarithm by its rain share.
// - THE TIERS (`tier_of`): rain by its rate (light under 2.5 mm/h, moderate to 7.6, heavy past
//   it), snow by what it hides (light seen through past 1 km, moderate past 500 m, heavy nearer);
//   either is a STORM when heavy under a wind of 15 m/s or more (snow so driven is a blizzard), or
//   rain past 50 mm/h whatever the wind.
// - THE PHASE (`rain_share_at`): snow above the freezing level, melting through the 300 m under
//   it (sleet, then wet snow's last flakes), rain below: a smoothstep over the band.
// - THE FALL'S PACE: a raindrop's terminal speed by its diameter (`drop_speed`, the exponential fit
//   9.65 - 10.3 e^(-0.6 D), m/s, D in mm), the median drop by the rate off the exponential size
//   spectrum (`median_drop`: D0 = 3.67 / (4.1 R^-0.21) mm); a snowflake's ~1 m/s; sleet between
//   (`fall_speed`, blended by the rain share). The drops landing on a square metre a second
//   (`drops_per_m2s`): that spectrum carried down at each size's speed, the count a lake's rings
//   and the rain's patter are drawn and heard by.
// - WHAT ONE SEES THROUGH IT (`visibility_of`): rain 11.5 R^-0.63 km (10 km in a drizzle, under a
//   kilometre in a downpour), snow 1.2 R^-0.9 km (a flake hides ten times what a drop does), a mix
//   blended in the logarithm.
// - THE SWIRL (`swirl_of`): the turbulent speed that carries a flake about the mean, the wind's
//   own turbulence intensity times its speed, plus a floor in still air.
// - THE WET GROUND (`Wet`, `step_wet`, `wet_steady`, `dry_rate`): the water standing on a surface,
//   mm: rain wets a film (0.3 mm) and fills puddles (up to 3 mm) that drain at 6 per hour of what
//   stands over the film (a tenth of it a minute) and dry by the wind and the sun. `wet` (the
//   film's share) and `puddles` (the puddles' share) are what a material reads. Snow lays no water;
//   it is the ground's snow's to settle (a snow layer reads `rate_of` with the rain share).
//
// ge:summary Snow, sleet and rain: intensity, rate, tiers, phase by height, fall speed, visibility
// ge:keywords precip precipitation rain sleet snow drizzle downpour storm blizzard intensity rate
// ge:keywords tier freezing-level phase drop terminal-speed marshall-palmer visibility swirl wet
// ge:keywords puddle wetness rate_of tier_of rain_share_at drop_speed median_drop fall_speed
// ge:keywords drops_per_m2s mean_drop_over impacts visibility_of swirl_of step_wet
// ge:keywords wet_steady dry_rate cover step_cover lightning
// ge:keywords flash_at strokes_per_minute thunderstorm
// ge:skill water render
// ge:test tests/wind_precip_test.cpp
#pragma once

#include <cstdint>

namespace ge::wind {

enum class Tier : uint8_t { none, light, moderate, heavy, storm };

// The tiers' words ("none", "light", "moderate", "heavy", "storm").
const char* tier_name(Tier tier);

// A fall: the dial and how much of it is liquid.
struct Precip {
  double intensity = 0;  // 0 nothing .. 1 the worst a sky gives
  double rain = 0;       // 0 dry snow .. 1 rain
};

// Under this intensity nothing is said to fall.
inline constexpr double kNoFall = 0.02;

// The liquid-equivalent rate, mm/h.
double rate_of(const Precip& p);

// The tier of a fall under a wind of `wind` m/s.
Tier tier_of(const Precip& p, double wind);

// The rain share at height `y` m under a freezing level at `freezing` m.
inline constexpr double kMeltBand = 300;  // m
double rain_share_at(double y, double freezing);

// A raindrop's terminal speed, m/s, by its diameter, mm.
double drop_speed(double diameter_mm);
// The median drop diameter, mm, at a rain rate, mm/h.
double median_drop(double rate);
// The drops of `smallest` mm and larger landing on a square metre a second at a rain rate, mm/h:
// the exponential spectrum's count N0 e^(-L D) (N0 = 8000 / m^3 / mm, L = 4.1 R^-0.21 / mm)
// carried down at each size's terminal speed and summed past `smallest`.
double drops_per_m2s(double rate, double smallest);
// The mean diameter of those drops, mm: `smallest` + 1 / L (the spectrum is exponential).
double mean_drop_over(double rate, double smallest);
// The fall's pace in still air, m/s: a snowflake's, a drop's of the median size, sleet between.
double fall_speed(const Precip& p);

// How far one sees through the fall, m (20 km when nothing falls).
double visibility_of(const Precip& p);

// The turbulent speed about the mean, m/s, for a wind of `speed` m/s and turbulence intensity
// `turbulence` (engine/wind's `turbulence_for`, or a world's own).
double swirl_of(double speed, double turbulence);

// THE WET GROUND: the water standing on a surface, and what a material reads off it.
struct Wet {
  double water = 0;    // mm
  double wet = 0;      // the film's share, 0 dry .. 1 soaked
  double puddles = 0;  // the puddles' share, 0 .. 1
};
inline constexpr double kFilm = 0.3;   // mm
inline constexpr double kPools = 3.0;  // mm
inline constexpr double kDrain = 6.0;  // of the water over the film, an hour

// The drying, mm/h, by the wind (m/s) and the sun's share on the surface (0 .. 1).
double dry_rate(double wind, double sun);
// `w` after `dt` s under a rain rate of `rate` mm/h drying at `dry` mm/h.
Wet step_wet(const Wet& w, double rate, double dry, double dt);
// What a long spell of the same rain and drying leaves (a rewind or a first frame starts here).
Wet wet_steady(double rate, double dry);

// THE SNOW'S COVER on a surface open to the fall.
struct Cover {
  double depth = 0;  // mm of snow
  double cover = 0;  // the share of the surface it hides, 0 .. 1
};
inline constexpr double kNewSnow = 10;        // mm of new snow a mm of water
inline constexpr double kCoverDepth = 6;      // mm: the depth that hides 63 % of a surface
inline constexpr double kDeepestCover = 400;  // mm: past this it is drifted away as it falls
// `c` after `dt` s of the fall `p` with the sun's share `sun` (0 .. 1) on it.
Cover step_cover(const Cover& c, const Precip& p, double sun, double dt);

// LIGHTNING: a thunderstorm's strokes a minute under the fall `p` in a wind of `wind` m/s.
double strokes_per_minute(const Precip& p, double wind);
// The flash at time `t` s: 0 dark .. about 1 a main stroke at its brightest, over the open sky's
// light. `seed` sets which half-second slots strike.
double flash_at(double t, const Precip& p, double wind, uint32_t seed = 0);

}  // namespace ge::wind
