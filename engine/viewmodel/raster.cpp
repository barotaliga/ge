// The held model's CPU raster: the near-plane clip, the perspective-correct triangle fill with
// its depth test and per-pixel light, and the resolve to sRGB bytes.
//
// ge:summary The held model's CPU raster: near clip, triangle fill, per-pixel light, resolve
// ge:keywords viewmodel raster draw_lit resolve make_target clear_depth place_held srgb-table
// ge:skill render labs
// ge:test tests/viewmodel_raster_test.cpp
#include "viewmodel/raster.hpp"

#include <array>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "viewmodel/place.hpp"

namespace ge::viewmodel {

namespace libm = core::libm;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();

// A vertex on its way to the screen: view position, normal, and its colour or its uv.
struct Vert {
  float p[3], n[3], a[3];
};

Vert lerp_vert(const Vert& x, const Vert& y, float t) {
  Vert o;
  for (int k = 0; k < 3; ++k) {
    o.p[k] = x.p[k] + (y.p[k] - x.p[k]) * t;
    o.n[k] = x.n[k] + (y.n[k] - x.n[k]) * t;
    o.a[k] = x.a[k] + (y.a[k] - x.a[k]) * t;
  }
  return o;
}

// The polygon cut to z >= near: at most four vertices from three.
int clip_near(const Vert in[3], float near_m, Vert out[4]) {
  int n = 0;
  for (int i = 0; i < 3; ++i) {
    const Vert& a = in[i];
    const Vert& b = in[(i + 1) % 3];
    const bool ain = a.p[2] >= near_m, bin = b.p[2] >= near_m;
    if (ain) out[n++] = a;
    if (ain != bin) out[n++] = lerp_vert(a, b, (near_m - a.p[2]) / (b.p[2] - a.p[2]));
  }
  return n;
}

// The sRGB curve from linear 0..1 in kSteps steps, built once with the engine's libm.
constexpr int kSteps = 4096;
const std::array<uint8_t, kSteps + 1>& srgb_table() {
  static const std::array<uint8_t, kSteps + 1> table = [] {
    std::array<uint8_t, kSteps + 1> t{};
    for (int i = 0; i <= kSteps; ++i) {
      const double x = double(i) / kSteps;
      const double s = x <= 0.0031308 ? 12.92 * x : 1.055 * libm::pow(x, 1.0 / 2.4) - 0.055;
      t[i] = static_cast<uint8_t>(std::lround(s * 255.0));
    }
    return t;
  }();
  return table;
}

struct Screen {
  float x, y, inv_z;
};

}  // namespace

ViewTarget make_target(int width, int height, int supersample, const float background[3]) {
  ViewTarget t;
  t.supersample = supersample < 1 ? 1 : supersample > 4 ? 4 : supersample;
  t.width = width * t.supersample;
  t.height = height * t.supersample;
  const size_t n = size_t(t.width) * size_t(t.height);
  t.rgb.resize(n * 3);
  for (size_t i = 0; i < n; ++i)
    for (int k = 0; k < 3; ++k) t.rgb[i * 3 + k] = background[k];
  t.depth.assign(n, kInf);
  return t;
}

void clear_depth(ViewTarget& t) { t.depth.assign(t.depth.size(), kInf); }

