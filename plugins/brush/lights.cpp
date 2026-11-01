// The light entities read off a map's entity text, and each as a bake's light.
//
// ge:summary Defines light_entities, bake_light_of and map_bake_lights
// ge:keywords bsp brush light-entity bake-light colour reach
// ge:skill brush
// ge:test tests/brush_lights_test.cpp
#include "brush/lights.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ge::brush {

namespace {

double number(const std::string& s, double fallback) {
  if (s.empty()) return fallback;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  return end == s.c_str() ? fallback : v;
}

}  // namespace

std::vector<LightEntity> light_entities(const Map& map) {
  std::vector<LightEntity> out;
  for (const Entity& e : map.entities) {
    const std::string& cls = e.get("classname");
    if (cls.rfind("light", 0) != 0) continue;
    double o[3];
    if (std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &o[0], &o[1], &o[2]) != 3) continue;
    LightEntity l;
    const float f[3] = {float(o[0]), float(o[1]), float(o[2])};
    to_engine(f, l.pos);
    l.value = number(e.get("light"), 300);
    const double wait = std::max(number(e.get("wait"), 1), 0.1);
    l.reach = l.value / wait * kUnitM;
    l.torch = cls.find("torch") != std::string::npos;
    l.flame = l.torch || cls.find("flame") != std::string::npos;
    l.large = l.flame && !l.torch && cls.find("large") != std::string::npos;
    l.style = int(number(e.get("style"), 0));
    static const double kFlame[3] = {1.0, 0.56, 0.24}, kBulb[3] = {1.0, 0.8, 0.58},
                        kTube[3] = {0.86, 0.95, 1.0};
    const double* c = l.flame ? kFlame : cls.find("fluoro") != std::string::npos ? kTube : kBulb;
    const double luma = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
    for (int k = 0; k < 3; ++k) l.colour[k] = c[k] / luma;
    out.push_back(l);
  }
  return out;
}

bake::Light bake_light_of(const LightEntity& l, double strength) {
  bake::Light b;
  for (int k = 0; k < 3; ++k) b.pos[k] = l.pos[k];
  const double half = l.value * kUnitM / 2;
  const double at1 = l.value / 256.0 * half * half * strength;
  for (int k = 0; k < 3; ++k) b.colour[k] = l.colour[k] * at1;
  b.reach = l.reach * 1.5;
  b.radius = 0.2;
  return b;
}

std::vector<bake::Light> map_bake_lights(const Map& map, double strength) {
  std::vector<bake::Light> out;
  for (const LightEntity& l : light_entities(map)) out.push_back(bake_light_of(l, strength));
  return out;
}

}  // namespace ge::brush
