// LAB `bake`: THE LIGHT BAKE (engine/bake) LOOKED AT on a hall built here: stone flags, brick
// walls, a red wall, a pillar, a window the sun comes through, a torch on the brick and a pool
// of glowing slime. The hall is baked three ways and drawn by tracing a ray a pixel through the
// bake's own BVH, so the pictures need no GPU:
//
// - OLD: coarse luxels (`--old-luxel`), the direct light alone with hard shadows: what a
//   lightmap compiler of the old kind gives.
// - REBAKED: fine luxels (`--luxel`), soft shadows and `--bounces` passes of bounced light,
//   coloured by what it bounced off (the red wall's glow on the floor, the slime's green).
// - DIRECTIONAL: the rebake read with its light direction, so the bricks' and the flags' relief
//   (a height pattern drawn here) catches the torch and the sun from the side they come from;
//   the rebake without it shades the same relief flat.
//
// Each from the hall's corner and close on the brick wall by the torch, then the atlas itself
// (irradiance, and the direction as colour). Metrics: luxels, filled luxels, rays, the bounce's
// share of the floor's light and the red bleed near the red wall.
//
// ge:summary Lab: a hall baked old-style, rebaked with colour bounce, and read directionally
// ge:keywords lab bake lightmap rebake gi bounce directional normal-map relief brick torch sun
// ge:keywords before-after atlas luxel colour-bleed
// ge:skill render labs
// ge:test tests/bake_bake_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "bake/bake.hpp"
#include "bake/chart.hpp"
#include "bake/trace.hpp"
#include "core/libm/libm.hpp"
#include "lab/lab.hpp"

namespace {

namespace gb = ge::bake;
namespace gl = ge::lab;
using gl::Det;

// The hall's materials.
enum Mat : uint32_t { kFlags, kBrick, kRed, kPlaster, kSlime, kPillar, kMats };
// The relief each material's surface carries (none, bricks, flags).
enum class Relief { none, bricks, flags };

constexpr double kW = 12, kH = 5, kD = 8;  // the hall, m (x, y, z)

struct Hall {
  gb::Scene scene;
  Relief relief[kMats];
};

// A quad turned to face `toward` (or away from it when `away`), charted.
void face(gb::Scene& s, const double q[4][3], uint32_t m, double luxel, const double toward[3],
          bool away = false) {
  const double e1[3] = {q[1][0] - q[0][0], q[1][1] - q[0][1], q[1][2] - q[0][2]};
  const double e2[3] = {q[2][0] - q[0][0], q[2][1] - q[0][1], q[2][2] - q[0][2]};
  const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0]};
  const double to[3] = {toward[0] - q[0][0], toward[1] - q[0][1], toward[2] - q[0][2]};
  const bool facing = n[0] * to[0] + n[1] * to[1] + n[2] * to[2] > 0;
  double c[4][3];
  for (int i = 0; i < 4; i++) {
    const int j = facing != away ? i : 3 - i;
    for (int k = 0; k < 3; k++) c[i][k] = q[j][k];
  }
  const uint32_t t = gb::add_quad(s, c, m);
  const uint32_t tris[2] = {t, t + 1};
  gb::add_chart(s, tris, luxel);
}

