// Props' shapes: the icosahedron, the shard's outline and the splinter's taper, the moss dome,
// and the frame a prop's up and yaw make.
//
// ge:summary Props' shape builders and the prop frame
// ge:keywords detail props icosahedron shard splinter moss dome frame
// ge:skill detail
// ge:test tests/detail_props_test.cpp
#include "detail/prop_shapes.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/prng.hpp"

namespace ge::detail {
namespace {

void tri(PropMesh& m, const Vec3& a, const Vec3& b, const Vec3& c) {
  const Vec3 n = normalized(cross(sub(b, a), sub(c, a)));
  m.pos.insert(m.pos.end(), {a, b, c});
  m.nrm.insert(m.nrm.end(), {n, n, n});
}

// The mesh moved so its lowest point is at y = 0.
void stand(PropMesh& m) {
  double lo = 1e300;
  for (const Vec3& v : m.pos) lo = std::min(lo, v.y);
  for (Vec3& v : m.pos) v.y -= lo;
}

double hash3(const Vec3& v, double seed) {
  return core::hash2(std::floor(v.x * 1000 + v.y * 37), std::floor(v.z * 1000 - v.y * 53), seed);
}

// A lump: an icosahedron cut once, every corner pushed to `base` + `rough` times its own hash,
// squashed and stretched within the ranges, standing on a cut flat face.
PropMesh lump(uint32_t var, uint32_t salt, double base, double rough, double squash_lo,
              double squash_hi, double stretch_hi);

PropMesh rubble(uint32_t var) { return lump(var, 17, 0.72, 0.4, 0.45, 0.75, 1.5); }

PropMesh lump(uint32_t var, uint32_t salt, double base, double rough, double squash_lo,
              double squash_hi, double stretch_hi) {
  const double g = (1 + std::sqrt(5.0)) / 2;
  std::vector<Vec3> v = {{-1, g, 0},  {1, g, 0},  {-1, -g, 0}, {1, -g, 0}, {0, -1, g},  {0, 1, g},
                         {0, -1, -g}, {0, 1, -g}, {g, 0, -1},  {g, 0, 1},  {-g, 0, -1}, {-g, 0, 1}};
  for (Vec3& p : v) p = normalized(p);
  const int f[20][3] = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                        {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                        {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                        {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
  core::Rng rng = core::make_rng(var * 2654435761u + salt);
  const double squash = core::range(rng, squash_lo, squash_hi);
  const double stretch = core::range(rng, 1, stretch_hi);
  const double seed = 41 + var + (salt - 17) * 101.0;
  // One cut: each face into four, the new corners pushed to the sphere, then every corner pushed
  // in or out by its own hash (shared corners get the same push, so the lump stays closed).
  auto push = [&](Vec3 p) {
    p = normalized(p);
    const double r = base + rough * hash3(p, seed);
    return Vec3{p.x * r * 0.5 * stretch, p.y * r * 0.5 * squash, p.z * r * 0.5};
  };
  PropMesh m;
  for (const auto& t : f) {
    const Vec3 a = v[t[0]], b = v[t[1]], c = v[t[2]];
    const Vec3 ab = mul(add(a, b), 0.5), bc = mul(add(b, c), 0.5), ca = mul(add(c, a), 0.5);
    // Winding: the icosahedron's faces wind with their front outward in the engine's cross.
    tri(m, push(a), push(ab), push(ca));
    tri(m, push(ab), push(b), push(bc));
    tri(m, push(ca), push(bc), push(c));
    tri(m, push(ab), push(bc), push(ca));
  }
  // The flat face it settled on: its lowest tenth cut level.
  stand(m);
  for (Vec3& p : m.pos) p.y = std::max(p.y, 0.05 * squash);
  for (size_t i = 0; i < m.pos.size(); i += 3) {
    const Vec3 n = normalized(cross(sub(m.pos[i + 1], m.pos[i]), sub(m.pos[i + 2], m.pos[i])));
    m.nrm[i] = m.nrm[i + 1] = m.nrm[i + 2] = n;
  }
  stand(m);
  return m;
}

// A prism over the outline `xz` (counter-clockwise from above) from y = 0 to `top(x, z)`.
template <class Top>
void slab(PropMesh& m, const std::vector<Vec3>& xz, Top top) {
  const size_t n = xz.size();
  auto up = [&](const Vec3& p) { return Vec3{p.x, top(p.x, p.z), p.z}; };
  for (size_t i = 1; i + 1 < n; ++i) {
    tri(m, up(xz[0]), up(xz[i + 1]), up(xz[i]));
    tri(m, xz[0], xz[i], xz[i + 1]);
  }
  for (size_t i = 0; i < n; ++i) {
    const Vec3& a = xz[i];
    const Vec3& b = xz[(i + 1) % n];
    tri(m, a, up(a), up(b));
    tri(m, a, up(b), b);
  }
}

PropMesh debris(uint32_t var) {
  core::Rng rng = core::make_rng(var * 2246822519u + 29);
  PropMesh m;
  std::vector<Vec3> xz;
  if (var % 2 == 0) {
    // A shard: five to seven corners round a ragged circle, a slab's thickness, tilted top.
    const int corners = core::int_in(rng, 5, 7);
    double a = core::range(rng, 0, core::kTau);
    for (int i = 0; i < corners; ++i) {
      const double r = core::range(rng, 0.28, 0.5);
      xz.push_back({core::libm::cos(a) * r, 0, core::libm::sin(a) * r});
      a += core::kTau / corners * core::range(rng, 0.7, 1.3);
    }
    const double t = core::range(rng, 0.06, 0.12), tilt = core::range(rng, -0.08, 0.08);
    slab(m, xz, [&](double x, double) { return t + tilt * x; });
  } else {
    // A splinter: a long thin plank, its ends broken to points.
    const double w = core::range(rng, 0.07, 0.12), l = 0.5;
    const double tip0 = core::range(rng, -w, w), tip1 = core::range(rng, -w, w);
    xz = {{-l, 0, tip0}, {-l * 0.8, 0, -w}, {l * 0.75, 0, -w},
          {l, 0, tip1},  {l * 0.85, 0, w},  {-l * 0.7, 0, w}};
    const double t = core::range(rng, 0.035, 0.06);
    slab(m, xz, [&](double, double) { return t; });
  }
  return m;
}

// A lumpy dome: moss's cushion, or a clod of earth when low.
PropMesh dome(uint32_t var, uint32_t salt, double h_lo, double h_hi) {
  constexpr int kRings = 5, kSegs = 12;
  const double seed = 61 + var + (salt - 5) * 89.0;
  core::Rng rng = core::make_rng(var * 3266489917u + salt);
  const double height = core::range(rng, h_lo, h_hi);
  auto at = [&](int ring, int seg) {
    const double r = 0.5 * ring / kRings;
    const double a = seg * core::kTau / kSegs;
    const double wob = 1 + 0.25 * (core::hash2(ring == 0 ? 0 : seg, ring, seed) - 0.5);
    const double rr = r * (ring == kRings ? 1 : wob);
    const double k = r / 0.5;
    const double h =
        ring == kRings ? 0 : height * (1 - k * k) * (0.75 + 0.5 * core::hash2(seg, ring + 9, seed));
    return Vec3{core::libm::cos(a) * rr, h, core::libm::sin(a) * rr};
  };
  PropMesh m;
  for (int ring = 0; ring < kRings; ++ring) {
    for (int s = 0; s < kSegs; ++s) {
      const Vec3 a = at(ring, s), b = at(ring, s + 1), c = at(ring + 1, s), d = at(ring + 1, s + 1);
      if (ring > 0) tri(m, a, b, d);
      tri(m, a, d, c);
    }
  }
  return m;
}

PropMesh moss(uint32_t var) { return dome(var, 5, 0.14, 0.26); }

// A tapered tube of `sides` from `a` (radius `ra`) to `b` (radius `rb`), capped at both ends.
void limb(PropMesh& m, const Vec3& a, const Vec3& b, double ra, double rb, int sides) {
  const Vec3 axis = normalized(sub(b, a));
  const Vec3 u = any_perpendicular(axis), v = cross(axis, u);
  auto ring = [&](const Vec3& c, double r, int k) {
    const double t = k * core::kTau / sides;
    return add(c, add(mul(u, core::libm::cos(t) * r), mul(v, core::libm::sin(t) * r)));
  };
  for (int k = 0; k < sides; ++k) {
    const Vec3 a0 = ring(a, ra, k), a1 = ring(a, ra, k + 1);
    const Vec3 b0 = ring(b, rb, k), b1 = ring(b, rb, k + 1);
    tri(m, a0, a1, b1);
    tri(m, a0, b1, b0);
    tri(m, a, a1, a0);
    tri(m, b, b0, b1);
  }
}

// A tuft of blades fanned out of a clump, each a bent strip seen from both sides.
PropMesh grass(uint32_t var) {
  core::Rng rng = core::make_rng(var * 2654435761u + 71);
  PropMesh m;
  const int blades = core::int_in(rng, 9, 14);
  for (int i = 0; i < blades; ++i) {
    const double a = core::range(rng, 0, core::kTau);
    const double r = core::range(rng, 0, 0.16);
    const double h = core::range(rng, 0.55, 1.0);
    const double lean = core::range(rng, 0.1, 0.45);
    const double w = core::range(rng, 0.03, 0.055);
    const Vec3 out{core::libm::cos(a), 0, core::libm::sin(a)};
    const Vec3 side{-out.z, 0, out.x};
    const Vec3 base = mul(out, r);
    const Vec3 mid = add(base, Vec3{out.x * lean * 0.35 * h, h * 0.55, out.z * lean * 0.35 * h});
    const Vec3 tip = add(base, Vec3{out.x * lean * h, h, out.z * lean * h});
    const Vec3 b0 = mad(base, side, -w), b1 = mad(base, side, w);
    const Vec3 m0 = mad(mid, side, -w * 0.7), m1 = mad(mid, side, w * 0.7);
    for (int face = 0; face < 2; ++face) {
      if (face == 0) {
        tri(m, b0, b1, m1);
        tri(m, b0, m1, m0);
        tri(m, m0, m1, tip);
      } else {
        tri(m, b0, m1, b1);
        tri(m, b0, m0, m1);
        tri(m, m0, tip, m1);
      }
    }
  }
  return m;
}

// A twig (a thin stick with a fork) or a branch (a heavier limb, bent, with two forks), lying on
// the ground along x.
PropMesh stick(uint32_t var, bool branch) {
  core::Rng rng = core::make_rng(var * 2246822519u + (branch ? 83 : 79));
  PropMesh m;
  const double r = branch ? 0.035 : 0.02;
  const double bend = core::range(rng, -0.12, 0.12);
  const Vec3 a{-0.5, r, 0}, mid{0, r * 0.9, bend}, b{0.5, r * 0.6, core::range(rng, -0.05, 0.05)};
  limb(m, a, mid, r, r * 0.8, 5);
  limb(m, mid, b, r * 0.8, r * 0.35, 5);
  const int forks = branch ? 2 : 1;
  for (int f = 0; f < forks; ++f) {
    const double t = core::range(rng, 0.2, 0.7);
    const Vec3 at = add(mul(a, 1 - t), mul(b, t));
    const double side = core::chance(rng, 0.5) ? 1 : -1;
    const double len = core::range(rng, 0.18, 0.35);
    const Vec3 end{at.x + len * 0.6, at.y * 0.7, at.z + side * len * 0.8};
    limb(m, at, end, r * 0.55, r * 0.2, 4);
  }
  stand(m);
  return m;
}

// A spent cartridge case lying on its side: a tube with its rim, along x.
PropMesh shell(uint32_t var) {
  core::Rng rng = core::make_rng(var * 3266489917u + 97);
  PropMesh m;
  const double r = core::range(rng, 0.17, 0.24);
  limb(m, {-0.5, r * 1.12, 0}, {-0.42, r * 1.12, 0}, r * 1.12, r * 1.12, 10);
  limb(m, {-0.42, r * 1.12, 0}, {0.5, r * 1.12, 0}, r, r * 0.97, 10);
  stand(m);
  return m;
}

// A bone: a shaft with a knuckle at each end, lying along x.
PropMesh bone(uint32_t var) {
  core::Rng rng = core::make_rng(var * 2654435761u + 101);
  PropMesh m;
  const double r = core::range(rng, 0.045, 0.07), k = r * core::range(rng, 1.8, 2.3);
  const double y = k;
  limb(m, {-0.5, y, 0}, {-0.4, y, 0}, k * 0.7, k, 6);
  limb(m, {-0.4, y, 0}, {-0.32, y, 0}, k, r, 6);
  limb(m, {-0.32, y, 0}, {0.32, y, 0}, r, r, 6);
  limb(m, {0.32, y, 0}, {0.4, y, 0}, r, k, 6);
  limb(m, {0.4, y, 0}, {0.5, y, 0}, k, k * 0.7, 6);
  stand(m);
  return m;
}

}  // namespace

PropMesh prop_mesh(PropKind kind, uint32_t variant) {
  switch (kind) {
    case PropKind::rubble:
      return rubble(variant);
    case PropKind::debris:
      return debris(variant);
    case PropKind::moss:
      return moss(variant);
    case PropKind::pebble:
      return lump(variant, 23, 0.85, 0.14, 0.4, 0.6, 1.5);
    case PropKind::rock:
      return lump(variant, 31, 0.62, 0.6, 0.55, 0.9, 1.3);
    case PropKind::dirt:
      return dome(variant, 13, 0.08, 0.15);
    case PropKind::grass:
      return grass(variant);
    case PropKind::twig:
      return stick(variant, false);
    case PropKind::branch:
      return stick(variant, true);
    case PropKind::shell:
      return shell(variant);
    case PropKind::bone:
      return bone(variant);
  }
  return {};
}

PropFrame prop_frame(const Prop& p) {
  const Vec3 y = normalized(p.up);
  const Vec3 x0 = any_perpendicular(y);
  const Vec3 z0 = cross(x0, y);
  const double c = core::libm::cos(p.yaw_rad), s = core::libm::sin(p.yaw_rad);
  const Vec3 x = add(mul(x0, c), mul(z0, s));
  const Vec3 z = cross(x, y);
  return {mul(x, p.size_m), mul(y, p.size_m), mul(z, p.size_m), p.at};
}

}  // namespace ge::detail
