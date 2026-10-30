// The map's fires: each fire's light entity to a flame at its model's flame, its level its light's.
//
// ge:summary The map's fires as flames: the preset by kind, the base, the light's level
// ge:keywords brush bsp torch flame fire bsp_fires
// ge:skill brush render
// ge:test tests/brush_bsp_flames_test.cpp
#include "brush/app/bsp_flames.hpp"

#include <algorithm>

namespace ge::brush::app {

double fire_light_lift(const PointLight& l) {
  if (l.fire == Fire::none) return 0;
  const bool torch = l.fire == Fire::wall_torch;
  return ((torch ? kTorchBase : kLargeBase) + 0.35 * (torch ? kTorchHeight : kLargeHeight)) *
         kUnitM;
}

double fire_light_size(const PointLight& l) {
  if (l.fire == Fire::none) return 0;
  return (l.fire == Fire::wall_torch ? kTorchHeight : kLargeHeight) * kUnitM;
}

void bsp_fires(const Scene& scene, double t_s, double brightness, std::vector<render::Flame>& out) {
  out.clear();
  for (const PointLight& l : scene.lights) {
    if (l.fire == Fire::none) continue;
    const bool torch = l.fire == Fire::wall_torch;
    double base[3] = {l.pos[0], l.pos[1] + (torch ? kTorchBase : kLargeBase) * kUnitM, l.pos[2]};
    render::Flame f =
        torch ? render::torch_flame(base, l.seed) : render::brazier_flame(base, l.seed);
    const double h = (torch ? kTorchHeight : kLargeHeight) * kUnitM;
    f.radius_m = static_cast<float>(f.radius_m * h / f.height_m);
    f.height_m = static_cast<float>(h);
    // The map's light flickers about 0.72 of its value (flicker_at's candle): the gas at full
    // height there.
    f.level = static_cast<float>(std::clamp(flicker_at(l, t_s) / 0.72, 0.0, 1.5));
    f.relative = static_cast<float>(kFireGlow * brightness);
    out.push_back(f);
  }
}

}  // namespace ge::brush::app
