// The definitions behind grades.hpp.
//
// ge:summary The grade rows' numbers and the colour a measured piste is signed with
// ge:keywords grade
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/grades.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/num.hpp"
#include "core/prng.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace R = rules;

namespace {
constexpr Band kNone{0, 0};
constexpr double kGreen = R::grade::band_green;
constexpr double kBlue = R::grade::band_blue;
constexpr double kRed = R::grade::band_red;
}  // namespace

// THE UNGRADED ROW: the rule book's own numbers.
const GradeRow kUngraded{
    std::nullopt,
    {0, R::track::max_grade},
    R::mountain::vertical,
    {R::mountain::profile::shoulder, R::mountain::profile::shoulder_run, R::mountain::profile::ease,
     R::mountain::profile::runout},
    {1, 1, 1, {1, 1}},
    {R::track::steepest, R::track::max_grade, R::track::width, R::track::sweeps_count, 1,
     R::track::length},
    {R::spawn::max_slope, 0},
    {R::kickers::on::count, 1, R::kickers::on::approach_grade, 1},
    kNone,
    {1, kNone, R::cliff::clearance},
    R::drift::share,
};

namespace {

// One row a colour, in PisteGrade order.
const std::array<GradeRow, 4> kGrades{{
    {PisteGrade::green,
     {0, kGreen},
     {300, 380},
     {0.85, 0.15, 0.3, 0.55},
     {0.3, 0.25, 0.25, {0, 0}},
     {0.13, 0.19, {32, 40}, {2, 3}, 1, {2500, R::track::length.max}},
     {0.12, 0},
     {{0, 2}, 0.6, 0.16, 0.35},
     kNone,
     {0.5, kNone, 40},
     {0, 0.1}},
    {PisteGrade::blue,
     {kGreen, kBlue},
     {470, 580},
     {0.6, 0.2, 0.6, 0.35},
     {0.5, 0.45, 0.5, {0.5, 0.4}},
     {0.22, 0.31, {26, 40}, {1, 3}, 1, R::track::length},
     {0.2, 0},
     {{1, 3}, 0.8, 0.27, 0.8},
     kNone,
     {0.8, kNone, 30},
     {0, 0.2}},
    {PisteGrade::red,
     {kBlue, kRed},
     {640, 780},
     {0.4, 0.25, 0.9, 0.22},
     {0.8, 0.8, 1, {1, 0.8}},
     {0.4, 0.55, {22, 40}, {1, 3}, 1.05, R::track::length},
     {0.25, 0},
     {{3, 6}, 1, 0.4, 1},
     kNone,
     {1, {0, 1}, 16},
     {0.05, 0.4}},
    {PisteGrade::black,
     {kRed, R::track::max_grade},
     {980, 1160},
     {1, 0.08, 1.5, 0.12},
     {1.1, 1.2, 1.6, {1.6, 1.4}},
     {0.72, 0.78, {20, 34}, {1, 2}, 1.2, {2800, R::track::length.max}},
     {0.6, 0.3},
     {{4, 9}, 1.3, 0.62, 1.6},
     {2, 5},
     {2, {3, 5}, 5},
     {0.1, 0.35}},
}};

}  // namespace

const GradeRow& grade_row(std::optional<PisteGrade> id) {
  return id ? kGrades[static_cast<size_t>(*id)] : kUngraded;
}

const char* grade_name(PisteGrade grade) {
  switch (grade) {
    case PisteGrade::green:
      return "green";
    case PisteGrade::blue:
      return "blue";
    case PisteGrade::red:
      return "red";
    case PisteGrade::black:
      return "black";
  }
  return "black";
}

PisteGrade piste_grade_of(double steepest) {
  return steepest <= kGreen  ? PisteGrade::green
         : steepest <= kBlue ? PisteGrade::blue
         : steepest <= kRed  ? PisteGrade::red
                             : PisteGrade::black;
}

double steepest_span(const Track& track, std::span<const Kicker> kickers) {
  // A park's kickers (`trick`) are to be left out of the read; no kicker the generator lays
  // yet is one, so every window is read.
  (void)kickers;
  const auto& pts = track.points;
  const size_t n = pts.size();
  if (n < 2) return 0;
  const double step = track.length / static_cast<double>(n - 1);
  const double span = num::max(1.0, num::round(R::track::colour_window / step));
  const size_t k = static_cast<size_t>(span);
  double steepest = 0;
  for (size_t i = 0; i + k < n; i++) {
    steepest = num::max(steepest, (pts[i].y - pts[i + k].y) / (span * step));
  }
  return steepest;
}

Band vertical_band(const Region& region, const GradeRow& grade) {
  const double k = region.relief.vertical;
  if (!grade.id) return scale_band(grade.vertical, k);
  return scale_band(grade.vertical, 1 + (k - 1) * R::grade::region_share);
}

GradeRow scaled_row(const GradeRow& row, double length) {
  constexpr double kKickerReach = 2000;
  const double k = std::min(1.4, length / kKickerReach);
  const double least = !row.id || *row.id == PisteGrade::green ? 0 : 1;
  GradeRow out = row;
  out.kickers.on = {std::max(least, std::floor(row.kickers.on.min * k)),
                    std::max(least, num::round(row.kickers.on.max * k))};
  out.drops = {std::floor(row.drops.min * k), num::round(row.drops.max * k)};
  return out;
}

PisteGrade deal_grade(double seed) {
  constexpr uint32_t kGradeSalt = 0x6a7ade5;
  ge::core::Rng rng = ge::core::make_rng(num::to_uint32(seed) ^ kGradeSalt);
  const double u = ge::core::next(rng);
  double acc = 0;
  for (size_t i = 0; i < R::grade::odds.size(); i++) {
    acc += R::grade::odds[i];
    if (u < acc) return static_cast<PisteGrade>(i);
  }
  return PisteGrade::black;
}

}  // namespace ge::mountain::mapgen
