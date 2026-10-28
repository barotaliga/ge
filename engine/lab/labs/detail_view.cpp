// The detail labs' view: the near-plane clip, the perspective-correct triangle walk, the per-pixel
// light, the decal blend, the depth-tested lines, the compose and the hall's materials.
//
// ge:summary Detail labs' CPU view: clip, raster, light, decal blend, lines, compose, hall paint
// ge:keywords lab detail view raster near-plane perspective-correct lighting decal paint ashlar
// ge:skill detail labs
#include "detail_view.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"

namespace ge::lab::detail_view {
namespace {

constexpr double kNear = 0.05;
constexpr double kInf = std::numeric_limits<double>::infinity();

// A vertex on its way to the screen: its world place and four values carried with it.
struct V {
  gd::Vec3 world;
  double a[4];
};

struct Screen {
  double x, y, z;  // pixels, and view depth
};

gd::Vec3 to_view(const Canvas& c, const gd::Vec3& p) {
  const gd::Vec3 d = gd::sub(p, c.cam.eye);
  return {gd::dot(d, c.right), gd::dot(d, c.up), gd::dot(d, c.fwd)};
}

Screen to_screen(const Canvas& c, const gd::Vec3& v) {
  const double W = c.w * c.ss, H = c.h * c.ss;
  if (c.cam.fov_deg <= 0) {
    const double k = H * 0.5 / c.cam.half_m;
    return {W * 0.5 + v.x * k, H * 0.5 - v.y * k, v.z};
  }
  return {W * 0.5 + v.x / v.z * c.focal, H * 0.5 - v.y / v.z * c.focal, v.z};
}

V lerp_v(const V& a, const V& b, double t) {
  V r;
  r.world = gd::mad(a.world, gd::sub(b.world, a.world), t);
  for (int k = 0; k < 4; ++k) r.a[k] = a.a[k] + (b.a[k] - a.a[k]) * t;
  return r;
}

// The triangle walked pixel by pixel; `fn(index, z, vertex)` gets every covered sample with its
// values interpolated in perspective. Cut at the near plane first.
template <class Fn>
void raster(const Canvas& c, const V& v0, const V& v1, const V& v2, Fn&& fn) {
  V poly[4], next[4];
  int n = 0;
  const V in[3] = {v0, v1, v2};
  double z[3];
  for (int k = 0; k < 3; ++k) z[k] = to_view(c, in[k].world).z - kNear;
  for (int k = 0; k < 3; ++k) {
    const int j = (k + 1) % 3;
    if (z[k] >= 0) poly[n++] = in[k];
    if ((z[k] >= 0) != (z[j] >= 0)) poly[n++] = lerp_v(in[k], in[j], z[k] / (z[k] - z[j]));
  }
  (void)next;
  if (n < 3) return;
  Screen s[4];
  for (int k = 0; k < n; ++k) s[k] = to_screen(c, to_view(c, poly[k].world));
  const int W = c.w * c.ss, H = c.h * c.ss;
  const bool ortho = c.cam.fov_deg <= 0;
  for (int t = 1; t + 1 < n; ++t) {
    const Screen& A = s[0];
    const Screen& B = s[t];
    const Screen& C = s[t + 1];
    const double area = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    if (area == 0) continue;
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min({A.x, B.x, C.x}))));
    const int x1 = std::min(W - 1, static_cast<int>(std::ceil(std::max({A.x, B.x, C.x}))));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min({A.y, B.y, C.y}))));
    const int y1 = std::min(H - 1, static_cast<int>(std::ceil(std::max({A.y, B.y, C.y}))));
    const double ia = ortho ? 1 : 1 / A.z, ib = ortho ? 1 : 1 / B.z, ic = ortho ? 1 : 1 / C.z;
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const double px = x + 0.5, py = y + 0.5;
        double wa = ((B.x - px) * (C.y - py) - (B.y - py) * (C.x - px)) / area;
        double wb = ((C.x - px) * (A.y - py) - (C.y - py) * (A.x - px)) / area;
        double wc = 1 - wa - wb;
        if (wa < 0 || wb < 0 || wc < 0) continue;
        // Perspective: the values over z are linear on the screen.
        const double q = wa * ia + wb * ib + wc * ic;
        wa *= ia / q;
        wb *= ib / q;
        wc *= ic / q;
        V v;
        const V& P = poly[0];
        const V& Q = poly[t];
        const V& R = poly[t + 1];
        v.world =
            gd::add(gd::add(gd::mul(P.world, wa), gd::mul(Q.world, wb)), gd::mul(R.world, wc));
        for (int k = 0; k < 4; ++k) v.a[k] = P.a[k] * wa + Q.a[k] * wb + R.a[k] * wc;
        const double depth = ortho ? A.z * wa + B.z * wb + C.z * wc : 1 / q;
        fn(static_cast<size_t>(y) * W + x, depth, v);
      }
    }
  }
}

