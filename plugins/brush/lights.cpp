// The light entities read off a map's entity text, and each as a bake's light.
//
// ge:summary Defines light_entities, bake_light_of and map_bake_lights
// ge:keywords bsp brush light-entity bake-light colour reach delay falloff spotlight cone
// ge:skill brush
// ge:test tests/brush_lights_test.cpp
#include "brush/lights.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "core/libm/libm.hpp"

namespace ge::brush {

namespace libm = core::libm;

namespace {

double number(const std::string& s, double fallback) {
  if (s.empty()) return fallback;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  return end == s.c_str() ? fallback : v;
}

constexpr double kDeg = 3.14159265358979323846 / 180;
// The distance, map units at a `wait` of 1, a falling light's value is matched at.
constexpr double kMatchUnits = 128;
// The light kinds by `delay`.
enum Falloff : uint8_t {
  kLinear = 0,
  kInverse = 1,
  kInverse2 = 2,
  kNone = 3,
  kFloor = 4,
  kInverse2Eased = 5
};

bool vec3(const std::string& s, double v[3]) {
  return std::sscanf(s.c_str(), "%lf %lf %lf", &v[0], &v[1], &v[2]) == 3;
}

// A direction in the file's frame to the engine's, unit length (false when it has none).
bool engine_dir(const double q[3], double out[3]) {
  double a[3], b[3];
  const float zero[3] = {0, 0, 0}, f[3] = {float(q[0]), float(q[1]), float(q[2])};
  to_engine(zero, a);
  to_engine(f, b);
  double len = 0;
  for (int k = 0; k < 3; ++k) len += (b[k] - a[k]) * (b[k] - a[k]);
  len = std::sqrt(len);
  if (!(len > 0)) return false;
  for (int k = 0; k < 3; ++k) out[k] = (b[k] - a[k]) / len;
  return true;
}

}  // namespace

std::vector<LightEntity> light_entities(const Map& map) {
  // Where each `targetname` stands, for the lights aimed at one.
  std::map<std::string, std::array<double, 3>> named;
  for (const Entity& e : map.entities) {
    double o[3];
    const std::string& name = e.get("targetname");
    if (!name.empty() && vec3(e.get("origin"), o)) named[name] = {o[0], o[1], o[2]};
  }
  std::vector<LightEntity> out;
  for (const Entity& e : map.entities) {
    const std::string& cls = e.get("classname");
    if (cls.rfind("light", 0) != 0) continue;
    double o[3];
    if (!vec3(e.get("origin"), o)) continue;
    const int falloff = int(number(e.get("delay"), kLinear));
    if (falloff == kFloor || !e.get("_surface").empty()) continue;
    LightEntity l;
    const float f[3] = {float(o[0]), float(o[1]), float(o[2])};
    to_engine(f, l.pos);
    l.value = number(e.get("light"), 300);
    const double wait = std::max(number(e.get("wait"), 1), 0.1);
    l.reach = l.value / wait * kUnitM;
    if (falloff != kLinear && l.value > 0) {
      // The value at kMatchUnits / wait units, matched there by an inverse square: the strength a
      // metre off, value / 128 times that distance squared, over the straight one's
      // (value / 256 times its half reach squared, value^3 kUnitM^2 / 1024).
      const double at = kMatchUnits / (falloff == kNone ? 1 : wait);
      l.gain = 8 * at * at / (l.value * l.value);
      if (falloff == kInverse2Eased) l.soft_m = at * kUnitM;
      // Its window (1.5 reaches) ends where it gives what a straight light's does at the end of
      // its own (value / 2304 of 128): 1.5 reaches is sqrt(18) times the match's distance.
      l.reach = std::min(std::sqrt(18.0) * at * kUnitM / 1.5, 64.0);
    }
    l.torch = cls.find("torch") != std::string::npos;
    l.flame = l.torch || cls.find("flame") != std::string::npos;
    l.large = l.flame && !l.torch && cls.find("large") != std::string::npos;
    l.style = int(number(e.get("style"), 0));
    static const double kFlame[3] = {1.0, 0.56, 0.24}, kBulb[3] = {1.0, 0.8, 0.58},
                        kTube[3] = {0.86, 0.95, 1.0};
    const double* kind = l.flame ? kFlame : cls.find("fluoro") != std::string::npos ? kTube : kBulb;
    double c[3] = {kind[0], kind[1], kind[2]}, given[3];
    // `_color`: the map's own, 0..255 or 0..1.
    if (vec3(e.get("_color"), given) && given[0] + given[1] + given[2] > 0) {
      const double scale = std::max({given[0], given[1], given[2]}) > 1 ? 1 / 255.0 : 1;
      for (int k = 0; k < 3; ++k) c[k] = std::max(given[k], 0.0) * scale;
    }
    const double luma = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
    for (int k = 0; k < 3; ++k) l.colour[k] = luma > 0 ? c[k] / luma : 1;
    // The spot: aimed by `mangle` or at its `target`.
    double aim[3], m[3];
    bool aimed = false;
    if (vec3(e.get("mangle"), m)) {
      const double yaw = m[0] * kDeg, pitch = m[1] * kDeg;
      const double q[3] = {libm::cos(pitch) * libm::cos(yaw), libm::cos(pitch) * libm::sin(yaw),
                           libm::sin(pitch)};
      aimed = engine_dir(q, aim);
    } else if (const auto t = named.find(e.get("target")); t != named.end()) {
      const double q[3] = {t->second[0] - o[0], t->second[1] - o[1], t->second[2] - o[2]};
      aimed = engine_dir(q, aim);
    }
    if (aimed) {
      const double cone = std::clamp(number(e.get("angle"), 40), 1.0, 179.0);
      double soft = number(e.get("_softangle"), 0);
      if (!(soft > 0 && soft < cone)) soft = 0.75 * cone;
      l.spot = true;
      for (int k = 0; k < 3; ++k) l.dir[k] = aim[k];
      l.cone_cos[0] = libm::cos(soft / 2 * kDeg);
      l.cone_cos[1] = libm::cos(cone / 2 * kDeg);
    }
    out.push_back(l);
  }
  return out;
}

bake::Light bake_light_of(const LightEntity& l, double strength) {
  bake::Light b;
  for (int k = 0; k < 3; ++k) b.pos[k] = l.pos[k];
  const double half = l.value * kUnitM / 2;
  const double at1 = l.value / 256.0 * half * half * l.gain * strength;
  for (int k = 0; k < 3; ++k) b.colour[k] = l.colour[k] * at1;
  b.reach = l.reach * 1.5;
  b.radius = std::max(0.2, l.soft_m);
  if (l.spot) {
    b.kind = bake::LightKind::spot;
    for (int k = 0; k < 3; ++k) b.dir[k] = l.dir[k];
    b.cone_cos[0] = l.cone_cos[0];
    b.cone_cos[1] = l.cone_cos[1];
  }
  return b;
}

std::vector<bake::Light> map_bake_lights(const Map& map, double strength) {
  std::vector<bake::Light> out;
  for (const LightEntity& l : light_entities(map)) out.push_back(bake_light_of(l, strength));
  return out;
}

}  // namespace ge::brush