// The hall at `luxel` m: inward walls with a window in the +x wall (z 3..5, y 1.6..3.6), a pillar,
// a slime pool, a torch and the sun.
Hall build_hall(double luxel) {
  Hall h;
  gb::Scene& s = h.scene;
  auto mat = [&](float r, float g, float b, float er = 0, float eg = 0, float eb = 0) {
    gb::Material m;
    m.albedo[0] = r;
    m.albedo[1] = g;
    m.albedo[2] = b;
    m.emit[0] = er;
    m.emit[1] = eg;
    m.emit[2] = eb;
    s.materials.push_back(m);
  };
  mat(0.42f, 0.40f, 0.37f);                     // flags
  mat(0.50f, 0.32f, 0.22f);                     // brick
  mat(0.70f, 0.12f, 0.08f);                     // red
  mat(0.62f, 0.60f, 0.55f);                     // plaster
  mat(0.10f, 0.30f, 0.08f, 0.15f, 1.6f, 0.2f);  // slime, glowing
  mat(0.55f, 0.52f, 0.48f);                     // pillar
  h.relief[kFlags] = Relief::flags;
  h.relief[kBrick] = Relief::bricks;
  h.relief[kRed] = Relief::none;
  h.relief[kPlaster] = Relief::none;
  h.relief[kSlime] = Relief::none;
  h.relief[kPillar] = Relief::bricks;

  const double floor[4][3] = {{0, 0, 0}, {0, 0, kD}, {kW, 0, kD}, {kW, 0, 0}};
  const double ceil[4][3] = {{0, kH, 0}, {kW, kH, 0}, {kW, kH, kD}, {0, kH, kD}};
  const double west[4][3] = {{0, 0, 0}, {0, kH, 0}, {0, kH, kD}, {0, 0, kD}};   // red, faces +x
  const double north[4][3] = {{0, 0, 0}, {kW, 0, 0}, {kW, kH, 0}, {0, kH, 0}};  // brick, faces +z
  const double south[4][3] = {{0, 0, kD}, {0, kH, kD}, {kW, kH, kD}, {kW, 0, kD}};  // plaster
  const double mid[3] = {kW / 2, kH / 2, kD / 2};
  face(s, floor, kFlags, luxel, mid);
  face(s, ceil, kPlaster, luxel, mid);
  face(s, west, kRed, luxel, mid);
  face(s, north, kBrick, luxel, mid);
  face(s, south, kPlaster, luxel, mid);
  // The east wall (faces -x) round the window: four quads.
  auto east = [&](double z0, double z1, double y0, double y1) {
    const double q[4][3] = {{kW, y0, z0}, {kW, y0, z1}, {kW, y1, z1}, {kW, y1, z0}};
    face(s, q, kBrick, luxel, mid);
  };
  east(0, 3, 0, kH);
  east(5, kD, 0, kH);
  east(3, 5, 0, 1.6);
  east(3, 5, 3.6, kH);
  // The window's reveal (the wall is 0.4 m thick): its sill, head and jambs.
  const double t = 0.4;
  const double sill[4][3] = {{kW, 1.6, 3}, {kW, 1.6, 5}, {kW + t, 1.6, 5}, {kW + t, 1.6, 3}};
  const double head[4][3] = {{kW, 3.6, 3}, {kW + t, 3.6, 3}, {kW + t, 3.6, 5}, {kW, 3.6, 5}};
  const double jamb0[4][3] = {{kW, 1.6, 3}, {kW + t, 1.6, 3}, {kW + t, 3.6, 3}, {kW, 3.6, 3}};
  const double jamb1[4][3] = {{kW, 1.6, 5}, {kW, 3.6, 5}, {kW + t, 3.6, 5}, {kW + t, 1.6, 5}};
  const double opening[3] = {kW + t / 2, 2.6, 4};
  face(s, sill, kPlaster, luxel, opening);
  face(s, head, kPlaster, luxel, opening);
  face(s, jamb0, kPlaster, luxel, opening);
  face(s, jamb1, kPlaster, luxel, opening);
  // The pillar: four outward sides from x 5..6, z 3.5..4.5.
  const double px0 = 5, px1 = 6, pz0 = 3.5, pz1 = 4.5;
  const double p1[4][3] = {{px0, 0, pz0}, {px0, kH, pz0}, {px1, kH, pz0}, {px1, 0, pz0}};
  const double p2[4][3] = {{px0, 0, pz1}, {px1, 0, pz1}, {px1, kH, pz1}, {px0, kH, pz1}};
  const double p3[4][3] = {{px0, 0, pz0}, {px0, 0, pz1}, {px0, kH, pz1}, {px0, kH, pz0}};
  const double p4[4][3] = {{px1, 0, pz0}, {px1, kH, pz0}, {px1, kH, pz1}, {px1, 0, pz1}};
  const double core[3] = {(px0 + px1) / 2, kH / 2, (pz0 + pz1) / 2};
  for (const auto* q : {&p1, &p2, &p3, &p4}) face(s, *q, kPillar, luxel, core, true);
  // The slime pool, a hair over the flags.
  const double pool[4][3] = {{2, 0.01, 5.6}, {2, 0.01, 7.4}, {4.4, 0.01, 7.4}, {4.4, 0.01, 5.6}};
  face(s, pool, kSlime, luxel, mid);

  gb::Light torch;
  torch.pos[0] = 3.0;
  torch.pos[1] = 2.6;
  torch.pos[2] = 0.45;
  torch.colour[0] = 3.5;
  torch.colour[1] = 2.1;
  torch.colour[2] = 0.9;
  torch.radius = 0.12;
  torch.reach = 10;
  s.lights.push_back(torch);
  gb::Light sun;
  sun.kind = gb::LightKind::sun;
  const double sd[3] = {-0.62, -0.55, 0.26};
  const double l = std::sqrt(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]);
  for (int k = 0; k < 3; k++) sun.dir[k] = sd[k] / l;
  sun.radius = 0.02;
  sun.colour[0] = 3.2;
  sun.colour[1] = 3.0;
  sun.colour[2] = 2.6;
  s.lights.push_back(sun);
  s.sky[0] = 0.55f;
  s.sky[1] = 0.70f;
  s.sky[2] = 1.0f;
  return h;
}