// x^32, by squaring.
double pow32(double x) {
  for (int i = 0; i < 5; ++i) x *= x;
  return x;
}

void shade_pixel(Canvas& c, size_t i, double z, const gd::Vec3& p, const gd::Vec3& n,
                 const Material& m) {
  if (z >= c.depth[i]) return;
  c.depth[i] = z;
  const Light& L = c.light;
  const gd::Vec3 to_eye =
      c.cam.fov_deg <= 0 ? gd::mul(c.fwd, -1) : gd::normalized(gd::sub(c.cam.eye, p));
  const double sky = L.ambient * (0.75 + 0.25 * n.y);
  const double key = L.key * std::max(0.0, gd::dot(n, L.key_dir));
  const gd::Vec3 to_lamp = gd::sub(L.lamp, p);
  const double d2 = gd::dot(to_lamp, to_lamp);
  const gd::Vec3 ld = gd::mul(to_lamp, 1 / std::sqrt(d2));
  const double lamp = L.lamp_power / (d2 + 0.5) * std::max(0.0, gd::dot(n, ld));
  c.diffuse[i * 3] = static_cast<float>(sky * 0.75 + key * 1.0 + lamp * 1.0);
  c.diffuse[i * 3 + 1] = static_cast<float>(sky * 0.85 + key * 0.96 + lamp * 0.72);
  c.diffuse[i * 3 + 2] = static_cast<float>(sky * 1.05 + key * 0.9 + lamp * 0.45);
  const double hk = std::max(0.0, gd::dot(n, gd::normalized(gd::add(L.key_dir, to_eye))));
  const double hl = std::max(0.0, gd::dot(n, gd::normalized(gd::add(ld, to_eye))));
  c.glint[i] = static_cast<float>(pow32(hk) * L.key * (key > 0 ? 1 : 0) +
                                  pow32(hl) * L.lamp_power / (d2 + 0.5));
  c.albedo[i * 3] = static_cast<float>(m.r);
  c.albedo[i * 3 + 1] = static_cast<float>(m.g);
  c.albedo[i * 3 + 2] = static_cast<float>(m.b);
  c.gloss[i] = static_cast<float>(m.gloss);
}

double fract(double v) { return v - std::floor(v); }

}  // namespace

Canvas make_canvas(int w, int h, const Cam& cam, const Light& light, int ss) {
  Canvas c;
  c.w = w;
  c.h = h;
  c.ss = ss;
  c.cam = cam;
  c.light = light;
  c.fwd = gd::normalized(gd::sub(cam.target, cam.eye));
  c.right = gd::normalized(gd::cross({0, 1, 0}, c.fwd));
  c.up = gd::cross(c.fwd, c.right);
  if (cam.fov_deg > 0) {
    c.focal = h * ss * 0.5 / core::libm::tan(cam.fov_deg * core::kPi / 360);
  }
  const size_t n = static_cast<size_t>(w) * h * ss * ss;
  c.depth.assign(n, kInf);
  c.albedo.assign(n * 3, 0);
  c.gloss.assign(n, 0);
  c.diffuse.assign(n * 3, 0);
  c.glint.assign(n, 0);
  c.over.assign(n * 4, 0);
  return c;
}