void draw_lit(ViewTarget& t, const DrawLit& d, const Lighting& light) {
  const float tan_y = static_cast<float>(libm::tan(d.fov_y * 0.5));
  const float tan_x = tan_y * float(t.width) / float(t.height);
  const float hw = 0.5f * float(t.width), hh = 0.5f * float(t.height);
  const bool skinned = d.skin && !d.skin->image.rgba.empty() && d.uv.size() >= 2;
  const assets::Image* img = skinned ? &d.skin->image : nullptr;
  const bool glow = skinned && !d.skin->glow.empty();
  const float near_m = static_cast<float>(d.near_m);

  for (size_t tri = 0; tri + 2 < d.indices.size(); tri += 3) {
    if (tri / 3 < d.hidden.size() && d.hidden[tri / 3] != 0) continue;
    Vert v[3];
    for (int c = 0; c < 3; ++c) {
      const uint32_t i = d.indices[tri + c];
      for (int k = 0; k < 3; ++k) {
        v[c].p[k] = d.positions[size_t(i) * 3 + k];
        v[c].n[k] = d.normals[size_t(i) * 3 + k];
        v[c].a[k] = 0;
      }
      if (skinned) {
        v[c].a[0] = d.uv[size_t(i) * 2];
        v[c].a[1] = d.uv[size_t(i) * 2 + 1];
      } else if (d.colour.size() >= size_t(i) * 3 + 3) {
        for (int k = 0; k < 3; ++k) v[c].a[k] = d.colour[size_t(i) * 3 + k];
      } else {
        for (int k = 0; k < 3; ++k) v[c].a[k] = 0.6f;
      }
    }
    if (d.cull_back) {
      // The front is the side (b - a) x (c - a) points to; the eye sits at the origin.
      const float e1[3] = {v[1].p[0] - v[0].p[0], v[1].p[1] - v[0].p[1], v[1].p[2] - v[0].p[2]};
      const float e2[3] = {v[2].p[0] - v[0].p[0], v[2].p[1] - v[0].p[1], v[2].p[2] - v[0].p[2]};
      const float cx = e1[1] * e2[2] - e1[2] * e2[1];
      const float cy = e1[2] * e2[0] - e1[0] * e2[2];
      const float cz = e1[0] * e2[1] - e1[1] * e2[0];
      if (-(cx * v[0].p[0] + cy * v[0].p[1] + cz * v[0].p[2]) <= 0) continue;
    }
    Vert poly[4];
    const int pn = clip_near(v, near_m, poly);
    if (pn < 3) continue;
    Screen s[4];
    for (int i = 0; i < pn; ++i) {
      const float iz = 1.0f / poly[i].p[2];
      s[i] = {(poly[i].p[0] * iz / tan_x + 1) * hw, (1 - poly[i].p[1] * iz / tan_y) * hh, iz};
    }
    for (int f = 1; f + 1 < pn; ++f) {
      const int id[3] = {0, f, f + 1};
      const Screen& a = s[id[0]];
      const Screen& b = s[id[1]];
      const Screen& c = s[id[2]];
      const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
      if (area == 0) continue;
      const float inv_area = 1.0f / area;
      int x0 = int(std::floor(std::fmin(a.x, std::fmin(b.x, c.x))));
      int x1 = int(std::ceil(std::fmax(a.x, std::fmax(b.x, c.x))));
      int y0 = int(std::floor(std::fmin(a.y, std::fmin(b.y, c.y))));
      int y1 = int(std::ceil(std::fmax(a.y, std::fmax(b.y, c.y))));
      x0 = x0 < 0 ? 0 : x0;
      y0 = y0 < 0 ? 0 : y0;
      x1 = x1 > t.width ? t.width : x1;
      y1 = y1 > t.height ? t.height : y1;
      for (int y = y0; y < y1; ++y) {
        const float py = float(y) + 0.5f;
        for (int x = x0; x < x1; ++x) {
          const float px = float(x) + 0.5f;
          float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) * inv_area;
          float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) * inv_area;
          float w2 = 1 - w0 - w1;
          if (w0 < 0 || w1 < 0 || w2 < 0) continue;
          const float iz = w0 * a.inv_z + w1 * b.inv_z + w2 * c.inv_z;
          const float z = 1.0f / iz;
          const size_t at = size_t(y) * size_t(t.width) + size_t(x);
          if (z >= t.depth[at]) continue;
          t.depth[at] = z;
          // Perspective-correct weights.
          w0 *= a.inv_z * z;
          w1 *= b.inv_z * z;
          w2 *= c.inv_z * z;
          Vert q;
          const Vert& va = poly[id[0]];
          const Vert& vb = poly[id[1]];
          const Vert& vc = poly[id[2]];
          for (int k = 0; k < 3; ++k) {
            q.p[k] = va.p[k] * w0 + vb.p[k] * w1 + vc.p[k] * w2;
            q.n[k] = va.n[k] * w0 + vb.n[k] * w1 + vc.n[k] * w2;
            q.a[k] = va.a[k] * w0 + vb.a[k] * w1 + vc.a[k] * w2;
          }
          const float len = std::sqrt(q.n[0] * q.n[0] + q.n[1] * q.n[1] + q.n[2] * q.n[2]);
          if (len > 0)
            for (int k = 0; k < 3; ++k) q.n[k] /= len;
          float base[3];
          bool lit_self = false;
          if (img) {
            const uint8_t* px8 = assets::texel(*img, q.a[0], q.a[1]);
            for (int k = 0; k < 3; ++k) base[k] = assets::srgb_to_linear(px8[k]);
            if (glow) {
              const size_t ti = size_t(px8 - img->rgba.data()) / 4;
              lit_self = ti < d.skin->glow.size() && d.skin->glow[ti] != 0;
            }
          } else {
            for (int k = 0; k < 3; ++k) base[k] = q.a[k];
          }
          float* out = &t.rgb[at * 3];
          if (lit_self) {
            for (int k = 0; k < 3; ++k) out[k] = base[k];
            continue;
          }
          float diff[3], spec[3];
          shade_held(light, q.p, q.n, diff, spec);
          for (int k = 0; k < 3; ++k) out[k] = base[k] * diff[k] + spec[k];
        }
      }
    }
  }
}

std::vector<uint8_t> resolve(const ViewTarget& t, double exposure) {
  const int ss = t.supersample;
  const int w = t.width / ss, h = t.height / ss;
  const auto& table = srgb_table();
  std::vector<uint8_t> out(size_t(w) * size_t(h) * 4);
  const float scale = static_cast<float>(exposure) / float(ss * ss);
  constexpr float kKnee = 0.8f;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sum[3] = {0, 0, 0};
      for (int j = 0; j < ss; ++j)
        for (int i = 0; i < ss; ++i) {
          const size_t at = (size_t(y * ss + j) * size_t(t.width) + size_t(x * ss + i)) * 3;
          for (int k = 0; k < 3; ++k) sum[k] += t.rgb[at + k];
        }
      uint8_t* o = &out[(size_t(y) * size_t(w) + size_t(x)) * 4];
      for (int k = 0; k < 3; ++k) {
        float v = sum[k] * scale;
        if (v > kKnee) {
          const float e = v - kKnee;
          v = kKnee + e / (1 + e / (1 - kKnee));
        }
        v = v < 0 ? 0 : v > 1 ? 1 : v;
        o[k] = table[size_t(std::lround(v * kSteps))];
      }
      o[3] = 255;
    }
  }
  return out;
}

void place_held(const HeldModel& model, const Placement& place, int a, int b, double t,
                std::vector<float>& out_positions, std::vector<float>& out_normals) {
  std::vector<float> p, n;
  pose_frames(model, a, b, t, p, n);
  out_positions.resize(p.size());
  out_normals.resize(n.size());
  for (size_t i = 0; i + 2 < p.size(); i += 3) {
    place_point(place, &p[i], &out_positions[i]);
    place_vector(place, &n[i], &out_normals[i]);
  }
}

}  // namespace ge::viewmodel