uint32_t hash2(int a, int b) {
  auto h = static_cast<uint32_t>(a) * 0x8da6b343u ^ static_cast<uint32_t>(b) * 0xd8163841u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return h;
}

// The relief's height (0 in a groove, 1 on a face) and a tint at plane coordinates (a, b), m.
double relief_height(Relief r, double a, double b, double* tint) {
  *tint = 1;
  if (r == Relief::none) return 1;
  const double bw = r == Relief::bricks ? 0.25 : 0.6;    // brick or flag size along a
  const double bh = r == Relief::bricks ? 0.075 : 0.45;  // and along b
  const double groove = r == Relief::bricks ? 0.012 : 0.02;
  const double bevel = r == Relief::bricks ? 0.012 : 0.03;
  const double row = std::floor(b / bh);
  const double shift = (static_cast<int64_t>(row) & 1) ? bw * 0.5 : 0;
  const double col = std::floor((a + shift) / bw);
  const double fa = a + shift - col * bw, fb = b - row * bh;
  const double edge = std::min(std::min(fa, bw - fa), std::min(fb, bh - fb));
  const uint32_t hv = hash2(static_cast<int>(col), static_cast<int>(row));
  *tint = 0.82 + 0.3 * ((hv & 1023) / 1023.0);
  if (edge < groove * 0.5) return 0;
  const double e = (edge - groove * 0.5) / bevel;
  return e >= 1 ? 1 : e * (2 - e);
}

struct Look {
  const Hall* hall;
  const gb::Bvh* bvh;
  const gb::Lightmap* map;
  bool directional;
  bool relief;
};