void draw_mesh(Canvas& c, const gd::TriMesh& mesh, Paint paint) {
  for (uint32_t t = 0; t < mesh.triangles(); ++t) {
    const gd::Vec3 n = gd::normal_of(mesh, t);
    const gd::Vec3& a = gd::corner(mesh, t, 0);
    const gd::Vec3 view = c.cam.fov_deg <= 0 ? gd::mul(c.fwd, -1) : gd::sub(c.cam.eye, a);
    if (gd::dot(n, view) <= 0) continue;
    const uint16_t tag = mesh.tag[t];
    raster(c, {a, {}}, {gd::corner(mesh, t, 1), {}}, {gd::corner(mesh, t, 2), {}},
           [&](size_t i, double z, const V& v) {
             if (z < c.depth[i]) shade_pixel(c, i, z, v.world, n, paint(tag, v.world, n));
           });
  }
}

void draw_prop(Canvas& c, const gd::PropMesh& shape, const gd::PropFrame& f, Material m) {
  for (size_t k = 0; k + 2 < shape.pos.size(); k += 3) {
    const gd::Vec3 a = gd::to_world(f, shape.pos[k]);
    const gd::Vec3 b = gd::to_world(f, shape.pos[k + 1]);
    const gd::Vec3 d = gd::to_world(f, shape.pos[k + 2]);
    const gd::Vec3 n = gd::normalized(gd::cross(gd::sub(b, a), gd::sub(d, a)));
    const gd::Vec3 view = c.cam.fov_deg <= 0 ? gd::mul(c.fwd, -1) : gd::sub(c.cam.eye, a);
    if (gd::dot(n, view) <= 0) continue;
    // A touch of variety across a prop's faces, so a lump reads as stone and not as plastic.
    const double var = 0.85 + 0.3 * core::hash2(static_cast<double>(k), 3, 11);
    const Material mm{m.r * var, m.g * var, m.b * var, m.gloss};
    raster(c, {a, {}}, {b, {}}, {d, {}}, [&](size_t i, double z, const V& v) {
      if (z < c.depth[i]) shade_pixel(c, i, z, v.world, n, mm);
    });
  }
}

void draw_decal(Canvas& c, std::span<const gd::DecalVertex> verts, gd::DecalKind kind,
                uint32_t variant, double age_s) {
  const gd::DecalLook look = gd::look_at(kind, age_s);
  for (size_t k = 0; k + 2 < verts.size(); k += 3) {
    V tri[3];
    for (int j = 0; j < 3; ++j) {
      const gd::DecalVertex& dv = verts[k + j];
      tri[j] = {dv.pos, {dv.u, dv.v, dv.fade, 0}};
    }
    raster(c, tri[0], tri[1], tri[2], [&](size_t i, double z, const V& v) {
      if (c.depth[i] == kInf || std::fabs(z - c.depth[i]) > 0.004 + 0.002 * z) return;
      const gd::MaskSample m = gd::mask_at(kind, variant, v.a[0], v.a[1]);
      const double a = m.cover * look.opacity * v.a[2];
      if (a <= 0) return;
      const double tone = core::clamp(1 + 0.7 * m.shade, 0, 1.6);
      const double rgb[3] = {look.r * tone, look.g * tone, look.b * tone};
      for (int ch = 0; ch < 3; ++ch) {
        c.albedo[i * 3 + ch] = static_cast<float>(c.albedo[i * 3 + ch] * (1 - a) + rgb[ch] * a);
      }
      c.gloss[i] = static_cast<float>(c.gloss[i] * (1 - a) + look.gloss * a);
    });
  }
}

