// A run's start placed under its top: the fair heading, the slide down the fall line.
//
// ge:summary A run's start: the fair heading off it, the slide to a glide's fall, the search
// ge:keywords run-start fair-heading head-below head-on-contour place-start
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/run_start.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
namespace RR = resort_rules;
using ge::core::hypot;
using ge::core::sample_field;

namespace {

// How far along the contour from its top station a run's start is looked for, m a step and
// steps; and how far apart along it two runs off one top start, m.
constexpr double kStartStep = 30;
constexpr int kStartSteps = 12;
constexpr double kSiblingApart = 80;
// The fall line a start is slid along, m a step and steps.
constexpr double kHeadSlide = 4;
constexpr int kHeadSlides = 60;
// How far inside a ramp's reach a start stands, m.
constexpr double kReachSpare = 20;
// How much further down the fall line than the highest spot under its top a start is tried, m.
constexpr double kHeadDowns[] = {0, 12, 24, 36, 48};
// The headings about the wanted one a start is tried on, rad.
constexpr double kTurns[] = {0,    0.25,  -0.25, 0.5,  -0.5, 0.8,  -0.8,
                             1.05, -1.05, 1.3,   -1.3, 1.55, -1.55};

// The start heading a run can be raced off (R12): the fall of the first stretch no steeper than
// its colour's start, and on a black at least its floor; none where no heading holds.
std::optional<double> fair_heading(const ge::core::Heightfield& ground, double x, double z,
                                   double want, const GradeRow& row) {
  const double run = R::grid::back + R::track::step + R::spawn::run;
  const double gw = R::track::grade_window;
  for (const double d : kTurns) {
    const double h = want + d;
    if (std::abs(h) > 1.3) continue;
    const double sx = libm::sin(h);
    const double cz = libm::cos(h);
    double steepest = 0;
    double climb = 0;
    double u = 0;
    while (u + gw <= run) {
      const double a = sample_field(ground, x + sx * u, z + cz * u);
      const double b = sample_field(ground, x + sx * (u + gw), z + cz * (u + gw));
      const double fall = (a - b) / gw;
      steepest = num::max(steepest, fall);
      climb = num::max(climb, -fall);
      u += 5;
    }
    const double end = sample_field(ground, x + sx * run, z + cz * run);
    const double mean = (sample_field(ground, x, z) - end) / run;
    if (steepest > row.spawn.max_slope * 0.85 || climb > 0.06) continue;
    if (row.spawn.min_slope > 0 && mean < row.spawn.min_slope * 1.2) continue;
    return h;
  }
  return std::nullopt;
}

// R27 — how far under the top's snow a start at (x, z) must lie: `drop` m, and a glide's fall
// over the way from the rim to it.
double head_below(const StartTop& top, double x, double z) {
  namespace K = RR::lift::top::ramp;
  return K::drop + top.glide * num::max(0.0, hypot(x - top.x, z - top.z) - top.rim);
}

}  // namespace

StartTop start_top(const ge::core::Heightfield& ground, const LiftPlan& lift, double rim,
                   double glide) {
  const double y = sample_field(ground, lift.top.x, lift.top.z);
  return {lift.top.x, lift.top.z, y, lift.kind == LiftKind::drag ? 0 : rim, glide};
}

std::optional<double> head_on_contour(const ge::core::Heightfield& ground, double x, double z0,
                                      const StartTop& top) {
  const auto under = [&](double z) {
    return sample_field(ground, x, z) <= top.y - head_below(top, x, z);
  };
  const auto near = [&](double z) {
    return hypot(x - top.x, z - top.z) <= RR::lift::top::ramp::far - kReachSpare;
  };
  if (!under(z0)) {
    for (int k = 1; k <= kHeadSlides; k++) {
      const double z = z0 + k * kHeadSlide;
      if (under(z)) return near(z) ? std::optional(z) : std::nullopt;
    }
    return std::nullopt;
  }
  double z = z0;
  for (int k = 1; k <= kHeadSlides; k++) {
    const double up = z0 - k * kHeadSlide;
    if (!under(up)) break;
    z = up;
  }
  return near(z) ? std::optional(z) : std::nullopt;
}

std::optional<RunSpec> place_start(const ge::core::Heightfield& ground, const RunSpec& spec,
                                   double lean, std::span<const double> siblings,
                                   const StartClear& clear, const std::optional<StartTop>& top) {
  const int dir = lean >= 0 ? 1 : -1;
  const double mid = (double(ground.cols - 1) * ground.cell) / 2;
  // Under a top, at half the step along the contour and a little further down the fall line at
  // each, for the ramp's room as much as the start's.
  const bool close = top.has_value();
  const int split = close ? 2 : 1;
  const std::span<const double> downs =
      close ? std::span<const double>(kHeadDowns) : std::span<const double>(kHeadDowns, 1);
  const double reach_x = (RR::massif::flank::inner * mid * 2) / R::world::size - 160;
  for (int k = 0; k <= kStartSteps * split; k++) {
    const int sides[2] = {dir, -dir};
    for (int si = 0; si < (k == 0 ? 1 : 2); si++) {
      const int sgn = k == 0 ? 1 : sides[si];
      const double x = spec.x + (sgn * k * kStartStep) / split;
      if (std::abs(x - mid) > reach_x) continue;
      if (std::any_of(siblings.begin(), siblings.end(),
                      [&](double sx) { return std::abs(sx - x) < kSiblingApart; })) {
        continue;
      }
      const std::optional<double> z0 =
          top ? head_on_contour(ground, x, spec.z, *top) : std::optional<double>(spec.z);
      if (!z0) continue;
      for (const double down : downs) {
        const double z = *z0 + down;
        if (top && close &&
            hypot(x - top->x, z - top->z) > RR::lift::top::ramp::far - kReachSpare) {
          break;
        }
        const std::optional<double> heading = fair_heading(ground, x, z, spec.heading, *spec.row);
        if (heading && clear(x, z, *heading)) {
          RunSpec out = spec;
          out.x = x;
          out.z = z;
          out.heading = *heading;
          return out;
        }
      }
    }
  }
  return std::nullopt;
}

}  // namespace ge::mountain::mapgen