// The lightmap's irradiance and direction at the world point `p` on charted triangle `tri`.
void read_map(const Look& look, uint32_t tri, const double p[3], double e[3], double dir[3]) {
  const gb::Scene& s = look.hall->scene;
  const gb::Chart& ch = s.charts[s.tri_chart[tri]];
  double cs = 0, ct = 0;
  gb::chart_point(ch, p, &cs, &ct);
  cs = std::clamp(cs, 0.5, ch.width - 0.5);
  ct = std::clamp(ct, 0.5, ch.height - 0.5);
  const double fx = ch.x + cs - 0.5, fy = ch.y + ct - 0.5;
  const double x0f = std::floor(fx), y0f = std::floor(fy);
  const double tx = fx - x0f, ty = fy - y0f;
  const auto x0 = static_cast<size_t>(x0f), y0 = static_cast<size_t>(y0f);
  const size_t x1 = std::min<size_t>(x0 + 1, ch.x + ch.width - 1);
  const size_t y1 = std::min<size_t>(y0 + 1, ch.y + ch.height - 1);
  const size_t w = look.map->width;
  auto bil = [&](const std::vector<float>& v, int n, int k) {
    const double a = v[(y0 * w + x0) * n + k] * (1 - tx) + v[(y0 * w + x1) * n + k] * tx;
    const double b = v[(y1 * w + x0) * n + k] * (1 - tx) + v[(y1 * w + x1) * n + k] * tx;
    return a * (1 - ty) + b * ty;
  };
  for (int k = 0; k < 3; k++) {
    e[k] = bil(look.map->rgb, 3, k);
    dir[k] = bil(look.map->dir, 4, k);
  }
}

double tone(double x) {
  // Exposure, a soft shoulder and the sRGB curve.
  const double c = 1 - ge::core::libm::exp(-x * 1.6);
  return c <= 0.0031308 ? c * 12.92 : 1.055 * ge::core::libm::pow(c, 1 / 2.4) - 0.055;
}

// The hall from `eye` toward `at`, `w` x `h` pixels, a ray a pixel.
gl::Picture draw(const Look& look, const double eye[3], const double at[3], double fov_deg, int w,
                 int h) {
  gl::Picture pic = gl::create_picture(w, h, {0, 0, 0});
  const gb::Scene& s = look.hall->scene;
  double f[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
  double fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  for (double& v : f) v /= fl;
  // The engine's frame: facing +z, +x is to the right and y is up.
  double r[3] = {f[2], 0, -f[0]};
  fl = std::sqrt(r[0] * r[0] + r[2] * r[2]);
  r[0] /= fl;
  r[2] /= fl;
  const double u[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2],
                       f[0] * r[1] - f[1] * r[0]};
  const double th = ge::core::libm::tan(fov_deg * 3.141592653589793 / 360);
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      const double sx = ((px + 0.5) / w * 2 - 1) * th * w / h;
      const double sy = (1 - (py + 0.5) / h * 2) * th;
      double d[3];
      for (int k = 0; k < 3; k++) d[k] = f[k] + r[k] * sx + u[k] * sy;
      gb::Ray ray;
      for (int k = 0; k < 3; k++) {
        ray.o[k] = static_cast<float>(eye[k]);
        ray.d[k] = static_cast<float>(d[k]);
      }
      const gb::Hit hit = gb::closest_hit(*look.bvh, ray);
      double c[3];
      if (!hit.hit()) {
        for (int k = 0; k < 3; k++) c[k] = s.sky[k] * 1.4;
      } else {
        const uint32_t m = s.tri_material[hit.tri];
        const gb::Material& mat = s.materials[m];
        const gb::Chart& ch = s.charts[s.tri_chart[hit.tri]];
        double p[3];
        for (int k = 0; k < 3; k++) p[k] = eye[k] + d[k] * hit.t;
        double e[3], dir[3];
        read_map(look, hit.tri, p, e, dir);
        // The relief on the chart's own axes: a, b in metres along u and v.
        const double lu = std::sqrt(ch.u[0] * ch.u[0] + ch.u[1] * ch.u[1] + ch.u[2] * ch.u[2]);
        const double lv = std::sqrt(ch.v[0] * ch.v[0] + ch.v[1] * ch.v[1] + ch.v[2] * ch.v[2]);
        double tu[3], tv[3];
        for (int k = 0; k < 3; k++) {
          tu[k] = ch.u[k] / lu;
          tv[k] = ch.v[k] / lv;
        }
        // Walls lay their bricks level: b along the world's height where the wall is upright.
        const bool upright = std::fabs(ch.normal[1]) < 0.5;
        auto coords = [&](const double q[3], double* a, double* b) {
          if (upright) {
            *a = q[0] * tu[0] + q[2] * tu[2];
            *b = q[1];
          } else {
            *a = q[0] * tu[0] + q[1] * tu[1] + q[2] * tu[2];
            *b = q[0] * tv[0] + q[1] * tv[1] + q[2] * tv[2];
          }
        };
        const Relief rel = look.hall->relief[m];
        double a = 0, b = 0, tint = 1;
        coords(p, &a, &b);
        const double h0 = relief_height(rel, a, b, &tint);
        double n[3] = {ch.normal[0], ch.normal[1], ch.normal[2]};
        double shade = 1;
        if (look.relief && rel != Relief::none) {
          const double eps = 0.003, depth = rel == Relief::bricks ? 0.006 : 0.008;
          double tmp = 0;
          const double ha =
              relief_height(rel, a + eps, b, &tmp) - relief_height(rel, a - eps, b, &tmp);
          const double hb =
              relief_height(rel, a, b + eps, &tmp) - relief_height(rel, a, b - eps, &tmp);
          const double ga = -ha / (2 * eps) * depth, gb2 = -hb / (2 * eps) * depth;
          // The tangent along b: the world's up on an upright wall, v on a floor.
          double ta[3], tb[3];
          for (int k = 0; k < 3; k++) {
            ta[k] = upright ? tu[k] : tu[k];
            tb[k] = upright ? (k == 1 ? 1.0 : 0.0) : tv[k];
          }
          for (int k = 0; k < 3; k++) n[k] += ta[k] * ga + tb[k] * gb2;
          const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
          for (double& v : n) v /= nl;
          // A groove's own darkness (what the bake cannot know at its luxel size).
          shade = 0.55 + 0.45 * h0;
        }
        const double scale = look.directional ? gb::directional_scale(dir, n, ch.normal) : 1.0;
        for (int k = 0; k < 3; k++)
          c[k] = mat.albedo[k] * tint * shade * e[k] * scale + mat.emit[k];
      }
      gl::set_pixel(pic, px, py,
                    {static_cast<uint8_t>(std::clamp(tone(c[0]), 0.0, 1.0) * 255 + 0.5),
                     static_cast<uint8_t>(std::clamp(tone(c[1]), 0.0, 1.0) * 255 + 0.5),
                     static_cast<uint8_t>(std::clamp(tone(c[2]), 0.0, 1.0) * 255 + 0.5)});
    }
  }
  return pic;
}