void draw_line3(Canvas& c, const gd::Vec3& a, const gd::Vec3& b, Rgba colour, double px) {
  gd::Vec3 va = to_view(c, a), vb = to_view(c, b);
  const bool ortho = c.cam.fov_deg <= 0;
  if (!ortho) {
    if (va.z < kNear && vb.z < kNear) return;
    if (va.z < kNear) va = gd::mad(va, gd::sub(vb, va), (kNear - va.z) / (vb.z - va.z));
    if (vb.z < kNear) vb = gd::mad(vb, gd::sub(va, vb), (kNear - vb.z) / (va.z - vb.z));
  }
  const Screen sa = to_screen(c, va), sb = to_screen(c, vb);
  const double len = core::hypot(sb.x - sa.x, sb.y - sa.y);
  const int steps = std::max(1, static_cast<int>(len * 2));
  const int W = c.w * c.ss, H = c.h * c.ss;
  const int r = std::max(0, static_cast<int>(px * c.ss * 0.5));
  for (int s = 0; s <= steps; ++s) {
    const double t = static_cast<double>(s) / steps;
    const double x = sa.x + (sb.x - sa.x) * t, y = sa.y + (sb.y - sa.y) * t;
    const double z = ortho ? sa.z + (sb.z - sa.z) * t : 1 / (1 / sa.z + (1 / sb.z - 1 / sa.z) * t);
    for (int dy = -r; dy <= r; ++dy) {
      for (int dx = -r; dx <= r; ++dx) {
        const int xi = static_cast<int>(x) + dx, yi = static_cast<int>(y) + dy;
        if (xi < 0 || yi < 0 || xi >= W || yi >= H) continue;
        const size_t i = static_cast<size_t>(yi) * W + xi;
        if (z > c.depth[i] * 1.004 + 0.01) continue;
        c.over[i * 4] = colour.r / 255.0f;
        c.over[i * 4 + 1] = colour.g / 255.0f;
        c.over[i * 4 + 2] = colour.b / 255.0f;
        c.over[i * 4 + 3] = colour.a / 255.0f;
      }
    }
  }
}

namespace {

// How much of the sky's and the lamps' light a sample's neighbours on the screen hold off it:
// eight depth samples a world radius round it, each nearer by a step and not far nearer.
double occlusion_at(const Canvas& c, int x, int y) {
  const int W = c.w * c.ss, H = c.h * c.ss;
  const double z = c.depth[static_cast<size_t>(y) * W + x];
  constexpr double kRadius = 0.3, kReach = 0.6;
  const double px = c.cam.fov_deg <= 0 ? kRadius * H * 0.5 / c.cam.half_m : kRadius * c.focal / z;
  const double r = core::clamp(px, 2, 48);
  static constexpr double kDirs[8][2] = {{1, 0},  {0.707, 0.707},   {0, 1},  {-0.707, 0.707},
                                         {-1, 0}, {-0.707, -0.707}, {0, -1}, {0.707, -0.707}};
  double occ = 0;
  for (int k = 0; k < 8; ++k) {
    for (double f : {0.5, 1.0}) {
      // Off the picture, the nearest edge sample stands in (so the border does not glow).
      const int sx = std::clamp(x + static_cast<int>(kDirs[k][0] * r * f), 0, W - 1);
      const int sy = std::clamp(y + static_cast<int>(kDirs[k][1] * r * f), 0, H - 1);
      const double d = z - c.depth[static_cast<size_t>(sy) * W + sx];
      if (d > 0.015 && d < kReach) occ += 1 - d / kReach;
    }
  }
  return occ / 16;
}

}  // namespace

Picture finish(const Canvas& c, Rgba sky) {
  Picture p = create_picture(c.w, c.h, sky);
  const int W = c.w * c.ss;
  const double sky_lin[3] = {sky.r / 255.0, sky.g / 255.0, sky.b / 255.0};
  for (int y = 0; y < c.h; ++y) {
    for (int x = 0; x < c.w; ++x) {
      double sum[3] = {0, 0, 0};
      for (int sy = 0; sy < c.ss; ++sy) {
        for (int sx = 0; sx < c.ss; ++sx) {
          const size_t i =
              static_cast<size_t>(y * c.ss + sy) * W + static_cast<size_t>(x * c.ss + sx);
          double rgb[3];
          if (c.depth[i] == kInf) {
            for (int k = 0; k < 3; ++k) rgb[k] = sky_lin[k];
          } else {
            const double ao = 1 - 1.4 * occlusion_at(c, x * c.ss + sx, y * c.ss + sy);
            for (int k = 0; k < 3; ++k) {
              const double lit = c.albedo[i * 3 + k] * c.diffuse[i * 3 + k] * std::max(0.2, ao) +
                                 c.gloss[i] * c.glint[i] * 0.6;
              // Tone: an exponential shoulder, then the display's gamma of two.
              rgb[k] = std::sqrt(1 - core::libm::exp(-lit * 1.6));
            }
          }
          const double a = c.over[i * 4 + 3];
          for (int k = 0; k < 3; ++k) sum[k] += rgb[k] * (1 - a) + c.over[i * 4 + k] * a;
        }
      }
      const double inv = 1.0 / (c.ss * c.ss);
      Rgba out{0, 0, 0, 255};
      uint8_t* ch[3] = {&out.r, &out.g, &out.b};
      for (int k = 0; k < 3; ++k) {
        *ch[k] = static_cast<uint8_t>(std::lround(core::clamp(sum[k] * inv, 0, 1) * 255));
      }
      set_pixel(p, x, y, out);
    }
  }
  return p;
}

