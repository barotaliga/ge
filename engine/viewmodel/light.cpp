// The held weapon's light: the strongest point lights kept and turned into the view, and the
// surface shaded by them, the probe and the floor.
//
// ge:summary The held weapon's light: lights ranked into the view, wrapped Lambert and Blinn
// ge:keywords viewmodel light_held shade_held light_falloff world_to_view view_to_world
// ge:skill render
// ge:test tests/viewmodel_light_test.cpp
#include "viewmodel/light.hpp"

#include <cmath>

#include "core/quat.hpp"

namespace ge::viewmodel {

namespace {

// How far round the terminator a light reaches: a wrapped Lambert, max(0, (n.l + w) / (1 + w)).
constexpr float kWrap = 0.25f;

float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void normalise(float v[3]) {
  const float len = std::sqrt(dot3(v, v));
  const float inv = len > 0 ? 1.0f / len : 0.0f;
  for (int k = 0; k < 3; ++k) v[k] *= inv;
}

float power(float x, int n) {
  float r = 1;
  for (; n > 0; n >>= 1) {
    if (n & 1) r *= x;
    x *= x;
  }
  return r;
}

struct Basis {
  core::Vec3 eye, right, up, fwd;
};

Basis basis_of(const camera::Pose& v) {
  const core::Quat q = core::from_euler(v.yaw, v.pitch, v.roll);
  return {v.eye, core::rotate(q, core::Vec3{1, 0, 0}), core::rotate(q, core::Vec3{0, 1, 0}),
          core::rotate(q, core::Vec3{0, 0, 1})};
}

void to_view_dir(const Basis& b, const float w[3], float out[3]) {
  out[0] = static_cast<float>(w[0] * b.right.x + w[1] * b.right.y + w[2] * b.right.z);
  out[1] = static_cast<float>(w[0] * b.up.x + w[1] * b.up.y + w[2] * b.up.z);
  out[2] = static_cast<float>(w[0] * b.fwd.x + w[1] * b.fwd.y + w[2] * b.fwd.z);
}

void to_view_point(const Basis& b, const double w[3], float out[3]) {
  const double d[3] = {w[0] - b.eye.x, w[1] - b.eye.y, w[2] - b.eye.z};
  out[0] = static_cast<float>(d[0] * b.right.x + d[1] * b.right.y + d[2] * b.right.z);
  out[1] = static_cast<float>(d[0] * b.up.x + d[1] * b.up.y + d[2] * b.up.z);
  out[2] = static_cast<float>(d[0] * b.fwd.x + d[1] * b.fwd.y + d[2] * b.fwd.z);
}

}  // namespace

void world_to_view(const camera::Pose& view, const double world[3], float out[3]) {
  to_view_point(basis_of(view), world, out);
}

void world_to_view_dir(const camera::Pose& view, const float world[3], float out[3]) {
  to_view_dir(basis_of(view), world, out);
}

void view_to_world(const camera::Pose& view, const float in[3], double out[3]) {
  const Basis b = basis_of(view);
  out[0] = b.eye.x + in[0] * b.right.x + in[1] * b.up.x + in[2] * b.fwd.x;
  out[1] = b.eye.y + in[0] * b.right.y + in[1] * b.up.y + in[2] * b.fwd.y;
  out[2] = b.eye.z + in[0] * b.right.z + in[1] * b.up.z + in[2] * b.fwd.z;
}

float light_falloff(float d, float range_m, float radius_m) {
  if (range_m <= 0 || d >= range_m) return 0;
  const float q = d / range_m;
  const float q4 = q * q * q * q;
  const float w = (1 - q4) * (1 - q4);
  return w / (d * d + radius_m * radius_m);
}

Lighting light_held(const Probe& probe, std::span<const PointLight> lights,
                    const camera::Pose& view, const float at_view[3], float floor) {
  Lighting l;
  l.floor = floor;
  const Basis b = basis_of(view);
  for (int k = 0; k < 3; ++k) {
    l.ambient[k] = probe.ambient[k];
    l.direct[k] = probe.direct[k];
  }
  to_view_dir(b, probe.toward, l.toward);
  normalise(l.toward);

  // Rank by the light each one throws on the grip: an insertion into a short list, strongest
  // first, an equal one after the earlier index, so the choice is the same on every machine.
  float strength[kHeldLights] = {};
  for (size_t i = 0; i < lights.size(); ++i) {
    const PointLight& pl = lights[i];
    HeldLight h;
    to_view_point(b, pl.position, h.at);
    const float d[3] = {h.at[0] - at_view[0], h.at[1] - at_view[1], h.at[2] - at_view[2]};
    const float s = pl.intensity * light_falloff(std::sqrt(dot3(d, d)), pl.range_m, pl.radius_m) *
                    (pl.colour[0] + pl.colour[1] + pl.colour[2]);
    if (!(s > 0)) continue;
    int at = l.count;
    while (at > 0 && strength[at - 1] < s) --at;
    if (at >= kHeldLights) continue;
    const int last = l.count < kHeldLights ? l.count : kHeldLights - 1;
    for (int j = last; j > at; --j) {
      l.lights[j] = l.lights[j - 1];
      strength[j] = strength[j - 1];
    }
    for (int k = 0; k < 3; ++k) h.rgb[k] = pl.colour[k] * pl.intensity;
    h.range_m = pl.range_m;
    h.radius_m = pl.radius_m;
    l.lights[at] = h;
    strength[at] = s;
    if (l.count < kHeldLights) ++l.count;
  }
  return l;
}

void shade_held(const Lighting& l, const float p[3], const float n[3], float diffuse[3],
                float specular[3]) {
  // The eye is at the origin: the way back to it.
  float v[3] = {-p[0], -p[1], -p[2]};
  normalise(v);
  const float sky = 0.8f + 0.2f * n[1];
  for (int k = 0; k < 3; ++k) {
    diffuse[k] = l.ambient[k] * sky;
    specular[k] = 0;
  }
  auto add = [&](const float dir[3], const float rgb[3], float atten) {
    const float nl = dot3(n, dir);
    const float wrap = (nl + kWrap) / (1 + kWrap);
    if (wrap <= 0) return;
    float h[3] = {dir[0] + v[0], dir[1] + v[1], dir[2] + v[2]};
    normalise(h);
    const float nh = dot3(n, h);
    const float spec = nl > 0 && nh > 0 ? l.gloss * power(nh, l.shininess) * nl : 0.0f;
    for (int k = 0; k < 3; ++k) {
      diffuse[k] += rgb[k] * atten * wrap;
      specular[k] += rgb[k] * atten * spec;
    }
  };
  add(l.toward, l.direct, 1.0f);
  for (int i = 0; i < l.count; ++i) {
    const HeldLight& h = l.lights[i];
    float d[3] = {h.at[0] - p[0], h.at[1] - p[1], h.at[2] - p[2]};
    const float dist = std::sqrt(dot3(d, d));
    const float a = light_falloff(dist, h.range_m, h.radius_m);
    if (a <= 0) continue;
    normalise(d);
    add(d, h.rgb, a);
  }
  for (int k = 0; k < 3; ++k)
    if (diffuse[k] < l.floor) diffuse[k] = l.floor;
}

}  // namespace ge::viewmodel
