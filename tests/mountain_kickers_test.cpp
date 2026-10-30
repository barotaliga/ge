// `on_kicker` and `on_cliff` (plugins/mountain/mapgen/kickers.cpp, cliffs.cpp) against the long
// way: every point turned into each box's own frame with its heading's sine and cosine. The two
// pass over a box whose reach on either axis cannot touch the point before they turn it, and that
// must never change an answer: points thrown all round each box, and right on its edges.
//
// ge:summary Test: the kicker and cliff footprint tests agree with the long way, early-outs and all
// ge:keywords test kicker cliff footprint on_kicker on_cliff early-out bounding-box
// ge:skill testing mountain
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/cliffs.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/rules.hpp"

namespace mg = ge::mountain::mapgen;
namespace R = ge::mountain::mapgen::rules;
namespace libm = ge::core::libm;

namespace {

bool kicker_long_way(const mg::Kicker& k, double x, double z, double margin) {
  const double dx = x - k.x, dz = z - k.z;
  const double fx = libm::sin(k.heading), fz = libm::cos(k.heading);
  const double u = dx * fx + dz * fz;
  const double v = std::abs(dx * fz - dz * fx);
  return u > -k.ramp - margin && u < k.landing + margin &&
         v < k.width / 2 + R::kickers::edge + margin;
}

bool cliff_long_way(const mg::Cliff& c, double x, double z, double margin) {
  const double back = c.shelf, ahead = c.face + c.landing + R::cliff::runout;
  const double side = c.width / 2 + R::cliff::edge;
  const double dx = x - c.x, dz = z - c.z;
  const double fx = libm::sin(c.heading), fz = libm::cos(c.heading);
  const double u = dx * fx + dz * fz;
  const double v = std::abs(dx * fz - dz * fx);
  return u > -back - margin && u < ahead + margin && v < side + margin;
}

}  // namespace

TEST(on_kicker_and_on_cliff_agree_with_the_long_way) {
  ge::core::Rng rng = ge::core::make_rng(0x6b1c);
  int inside = 0, checked = 0;
  for (int b = 0; b < 200; ++b) {
    mg::Kicker k{};
    k.x = ge::core::range(rng, -2000, 2000);
    k.z = ge::core::range(rng, -2000, 2000);
    k.heading = ge::core::range(rng, -7, 7);
    k.ramp = ge::core::range(rng, 2, 30);
    k.landing = ge::core::range(rng, 5, 60);
    k.width = ge::core::range(rng, 3, 20);
    mg::Cliff c{};
    c.x = k.x + ge::core::range(rng, -200, 200);
    c.z = k.z + ge::core::range(rng, -200, 200);
    c.heading = ge::core::range(rng, -7, 7);
    c.shelf = ge::core::range(rng, 2, 20);
    c.face = ge::core::range(rng, 3, 40);
    c.landing = ge::core::range(rng, 5, 50);
    c.width = ge::core::range(rng, 10, 120);
    const std::vector<mg::Kicker> ks{k};
    const std::vector<mg::Cliff> cs{c};
    for (int p = 0; p < 2000; ++p) {
      // Most points near the boxes' edges, where a wrong reach would show; some far off.
      const double r = p % 4 == 0 ? ge::core::range(rng, 0, 600) : ge::core::range(rng, 0, 160);
      const double a = ge::core::range(rng, 0, 7);
      const double margin = (p % 3) * 4.0;
      const double x = k.x + r * libm::sin(a), z = k.z + r * libm::cos(a);
      const bool want_k = kicker_long_way(k, x, z, margin);
      const bool want_c = cliff_long_way(c, x, z, margin);
      CHECK(mg::on_kicker(ks, x, z, margin) == want_k);
      CHECK(mg::on_cliff(cs, x, z, margin) == want_c);
      inside += want_k + want_c;
      ++checked;
    }
  }
  // The points must reach inside the boxes too, or the test holds nothing.
  CHECK(inside > checked / 50);
}
