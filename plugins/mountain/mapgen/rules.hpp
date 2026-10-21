// THE RULE BOOK'S NUMBERS (R1–R24): every number the single-piste generator builds to and the
// analysis holds a finished map to, with the band helpers. Each namespace's comment says what
// its rule asks. The `trick` block (R20, the terrain park) is not here yet: no map the
// generator builds carries a park (docs/goals.md M4c).
//
// Every value is a decimal literal, so the doubles are fixed bits. A band is [min, max].
// Held by tests/mountain_generate_test.cpp (every map is built off these numbers).
//
// ge:summary The rule book's numbers (R1-R24) as constexpr namespaces
// ge:keywords rules rule-book
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"

namespace ge::mountain::mapgen {

struct Band {
  double min;
  double max;
};

// The skies R19 deals, lightest first (WEATHER_KINDS' order, which the digest and the golden
// files index by).
enum class WeatherKind : uint8_t { clear, fair, flurries, high, overcast, snow, storm, fog };
inline constexpr int kWeatherKinds = 8;

namespace rules {

namespace world {
inline constexpr double size = 3000;  // R1: the map is [0, size]² m
inline constexpr double cell = 2;     // the heightfield's pitch, m
}  // namespace world

namespace mountain {
inline constexpr double summit = 0.08;  // the summit ridge's crest, share of size down the map
inline constexpr double base = 0.9;     // where the valley floor begins
inline constexpr Band vertical{850, 1030};
inline constexpr double crests = 40;
inline constexpr double crest_spread = 140;
inline constexpr double back = 0.35;
namespace profile {
inline constexpr double shoulder = 0.22;
inline constexpr double shoulder_run = 0.25;
inline constexpr double ease = 1.2;
inline constexpr double runout = 0.14;
}  // namespace profile
namespace flank {
inline constexpr double inner = 560;
inline constexpr double outer = 800;
inline constexpr Band height{120, 220};
inline constexpr double warp = 40;
inline constexpr Band open{0.7, 1};
}  // namespace flank
}  // namespace mountain

namespace face {
namespace hills {
inline constexpr Band amplitude{12, 24};
inline constexpr double scale = 260;
inline constexpr double floor = 0.35;
}  // namespace hills
namespace ridges {
inline constexpr Band amplitude{8, 18};
inline constexpr double scale = 200;
inline constexpr double stretch = 3.5;
}  // namespace ridges
namespace bowls {
inline constexpr Band count{2, 4};
inline constexpr Band radius{70, 140};
inline constexpr Band depth{6, 14};
inline constexpr Band at{0.06, 0.42};
}  // namespace bowls
namespace headwalls {
inline constexpr Band count{1, 3};
inline constexpr Band drop{20, 45};
inline constexpr Band run{60, 110};
inline constexpr Band at{0.12, 0.6};
inline constexpr double wander = 30;
}  // namespace headwalls
namespace rollers {
inline constexpr double amplitude = 4;
inline constexpr double scale = 70;
}  // namespace rollers
}  // namespace face

namespace kickers {
namespace off {
inline constexpr Band count{6, 12};
inline constexpr Band height{1.8, 3.5};
inline constexpr Band ramp{14, 22};
inline constexpr Band landing{26, 40};
inline constexpr Band width{14, 26};
inline constexpr double clearance = 30;
inline constexpr double roll = 0.08;
inline constexpr double max_slope = 0.7;
}  // namespace off
namespace on {
inline constexpr Band count{2, 6};
inline constexpr Band height{1, 2.6};
inline constexpr Band pitch{0.2, 0.5};
inline constexpr Band air{40, 80};
inline constexpr Band ramp{8, 11};
inline constexpr Band landing{14, 20};
inline constexpr double straight = 0.45;
inline constexpr double spacing = 220;
inline constexpr double approach_grade = 0.4;
inline constexpr double roll = 0.04;
}  // namespace on
inline constexpr double edge = 8;
}  // namespace kickers

namespace track {
inline constexpr Band length{3000, 4500};
inline constexpr Band aim{3200, 4200};
inline constexpr double step = 2;
inline constexpr Band start_x{0.36, 0.64};
inline constexpr double start_z = 0.14;
inline constexpr double hold = 60;
inline constexpr double swing = 1.3;
inline constexpr Band wander_scale{180, 340};
inline constexpr double wander_vary = 0.35;
inline constexpr Band sweeps_count{1, 3};
inline constexpr Band sweeps_length{150, 320};
inline constexpr Band sweeps_angle{0.8, 1.15};
inline constexpr Band schuss_count{1, 2};
inline constexpr Band schuss_length{140, 240};
inline constexpr double min_fall = 0.04;
inline constexpr double steepest = 0.6;
inline constexpr double finish_z = 0.9;
inline constexpr double finish = 80;
inline constexpr double runout = 60;
inline constexpr double finish_turn = 120;
inline constexpr double separation_plan = 60;
inline constexpr double separation_along = 200;
inline constexpr double min_radius = 20;
inline constexpr double steep_radius = 50;
inline constexpr double turn_window = 10;
inline constexpr Band width{20, 40};
inline constexpr double width_scale = 300;
inline constexpr Band narrow{0.3, 0.7};
inline constexpr double shoulder_flat = 2;
inline constexpr double shoulder_packed = 5;
inline constexpr double bank_slope = 0.6;
inline constexpr double bank_min = 8;
inline constexpr double bank_max = 30;
inline constexpr double min_grade = 0.06;
inline constexpr double max_grade = 0.78;
inline constexpr double bend_knee = 0.25;
inline constexpr double camber = 0.06;
inline constexpr double grade_window = 10;
inline constexpr double colour_window = 100;
inline constexpr double max_cut = 14;
}  // namespace track

namespace checkpoint {
inline constexpr Band spacing{90, 150};
inline constexpr double spacing_target = 120;
inline constexpr double margin = 3;
}  // namespace checkpoint

namespace spawn {
inline constexpr double kicker_gap = 100;
inline constexpr double run = 40;
inline constexpr double straight = 0.2;
inline constexpr double max_slope = 0.25;
}  // namespace spawn

namespace grid {
inline constexpr int slots = 4;
inline constexpr int abreast = 4;
inline constexpr double spacing = 3;
inline constexpr double back = 6;
}  // namespace grid

namespace forest {
inline constexpr double spacing = 6;
inline constexpr double scale = 240;
inline constexpr double density = 0.72;
inline constexpr double meadow = 0.03;
inline constexpr Band clearings_count{4, 9};
inline constexpr Band clearings_radius{25, 70};
inline constexpr Band height{6, 19};
namespace age {
inline constexpr double top = 24;
inline constexpr double rate = 0.012;
inline constexpr double spread = 0.45;
inline constexpr double veterans = 0.07;
inline constexpr double veteran = 2.6;
inline constexpr double max = 480;
}  // namespace age
namespace trunk {
inline constexpr double floor = 0.04;
inline constexpr double top = 0.68;
inline constexpr double rate = 0.006;
inline constexpr double shape = 1.1;
}  // namespace trunk
inline constexpr double crown = 0.24;
inline constexpr double crown_max = 3.2;
inline constexpr double gap = 9;
inline constexpr double open = 0.95;
namespace clumps {
inline constexpr double spacing = 36;
inline constexpr double woods = 0.7;
inline constexpr double meadow = 0.16;
inline constexpr Band trees{4, 8};
inline constexpr double radius = 6.5;
inline constexpr double gap = 2.6;
}  // namespace clumps
namespace lanes {
inline constexpr int families = 2;
inline constexpr double spacing = 150;
inline constexpr double width = 16;
inline constexpr double swing = 28;
inline constexpr double scale = 280;
}  // namespace lanes
inline constexpr double corridor = 9;
inline constexpr double max_slope = 0.75;
inline constexpr double krummholz = 150;
inline constexpr double krummholz_height = 0.35;
}  // namespace forest

namespace sun {
inline constexpr Band latitude{44, 48};
inline constexpr Band day_of_year{1, 90};
inline constexpr Band hour{9, 16};
inline constexpr double min_elevation = 5;
inline constexpr Band evening{-0.5, 3.5};
inline constexpr double facing = (55 * ge::core::kPi) / 180;
}  // namespace sun

namespace race {
inline constexpr int laps = 1;
}  // namespace race

namespace drift {
inline constexpr Band share{0, 0.5};
inline constexpr Band length{60, 180};
inline constexpr double gap = 40;
inline constexpr double packed = 0.05;
inline constexpr double fade = 12;
inline constexpr double clear = 100;
}  // namespace drift

namespace berm {
inline constexpr double width = 6;
inline constexpr Band height{0.3, 0.5};
inline constexpr double wander_a = 23;
inline constexpr double wander_b = 61;
inline constexpr double max_slope = 0.3;
}  // namespace berm

namespace weather {
// By WeatherKind: clear fair flurries high overcast snow storm fog.
inline constexpr std::array<double, kWeatherKinds> odds{0.26, 0.24, 0.1,  0.12,
                                                        0.06, 0.08, 0.07, 0.07};
inline constexpr Band snowfall_flurries{0.05, 0.3};
inline constexpr Band snowfall_snow{0.3, 0.75};
inline constexpr Band snowfall_storm{0.75, 1};
inline constexpr Band fog{0.35, 1};
inline constexpr std::array<Band, kWeatherKinds> wind{
    {{0.5, 5}, {2, 7}, {1, 5}, {3, 9}, {2, 8}, {2, 16}, {11, 22}, {0, 2}}};
inline constexpr double evening = 0.25;
}  // namespace weather

namespace cliff {
inline constexpr Band count{3, 6};
inline constexpr Band drop{5, 10};
inline constexpr double face = 0.4;
inline constexpr double apron = 0.5;
inline constexpr double landing = 6;
inline constexpr Band shelf{70, 110};
inline constexpr Band width{36, 70};
inline constexpr double edge = 14;
inline constexpr double runout = 25;
inline constexpr double clearance = 25;
inline constexpr double fall = 0.05;
}  // namespace cliff

namespace grade {
inline constexpr double band_green = 0.16;
inline constexpr double band_blue = 0.27;
inline constexpr double band_red = 0.47;
inline constexpr double region_share = 0.35;
}  // namespace grade

namespace drop {
inline constexpr Band drop{1.8, 3.2};
inline constexpr Band shelf{20, 50};
inline constexpr double landing = 18;
inline constexpr double min_fall = 0.22;
inline constexpr double straight = 0.5;
inline constexpr double spacing = 150;
inline constexpr double kicker_clear = 40;
inline constexpr double gate_clear = 6;
inline constexpr double finish_clear = 120;
}  // namespace drop

}  // namespace rules

// R6 — the least radius a bend on a pitch of `grade` m per m may turn: R6's floor, eased up
// to the steep radius past the knee.
inline double bend_floor(double grade) {
  namespace T = rules::track;
  const double t = ge::core::num::min(
      1.0, ge::core::num::max(0.0, (grade - T::bend_knee) / (T::max_grade - T::bend_knee)));
  const double ease = t * t * (3 - 2 * t);
  return T::min_radius + (T::steep_radius - T::min_radius) * ease;
}

// A draw inside a band: `rng.range(band.min, band.max)`.
inline double in_band(ge::core::Rng& rng, Band band) {
  return ge::core::range(rng, band.min, band.max);
}

// Whether `value` lies in a band, with `slack` either side.
inline bool within_band(double value, Band band, double slack = 1e-6) {
  return value >= band.min - slack && value <= band.max + slack;
}

}  // namespace ge::mountain::mapgen