// The atlas: irradiance on top, the direction (x, y, z as r, g, b about grey) below.
gl::Picture atlas_picture(const gb::Lightmap& m) {
  gl::Picture pic =
      gl::create_picture(static_cast<int>(m.width), static_cast<int>(m.height) * 2, {0, 0, 0});
  for (uint32_t y = 0; y < m.height; y++) {
    for (uint32_t x = 0; x < m.width; x++) {
      const size_t a = size_t{y} * m.width + x;
      gl::set_pixel(
          pic, static_cast<int>(x), static_cast<int>(y),
          {static_cast<uint8_t>(std::clamp(tone(m.rgb[a * 3] * 0.5), 0.0, 1.0) * 255),
           static_cast<uint8_t>(std::clamp(tone(m.rgb[a * 3 + 1] * 0.5), 0.0, 1.0) * 255),
           static_cast<uint8_t>(std::clamp(tone(m.rgb[a * 3 + 2] * 0.5), 0.0, 1.0) * 255)});
      auto ch = [&](int k) {
        return static_cast<uint8_t>(std::clamp(m.dir[a * 4 + k] * 0.5 + 0.5, 0.0, 1.0) * 255);
      };
      gl::set_pixel(pic, static_cast<int>(x), static_cast<int>(y + m.height),
                    m.dir[a * 4 + 3] > 0 ? gl::Rgba{ch(0), ch(1), ch(2)} : gl::Rgba{20, 20, 20});
    }
  }
  return pic;
}