gd::TriMesh mesh_of(const std::vector<gd::Solid>& solids) {
  gd::TriMesh m;
  for (const gd::Solid& s : solids) gd::triangulate(s, gd::polygons_of(s), m);
  return m;
}

std::vector<gd::Solid> bevelled(const std::vector<gd::Solid>& solids, const gd::BevelSpec& spec,
                                std::vector<gd::BevelResult>* out) {
  const auto r = gd::bevel_all(solids, spec);
  std::vector<gd::Solid> s;
  s.reserve(r.size());
  for (const gd::BevelResult& b : r) s.push_back(b.solid);
  if (out) *out = r;
  return s;
}

void label(Picture& p, int x, int y, const std::string& text) {
  const int scale = p.width >= 600 ? 2 : 1;
  fill_rect(p, x, y, text_width(text, scale) + 8, kGlyphH * scale + 6, {0, 0, 0, 150});
  draw_text(p, x + 4, y + 3, text, {255, 255, 255}, scale);
}

Material hall_paint(uint16_t tag, const gd::Vec3& p, const gd::Vec3& n) {
  const double grain = core::value_noise(p.x * 7 + p.y * 3, p.z * 7 - p.y * 5, 1, 5);
  const double fine = core::value_noise(p.x * 31 + p.y * 17, p.z * 31 - p.y * 13, 1, 9);
  if (tag == 2) {  // timber: grain along x
    const double g = 0.8 + 0.25 * core::value_noise(p.y * 60, p.z * 60, 1, 3) + 0.1 * fine;
    return {0.30 * g, 0.18 * g, 0.10 * g, 0.12};
  }
  if (tag == 3) {  // the basin's wet floor
    const double g = 0.85 + 0.2 * grain;
    return {0.10 * g, 0.12 * g, 0.115 * g, 0.7};
  }
  const bool level = n.y > 0.6;
  double u, v, joint_w = 0.025;
  if (tag == 0 && level) {  // flagstones, 0.8 m, every other row offset
    const double row = std::floor(p.z / 0.8);
    u = p.x / 0.8 + 0.5 * row;
    v = p.z / 0.8;
  } else if (!level) {  // ashlar: courses of 0.35 m, blocks of 0.7 m
    const double along = std::fabs(n.z) > std::fabs(n.x) ? p.x : p.z;
    const double course = std::floor(p.y / 0.35);
    u = along / 0.7 + 0.5 * course;
    v = p.y / 0.35;
    joint_w = 0.04;
  } else {
    u = v = 0.5;  // a wall's top: stone, unjointed
    joint_w = 0;
  }
  const double block = core::hash2(std::floor(u), std::floor(v), tag == 0 ? 21 : 23);
  const bool joint = fract(u) < joint_w || fract(v) < joint_w * (level ? 1 : 2);
  const double base[3] = {tag == 0 ? 0.40 : 0.46, tag == 0 ? 0.38 : 0.43, tag == 0 ? 0.34 : 0.38};
  if (joint) {
    const double g = 0.55 + 0.1 * fine;
    return {base[0] * g, base[1] * g, base[2] * g, 0.05};
  }
  const double g = (0.78 + 0.35 * block) * (0.88 + 0.16 * grain + 0.08 * fine);
  const double warm = 0.04 * (block - 0.5);
  return {base[0] * g + warm, base[1] * g, base[2] * g - warm, 0.18};
}

}  // namespace ge::lab::detail_view
