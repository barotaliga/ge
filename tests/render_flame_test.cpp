// engine/render/flame.hpp: a flame's light sits in its gas, flickers as the flame does with the
// flame's seed and is as bright as its luminance over its area; its bounds hold its gas and lean
// with the wind.
//
// ge:summary Tests for flames: the flame's light, its flicker and its bounds
// ge:keywords test render flame flame_light flame_level flame_bounds
// ge:skill render testing
#include <cmath>

#include "check.hpp"
#include "render/flame.hpp"
#include "render/lights.hpp"

namespace {

namespace r = ge::render;

const double kBase[3] = {3, 1, -2};

}  // namespace

TEST(a_flame_lights_as_it_burns) {
  const r::Flame f = r::torch_flame(kBase, 42);
  const r::LocalLight l = r::flame_light(f);
  CHECK(l.flicker == r::Flicker::flame && l.seed == 42 && l.rate_hz == f.rate_hz);
  // In its gas, low down.
  CHECK(std::fabs(l.position[0] - kBase[0]) < 1e-9 && l.position[1] > kBase[1] &&
        l.position[1] < kBase[1] + f.height_m * 0.5);
  // A torch's 40 to 200 cd; a candle's about one.
  CHECK(l.candela > 40 && l.candela < 200);
  const r::LocalLight c = r::flame_light(r::candle_flame(kBase, 1));
  CHECK(c.candela > 0.4f && c.candela < 3);
  CHECK(l.colour[0] > l.colour[1] && l.colour[1] > l.colour[2]);
  CHECK(l.range_m > 5 && l.range_m <= 40);
  for (int i = 0; i < 15; ++i) {
    const double t = i * 0.137;
    CHECK_BITS(r::flame_level(f, t), r::flicker_level(l, t));
  }
}

TEST(the_bounds_hold_the_gas_and_lean_downwind) {
  r::Flame f = r::torch_flame(kBase, 7);
  double lo[3], hi[3];
  r::flame_bounds(f, lo, hi);
  CHECK(lo[1] < kBase[1] && hi[1] >= kBase[1] + f.height_m * 1.3);
  CHECK(lo[0] <= kBase[0] - f.radius_m && hi[0] >= kBase[0] + f.radius_m);
  f.wind[0] = 2;  // a draught along +x
  double lo2[3], hi2[3];
  r::flame_bounds(f, lo2, hi2);
  CHECK(hi2[0] > hi[0] + 0.3 && std::fabs(lo2[0] - lo[0]) < 1e-9);
  // Its light leans with it.
  CHECK(r::flame_light(f).position[0] > kBase[0]);
}