// The floor's mean irradiance over its luxels' middle half, direct and in all.
void floor_means(const Hall& hall, const gb::Lightmap& m, double* direct, double* total) {
  const gb::Chart& ch = hall.scene.charts[0];
  double d = 0, t = 0;
  int n = 0;
  for (uint32_t j = ch.height / 4; j < ch.height * 3 / 4; j++) {
    for (uint32_t i = ch.width / 4; i < ch.width * 3 / 4; i++) {
      const size_t a = size_t{ch.y + j} * m.width + ch.x + i;
      d += 0.2126 * m.direct[a * 3] + 0.7152 * m.direct[a * 3 + 1] + 0.0722 * m.direct[a * 3 + 2];
      t += 0.2126 * m.rgb[a * 3] + 0.7152 * m.rgb[a * 3 + 1] + 0.0722 * m.rgb[a * 3 + 2];
      n++;
    }
  }
  *direct = n ? d / n : 0;
  *total = n ? t / n : 0;
}

// Red over green on the floor 0.5 m from the red wall, mid-hall.
double red_bleed(const Hall& hall, const gb::Lightmap& m) {
  const gb::Chart& ch = hall.scene.charts[0];
  const double p[3] = {0.5, 0, kD / 2};
  double s = 0, t = 0;
  gb::chart_point(ch, p, &s, &t);
  const size_t a =
      size_t{ch.y + static_cast<uint32_t>(t)} * m.width + ch.x + static_cast<uint32_t>(s);
  return m.rgb[a * 3 + 1] > 0 ? m.rgb[a * 3] / m.rgb[a * 3 + 1] : 0;
}

struct Baked {
  Hall hall;
  gb::Bvh bvh;
  gb::Lightmap map;
  gb::Stats stats;
  double ms = 0;
};

Baked bake(double luxel, const gb::Options& o) {
  Baked b{build_hall(luxel), {}, {}, {}, 0};
  gb::pack_charts(b.hall.scene, 512);
  const auto t0 = std::chrono::steady_clock::now();
  b.map = gb::bake_scene(b.hall.scene, o, &b.stats);
  b.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  b.bvh = gb::build_bvh(b.hall.scene);
  return b;
}

