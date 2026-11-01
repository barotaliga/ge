// The map's fires: each fire's light entity to a flame at its model's flame, its level its light's;
// a large flame as a bonfire, its tongues, its air and its sparks; a caged one as a lantern.
//
// ge:summary The map's fires as flames: the preset by kind, the base, the light's level, bonfires
// ge:keywords brush bsp torch flame fire bsp_fires bonfire sparks place_fires fire_air lantern
// ge:skill brush render
// ge:test tests/brush_bsp_flames_test.cpp
#include "brush/app/bsp_flames.hpp"

#include <algorithm>
#include <cmath>

#include "render/emitters.hpp"

namespace ge::brush::app {

namespace {

// The base of a light's fire, engine m.
void base_of(const PointLight& l, double out[3]) {
  out[0] = l.pos[0];
  out[1] = l.pos[1] + (l.fire == Fire::wall_torch ? kTorchBase : kLargeBase) * kUnitM;
  out[2] = l.pos[2];
}

// A seed's phase in 0..2 pi, its `salt`th.
double phase(uint32_t seed, uint32_t salt) {
  uint32_t h = (seed ^ (salt * 0x9e3779b9u)) * 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;
  return h / 4294967296.0 * 6.283185307179586;
}

// What straight up from `p` (engine m) meets within `reach` m: the sky (2), a wall (1) or neither
// (0).
int up_from(const Map& map, double x, double y, double z, double reach) {
  const int steps = static_cast<int>(reach / 0.25);
  for (int k = 0; k <= steps; ++k) {
    const float f[3] = {float(x / kUnitM), float(z / kUnitM), float((y + 0.25 * k) / kUnitM)};
    const Leaf& l = map.leaves[leaf_at(map, f)];
    if (l.contents == -6) return 2;
    if (l.solid || l.contents == -2) return 1;
  }
  return 0;
}

}  // namespace

bool is_bonfire(const PointLight& l, const FireLook& look) {
  return look.bonfires && l.fire == Fire::large;
}

double fire_height(const PointLight& l, bool bonfires) {
  switch (l.fire) {
    case Fire::none:
      return 0;
    case Fire::wall_torch:
      return kTorchHeight;
    case Fire::large:
      return bonfires ? kBonfireHeight : kLargeHeight;
    case Fire::lantern:
      return bonfires ? kLanternHeight : kLargeHeight;
    case Fire::flame:
      break;
  }
  return kLargeHeight;
}

double fire_light_lift(const PointLight& l, bool bonfires) {
  if (l.fire == Fire::none) return 0;
  const double base = l.fire == Fire::wall_torch ? kTorchBase : kLargeBase;
  return (base + 0.35 * fire_height(l, bonfires)) * kUnitM;
}

double fire_light_size(const PointLight& l, bool bonfires) {
  return fire_height(l, bonfires) * kUnitM;
}

double fire_light_gain(const PointLight& l, bool bonfires) {
  if (!bonfires) return 1;
  return l.fire == Fire::large ? kBonfireLight : l.fire == Fire::lantern ? kLanternLight : 1.0;
}

void place_fires(Scene& scene) {
  if (scene.map.leaves.empty() || scene.map.nodes.empty()) return;
  // The flame's middle and four columns round it, at the reach of a bonfire's tongues and past.
  constexpr double kRound[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (PointLight& l : scene.lights) {
    if (l.fire == Fire::none) continue;
    double b[3];
    base_of(l, b);
    int open = 0;
    bool room = true;
    for (const auto& r : kRound) {
      const double x = b[0] + r[0] * kBonfireReach, z = b[2] + r[1] * kBonfireReach;
      if (up_from(scene.map, x, b[1] + 0.3, z, kBonfireRoom) == 1) room = false;
      open += up_from(scene.map, x, b[1] + 0.3, z, 200) == 2;
    }
    l.sky = static_cast<float>(open / 5.0);
    if (l.fire == Fire::large && !room) l.fire = Fire::lantern;
  }
}

void fire_air(const PointLight& l, const FireLook& look, double t, double out[3]) {
  const double open = l.sky;
  const double a = phase(l.seed, 1), b = phase(l.seed, 2);
  // Two slow swings, a few seconds each, so no two fires sway together.
  out[0] = look.air[0] * open + look.draught * std::sin(0.61 * t + a);
  out[1] = look.air[1] * open;
  out[2] = look.air[2] * open + look.draught * std::sin(0.47 * t + b);
}

void bsp_fires(const Scene& scene, double t_s, double brightness, std::vector<render::Flame>& out,
               const FireLook& look) {
  out.clear();
  for (size_t i = 0; i < scene.lights.size(); ++i) {
    const PointLight& l = scene.lights[i];
    if (l.fire == Fire::none) continue;
    const bool torch = l.fire == Fire::wall_torch;
    const bool bonfire = is_bonfire(l, look);
    double base[3];
    base_of(l, base);
    // The map's light flickers about 0.72 of its value (flicker_at's candle): the gas at full
    // height there.
    const auto level = static_cast<float>(std::clamp(flicker_at(l, t_s) / 0.72, 0.0, 1.5));
    const auto relative = static_cast<float>(kFireGlow * brightness);
    if (!bonfire) {
      render::Flame f =
          torch ? render::torch_flame(base, l.seed) : render::brazier_flame(base, l.seed);
      const double tall = fire_height(l, look.bonfires);
      const double h = tall * kUnitM;
      // A lantern's flame grows fatter than it grows tall: it fills its cage.
      const double fat = tall > kLargeHeight ? kLanternWidth * kLargeHeight / tall : 1.0;
      f.radius_m = static_cast<float>(f.radius_m * h / f.height_m * fat);
      f.height_m = static_cast<float>(h);
      f.level = level;
      f.relative = relative;
      out.push_back(f);
      continue;
    }
    double air[3];
    fire_air(l, look, t_s, air);
    // The heap's main body, and four lesser tongues round its bed, turned by the seed: a heap
    // burns in several flames, never one.
    constexpr double kTongue[5] = {1.0, 0.62, 0.5, 0.7, 0.45};  // each one's height, a share
    const double turn = phase(l.seed, 3);
    for (int k = 0; k < 5; ++k) {
      double at[3] = {base[0], base[1], base[2]};
      if (k > 0) {
        const double a = turn + 1.5707963267948966 * k;
        at[0] += kBonfireBed * std::cos(a);
        at[2] += kBonfireBed * std::sin(a);
      }
      render::Flame f = render::bonfire_flame(at, l.seed * 5u + uint32_t(k));
      const double h = kTongue[k] * kBonfireHeight * kUnitM;
      f.radius_m = static_cast<float>(f.radius_m * (k == 0 ? 1.3 : 0.75 * kTongue[k]));
      f.height_m = static_cast<float>(h);
      for (int a = 0; a < 3; ++a) f.wind[a] = static_cast<float>(air[a]);
      f.level = level;
      f.relative = relative;
      out.push_back(f);
    }
  }
}

void bonfire_sparks(const Scene& scene, double t0_s, double t1_s, const FireLook& look,
                    std::vector<render::Particle>& out) {
  if (!look.bonfires) return;
  for (size_t i = 0; i < scene.lights.size(); ++i) {
    const PointLight& l = scene.lights[i];
    if (!is_bonfire(l, look)) continue;
    render::AreaEmitter e;
    e.kind = render::AreaEmitter::Kind::sparks;
    base_of(l, e.centre);
    e.half[0] = e.half[2] = 0.3;
    e.half[1] = 0.25;
    e.centre[1] += e.half[1];
    e.rate = kBonfireSparks;
    e.seed = l.seed * 7u + 1u;
    fire_air(l, look, t1_s, e.air);
    render::emit_area(e, t0_s, t1_s, out);
  }
}

}  // namespace ge::brush::app
