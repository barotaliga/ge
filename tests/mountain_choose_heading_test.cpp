// `choose_heading` (plugins/mountain/mapgen/network.cpp) against the long way: every heading's
// whole cost read off the ground and the cheapest taken, the first of equals in the table's
// order. The walk passes over a heading once what it has read already makes it dearer than the
// best so far, and that must never change a choice: thousands of points on rough ground, every
// swing the walks use and one they do not, with and without an aim and a zig-zag side.
//
// The long way restates the cost the walk weighs (its look-aheads, its heading step and the
// zig-zag's price, private to network.cpp); a change to the cost moves both.
//
// ge:summary Test: the run walk's heading choice agrees with weighing every heading in full
// ge:keywords test choose-heading heading-choice pruning piste-walk early-out
// ge:skill testing mountain
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace mg = ge::mountain::mapgen;
namespace R = ge::mountain::mapgen::rules;
namespace RR = ge::mountain::mapgen::resort_rules;
namespace libm = ge::core::libm;
namespace num = ge::core::num;
using ge::core::angle_diff;
using ge::core::sample_field;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kLookAhead[] = {20, 40, 60};
constexpr double kHeadingStep = 0.06;
constexpr double kZigzagSide = 0.8;

double long_way(const ge::core::Heightfield& ground, double x, double z, double want,
                double current, double cap, std::optional<double> aim, double swing, int side) {
  const double y0 = sample_field(ground, x, z);
  const double least = R::track::min_fall;
  const double w = R::track::grade_window;
  const double down =
      libm::atan2(-(sample_field(ground, x + w, z) - sample_field(ground, x - w, z)),
                  -(sample_field(ground, x, z + w) - sample_field(ground, x, z - w)));
  double best = want;
  double best_cost = kInf;
  for (double h = -swing; h <= swing + 1e-9; h += kHeadingStep) {
    const double fx = libm::sin(h);
    const double fz = libm::cos(h);
    double over = 0;
    double under = 0;
    double fall_sum = 0;
    for (const double L : kLookAhead) {
      const double fall = (y0 - sample_field(ground, x + fx * L, z + fz * L)) / L;
      over = num::max(over, fall - cap);
      under = num::max(under, least - fall);
      fall_sum += fall;
    }
    double cost = num::max(0.0, over) * 40 + num::max(0.0, under) * 40;
    if (aim) cost += std::abs(fall_sum / 3 - *aim) * 12;
    cost += std::abs(angle_diff(h, want)) * 0.35 + std::abs(angle_diff(h, current)) * 0.25;
    if (side != 0) {
      const double off = angle_diff(down, h);
      if (std::abs(off) > 0.12 && num::sign(off) != side) cost += kZigzagSide;
    }
    if (cost < best_cost) {
      best_cost = cost;
      best = h;
    }
  }
  return best;
}

// Rough ground falling toward +z: a few long waves, a fold and a random bump every cell.
ge::core::Heightfield rough_ground(ge::core::Rng& rng) {
  ge::core::Heightfield g = ge::core::create_heightfield(0, 0, 4, 160, 160);
  for (int r = 0; r < g.rows; r++) {
    for (int c = 0; c < g.cols; c++) {
      const double x = c * g.cell;
      const double z = r * g.cell;
      const double y = 400 - 0.35 * z + 18 * libm::sin(x / 37) * libm::cos(z / 53) +
                       9 * libm::sin((x + z) / 19) + 25 * std::abs(libm::sin(x / 90)) +
                       ge::core::range(rng, -0.6, 0.6);
      g.data[size_t(r) * size_t(g.cols) + size_t(c)] = static_cast<float>(y);
    }
  }
  return g;
}

}  // namespace

TEST(choose_heading_agrees_with_weighing_every_heading) {
  ge::core::Rng rng = ge::core::make_rng(0x4ead);
  const ge::core::Heightfield ground = rough_ground(rng);
  const double swings[] = {R::track::swing, RR::road::swing, 0.9};
  int turned = 0;
  for (int i = 0; i < 6000; i++) {
    const double x = ge::core::range(rng, 80, 560);
    const double z = ge::core::range(rng, 80, 560);
    const double swing = swings[i % 3];
    const double want = ge::core::range(rng, -swing, swing);
    const double current = ge::core::range(rng, -swing, swing);
    const double cap = ge::core::range(rng, 0.1, 0.9);
    const std::optional<double> aim =
        i % 2 ? std::optional<double>(ge::core::range(rng, 0.05, 0.5)) : std::nullopt;
    const int side = i % 5 == 0 ? 0 : i % 5 < 3 ? 1 : -1;
    const double got = mg::choose_heading(ground, x, z, want, current, cap, aim, swing, side);
    const double expect = long_way(ground, x, z, want, current, cap, aim, swing, side);
    CHECK_BITS(got, expect);
    if (got != want) turned++;
  }
  // The ground and the asks are rough enough that the choice is often not the heading wanted.
  CHECK(turned > 1000);
}