void run(gl::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = w * 5 / 8;
  gb::Options old_o;
  old_o.bounces = 0;
  old_o.light_samples = 1;
  old_o.filter = 0;
  gb::Options new_o;
  new_o.bounces = static_cast<int>(lab.arg_num("bounces"));
  new_o.samples = static_cast<int>(lab.arg_num("samples"));
  new_o.light_samples = 8;
  const Baked old_b = bake(lab.arg_num("old-luxel"), old_o);
  const Baked new_b = bake(lab.arg_num("luxel"), new_o);

  for (const auto* b : {&old_b, &new_b}) {
    const std::string k = b == &old_b ? "old" : "rebaked";
    lab.metric(k + ".luxels", static_cast<double>(b->stats.luxels), "count");
    lab.metric(k + ".filled", static_cast<double>(b->stats.filled), "count");
    lab.metric(k + ".rays", static_cast<double>(b->stats.rays), "count");
    lab.metric(k + ".atlas-height", b->map.height, "luxels");
    lab.metric(k + ".bake-ms", b->ms, "ms", Det::measured);
    double d = 0, t = 0;
    floor_means(b->hall, b->map, &d, &t);
    lab.metric(k + ".floor-direct", d, "");
    lab.metric(k + ".floor-total", t, "");
    lab.metric(k + ".floor-bounce-share", t > 0 ? (t - d) / t * 100 : 0, "%");
    lab.metric(k + ".red-bleed", red_bleed(b->hall, b->map), "r/g");
  }

  const double eye[3] = {10.8, 2.5, 7.6}, at[3] = {2.0, 1.0, 2.0};
  const double close_eye[3] = {4.8, 1.7, 1.5}, close_at[3] = {2.4, 2.1, 0};
  const std::vector<std::pair<std::string, std::string>> in = {{"old-luxel", lab.arg("old-luxel")},
                                                               {"luxel", lab.arg("luxel")},
                                                               {"bounces", lab.arg("bounces")},
                                                               {"samples", lab.arg("samples")}};
  struct View {
    const char* id;
    const char* title;
    const char* caption;
    const char* look_for;
    const Baked* b;
    bool directional, relief, close;
  };
  const View views[] = {
      {"hall-old", "THE HALL, OLD BAKE",
       "Direct light only at coarse luxels, hard shadows: the torch on the brick (left), the sun "
       "through the window (right), the slime pool glowing on its own.",
       "blocky shadow edges; shadowed areas near black; no colour from the red wall or the slime "
       "on anything round them",
       &old_b, false, true, false},
      {"hall-rebaked", "THE HALL, REBAKED",
       "Fine luxels, soft shadows and bounced light: the sun patch lights the room, the red wall "
       "tints the floor and pillar near it, the slime throws green on the flags.",
       "smooth shadow edges; no black corners; a red tint on the floor by the red wall; green "
       "round the pool",
       &new_b, false, true, false},
      {"hall-directional", "THE HALL, REBAKED AND DIRECTIONAL",
       "The same bake read with its light direction: the flags' and bricks' relief is lit from "
       "the side the sun and the torch are on.",
       "the flags' bevels bright on the sun's side and dark away from it; nothing else changes "
       "against the rebaked picture",
       &new_b, true, true, false},
      {"brick-old", "BRICK BY THE TORCH, OLD BAKE",
       "Close on the brick wall beside the torch, direct light at coarse luxels.",
       "stair-stepped light falloff along the luxels; the bricks' relief shaded flat", &old_b,
       false, true, true},
      {"brick-rebaked", "BRICK BY THE TORCH, REBAKED, FLAT",
       "Fine luxels with bounce, the relief shaded without the light's direction.",
       "smooth falloff; the bricks read only by their grooves' darkness, not by light", &new_b,
       false, true, true},
      {"brick-directional", "BRICK BY THE TORCH, REBAKED AND DIRECTIONAL",
       "The same bake read with its light direction: every brick's edges catch the torch from "
       "its side.",
       "each brick's edge toward the torch bright and the far edge dark, the effect strongest "
       "nearest the torch",
       &new_b, true, true, true},
  };
  for (const View& v : views) {
    Look look{&v.b->hall, &v.b->bvh, &v.b->map, v.directional, v.relief};
    const gl::Picture pic =
        v.close ? draw(look, close_eye, close_at, 50, w, h) : draw(look, eye, at, 72, w, h);
    lab.shot({v.id, v.title, v.caption, v.look_for, in}, pic);
  }
  lab.shot({"atlas", "THE REBAKED ATLAS",
            "Every chart of the hall packed in one atlas: the irradiance above, the light's "
            "direction below (x, y, z as red, green, blue about grey; dark grey where filled in).",
            "every chart a lit rectangle; green-heavy direction on floors (light from above), "
            "the torch's warm pool on the brick chart",
            in},
           gl::resized(atlas_picture(new_b.map), static_cast<int>(new_b.map.width),
                       static_cast<int>(new_b.map.height) * 2));
}

const gl::LabSpec kSpec{
    "bake",
    gl::LabKind::picture,
    "engine",
    "engine/bake/bake.hpp, engine/bake/chart.hpp, engine/bake/trace.hpp",
    "render",
    "What does a hall look like baked old-style, rebaked with colour bounce, and read "
    "directionally?",
    {
        {"old-luxel", "0.4", "the old bake's luxel, m"},
        {"luxel", "0.125", "the rebake's luxel, m"},
        {"bounces", "3", "the rebake's bounce passes"},
        {"samples", "128", "the rebake's gather rays a luxel a pass"},
        {"width", "640", "the pictures' width, px"},
    },
    "--luxel=0.5 --old-luxel=0.8 --samples=8 --bounces=1 --width=160",
    run,
};
GE_LAB(kSpec);

}  // namespace
