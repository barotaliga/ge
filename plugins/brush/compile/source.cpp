// A map's source (source.hpp): entity keys, contents from texture names, the axial projection.
//
// ge:summary Defines the source's keys, contents from texture names and the axial projection
// ge:keywords map-compiler source contents texture-name axial projection box-brush
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/source.hpp"

#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::brush::mapc {

const std::string* Entity::get(const std::string& key) const {
  for (const auto& [k, v] : keys) {
    if (k == key) return &v;
  }
  return nullptr;
}

void Entity::set(const std::string& key, const std::string& value) {
  for (auto& [k, v] : keys) {
    if (k == key) {
      v = value;
      return;
    }
  }
  keys.emplace_back(key, value);
}

uint32_t contents_of_texture(const std::string& name) {
  const auto has = [&](const char* s) { return name.find(s) != std::string::npos; };
  const auto starts = [&](const char* s) { return name.rfind(s, 0) == 0; };
  if (starts("*")) return has("lava") ? kLava : has("slime") ? kSlime : kWater;
  if (starts("sky")) return kSky;
  if (name == "clip" || starts("clip")) return kClip;
  if (starts("trigger")) return kTrigger;
  return kEmpty;
}

void set_contents(Source* src) {
  for (Entity& e : src->entities) {
    for (Brush& b : e.brushes) {
      b.contents = kSolid;
      for (const Side& s : b.sides) {
        const uint32_t c = contents_of_texture(s.texture);
        if (c != kEmpty) {
          b.contents = c;
          break;
        }
      }
    }
  }
}

void axial_vecs(const V3& n, double shift_s, double shift_t, double turn, double scale_s,
                double scale_t, double out[2][4]) {
  // The face's nearest axis, floors and ceilings first on a tie.
  static const V3 kAxes[6][3] = {
      {{0, 0, 1}, {1, 0, 0}, {0, -1, 0}}, {{0, 0, -1}, {1, 0, 0}, {0, -1, 0}},
      {{1, 0, 0}, {0, 1, 0}, {0, 0, -1}}, {{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
      {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}},
  };
  size_t best = 0;
  double best_dot = -2;
  for (size_t i = 0; i < 6; ++i) {
    const double d = dot(n, kAxes[i][0]);
    if (d > best_dot + 1e-9) {
      best_dot = d;
      best = i;
    }
  }
  V3 s = kAxes[best][1], t = kAxes[best][2];
  if (turn != 0) {
    double sn = 0, cs = 1;
    const double q = turn / 90;
    if (q == std::nearbyint(q)) {
      const int k = ((int(q) % 4) + 4) % 4;
      sn = k == 1 ? 1 : k == 3 ? -1 : 0;
      cs = k == 0 ? 1 : k == 2 ? -1 : 0;
    } else {
      const double r = turn * 3.14159265358979323846 / 180;
      sn = core::libm::sin(r);
      cs = core::libm::cos(r);
    }
    const V3 s2 = sub(mul(s, cs), mul(t, sn));
    const V3 t2 = add(mul(s, sn), mul(t, cs));
    s = s2;
    t = t2;
  }
  if (scale_s == 0) scale_s = 1;
  if (scale_t == 0) scale_t = 1;
  for (size_t k = 0; k < 3; ++k) {
    out[0][k] = s[k] / scale_s;
    out[1][k] = t[k] / scale_t;
  }
  out[0][3] = shift_s;
  out[1][3] = shift_t;
}

Brush box_brush(const V3& lo, const V3& hi, const std::string& texture) {
  Brush b;
  for (size_t k = 0; k < 3; ++k) {
    for (int s = 0; s < 2; ++s) {
      Side side;
      side.plane.n = {0, 0, 0};
      side.plane.n[k] = s ? 1 : -1;
      side.plane.d = s ? hi[k] : -lo[k];
      side.texture = texture;
      axial_vecs(side.plane.n, 0, 0, 0, 1, 1, side.vecs);
      b.sides.push_back(side);
    }
  }
  b.contents = contents_of_texture(texture);
  if (b.contents == kEmpty) b.contents = kSolid;
  return b;
}

}  // namespace ge::brush::mapc
