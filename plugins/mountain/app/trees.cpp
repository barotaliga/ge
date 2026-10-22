// The trees' forms built face by face, their parts by facing, the kinds' colours, the instance.
//
// ge:summary The trees' procedural meshes: skirts, pads, arms and limbs, parts, kinds' colours
// ge:keywords trees tree-mesh skirt pad limb whorl crown-part snow-by-facing kind-colours
// ge:skill mountain render
#include "mountain/app/trees.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ge::mountain::app {

namespace {

using mapgen::TreeKind;
using V = std::array<float, 3>;

constexpr float kPi = 3.14159265f;

V add(V a, V b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V sub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V mul(V a, float k) { return {a[0] * k, a[1] * k, a[2] * k}; }
float dot(V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V cross(V a, V b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
V unit(V a) {
  const float l = std::sqrt(dot(a, a));
  return l > 0 ? mul(a, 1 / l) : V{0, 1, 0};
}

// A jitter in [-1, 1] for the integers (a, b, c): the same on every machine.
float jit(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca77u ^ (c + 0x165667b1u) * 0xc2b2ae3du;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  h *= 0x297a2d39u;
  h ^= h >> 15;
  return static_cast<float>(h & 0xffffu) / 32767.5f - 1.0f;
}

// What a face is made of; its part follows from it and the way the face looks.
enum class Stuff : uint8_t { crown, wood, twig };

// The mesh being built and the variant's snow: a face turned up at least `snow` (the normal's y)
// holds snow, at least `frost` a dusting. The mesh is unit-sized and an instance stretches it
// `aspect` times as tall as wide (a form's usual height over its crown), so a face is judged by
// the normal it will have standing: its y scaled by 1 / aspect against the rest.
struct Build {
  render::MeshData& m;
  float snow;
  float frost;
  float aspect;
};

uint8_t part_of(Stuff s, float ny, const Build& b) {
  switch (s) {
    case Stuff::crown:
      if (ny >= b.snow) return kSnow;
      if (ny >= b.frost) return kFrost;
      return ny <= -0.25f ? kNeedleShade : kNeedle;
    case Stuff::wood:
      return ny >= b.snow ? kSnow : kBark;
    case Stuff::twig:
      return ny >= b.snow ? kFrost : kTwig;
  }
  return kNeedle;
}

// The axis a face is turned away from: the segment [p0, p1] (a point when they meet).
struct Axis {
  V p0, p1;
};

V inside_of(const Axis& ax, V c) {
  const V d = sub(ax.p1, ax.p0);
  const float dd = dot(d, d);
  if (dd <= 0) return ax.p0;
  const float t = std::clamp(dot(sub(c, ax.p0), d) / dd, 0.0f, 1.0f);
  return add(ax.p0, mul(d, t));
}

// One flat face, wound to face away from the axis.
void face(Build& b, V p, V q, V r, const Axis& ax, Stuff s) {
  V n = cross(sub(q, p), sub(r, p));
  if (dot(n, n) <= 0) return;
  const V c = mul(add(add(p, q), r), 1.0f / 3);
  if (dot(n, sub(c, inside_of(ax, c))) < 0) {
    std::swap(q, r);
    n = mul(n, -1);
  }
  const V standing = unit({n[0], n[1] / b.aspect, n[2]});
  render::add_triangle(b.m, p.data(), q.data(), r.data(), part_of(s, standing[1], b));
}

// A ring of `count` points round the vertical through `centre` at its height, `radius` out,
// starting at `phase` (radians), each point's radius jittered by up to `rough` of it.
std::vector<V> ring(V centre, float radius, int count, float phase, float rough, uint32_t seed) {
  std::vector<V> out(static_cast<size_t>(count));
  for (int k = 0; k < count; ++k) {
    const float a = phase + 2 * kPi * static_cast<float>(k) / static_cast<float>(count);
    const float r = radius * (1 + rough * jit(seed, static_cast<uint32_t>(k), 3));
    out[static_cast<size_t>(k)] = {centre[0] + r * std::sin(a), centre[1],
                                   centre[2] + r * std::cos(a)};
  }
  return out;
}

// The faces between two rings of the same count.
void band(Build& b, const std::vector<V>& lo, const std::vector<V>& hi, const Axis& ax, Stuff s) {
  const size_t n = lo.size();
  for (size_t k = 0; k < n; ++k) {
    const size_t j = (k + 1) % n;
    face(b, lo[k], lo[j], hi[j], ax, s);
    face(b, lo[k], hi[j], hi[k], ax, s);
  }
}

// The faces from a ring to one point.
void fan(Build& b, const std::vector<V>& rim, V apex, const Axis& ax, Stuff s) {
  for (size_t k = 0; k < rim.size(); ++k) face(b, rim[k], rim[(k + 1) % rim.size()], apex, ax, s);
}

// A tapering limb from p0 (radius r0) to p1 (r1), `sides` faces round, closed at its tip.
void limb(Build& b, V p0, V p1, float r0, float r1, int sides, Stuff s, float phase) {
  const V d = unit(sub(p1, p0));
  const V ref = std::fabs(d[1]) < 0.9f ? V{0, 1, 0} : V{1, 0, 0};
  const V u = unit(cross(d, ref)), w = cross(d, u);
  std::vector<V> lo(static_cast<size_t>(sides)), hi(lo.size());
  for (int k = 0; k < sides; ++k) {
    const float a = phase + 2 * kPi * static_cast<float>(k) / static_cast<float>(sides);
    // Round in the standing tree, not the unit mesh: the height's stretch undone.
    V o = add(mul(u, std::cos(a)), mul(w, std::sin(a)));
    o[1] /= b.aspect;
    lo[static_cast<size_t>(k)] = add(p0, mul(o, r0));
    hi[static_cast<size_t>(k)] = add(p1, mul(o, r1));
  }
  const Axis ax{p0, p1};
  band(b, lo, hi, ax, s);
  if (r1 > 0) fan(b, hi, add(p1, mul(d, r1)), Axis{p0, p0}, s);
}

// A squashed pad of needles round `c`, radii (rx, ry, rx).
void pad(Build& b, V c, float rx, float ry, int rings, int segments, float phase, uint32_t seed) {
  std::vector<std::vector<V>> rows;
  for (int i = 1; i < rings; ++i) {
    const float t = kPi * static_cast<float>(i) / static_cast<float>(rings);
    rows.push_back(ring({c[0], c[1] + ry * std::cos(t), c[2]}, rx * std::sin(t), segments, phase,
                        0.12f, seed + static_cast<uint32_t>(i)));
  }
  const Axis ax{c, c};
  fan(b, rows.front(), {c[0], c[1] + ry, c[2]}, ax, Stuff::crown);
  for (size_t i = 0; i + 1 < rows.size(); ++i) band(b, rows[i + 1], rows[i], ax, Stuff::crown);
  fan(b, rows.back(), {c[0], c[1] - ry, c[2]}, ax, Stuff::crown);
}

// A point `r` out from the trunk at height y, round at angle a.
V out_at(float a, float r, float y) { return {r * std::sin(a), y, r * std::cos(a)}; }

// THE CONIFER: a trunk into a stack of skirts, each a cone up to a shoulder, a lip drooping
// under it and a shaded underside back to the trunk.
void conifer(Build& b, int v, int lod) {
  const uint32_t seed = 100 + static_cast<uint32_t>(v);
  const int segs[] = {9, 7, 5};
  const int tiers = lod == 0 ? 5 + (v & 1) : lod == 1 ? 3 : 2;
  const int n = segs[lod];
  const float base = 0.10f + 0.03f * static_cast<float>(v % 2);
  const float droop = 0.05f + 0.02f * static_cast<float>(v % 3);
  limb(b, {0, -0.05f, 0}, {0, base + 0.12f, 0}, 0.07f, 0.05f, 5 - lod, Stuff::wood, 0);
  for (int i = 0; i < tiers; ++i) {
    const float f0 = static_cast<float>(i) / static_cast<float>(tiers);
    const float f1 = static_cast<float>(i + 1) / static_cast<float>(tiers);
    const float yb = base + (1 - base) * f0 * 0.9f;
    const bool last = i + 1 == tiers;
    const float yt = last ? 1.0f : base + (1 - base) * std::min(1.0f, f1 * 0.9f + 0.2f);
    const float rb =
        std::pow(1 - f0, 0.85f) * (1 - 0.08f * (1 + jit(seed, static_cast<uint32_t>(i), 1)));
    const float h = yt - yb;
    const float ys = yb + 0.1f * h;   // the shoulder, over the lip
    const float yf = yb + 0.28f * h;  // the shelf's inner edge: the shelf is where snow lies
    const float phase = 0.7f * static_cast<float>(i) + static_cast<float>(v);
    const uint32_t s = seed * 31 + static_cast<uint32_t>(i);
    const auto lip = ring({0, yb - droop * (1 - 0.5f * f0), 0}, rb, n, phase, 0.1f, s);
    const auto shoulder = ring({0, ys, 0}, rb * 0.85f, n, phase, 0.1f, s);
    const auto shelf = ring({0, yf, 0}, rb * 0.45f, n, phase, 0.1f, s + 7);
    const Axis core{{0, yb - droop, 0}, {0, ys, 0}};
    // At far the shoulder goes: the lip runs straight up to the shelf.
    if (lod == 2) {
      band(b, lip, shelf, core, Stuff::crown);
    } else {
      band(b, lip, shoulder, core, Stuff::crown);
      band(b, shoulder, shelf, core, Stuff::crown);
    }
    if (last) {
      fan(b, shelf, {0, yt, 0}, core, Stuff::crown);
    } else {
      band(b, shelf, ring({0, yt, 0}, rb * 0.1f, n, phase, 0.0f, s), core, Stuff::crown);
    }
    // The underside, seen from below: from the lip back up to the trunk.
    fan(b, lip, {0, yb + 0.15f * h, 0}, Axis{{0, yt, 0}, {0, yt, 0}}, Stuff::crown);
  }
}

// THE PINE: a bare trunk, a branch to each pad, the pads flat and capped.
void pine(Build& b, int v, int lod) {
  const uint32_t seed = 200 + static_cast<uint32_t>(v);
  const float top = 0.78f + 0.04f * static_cast<float>(v % 3);
  limb(b, {0, -0.05f, 0}, {0, top, 0}, 0.06f, 0.03f, 5 - lod, Stuff::wood, 0);
  if (lod == 2) {
    pad(b, {0, 0.84f, 0}, 0.95f, 0.09f, 3, 5, static_cast<float>(v), seed);
    return;
  }
  const int pads = lod == 0 ? 4 + (v & 1) : 3;
  const int rings = lod == 0 ? 4 : 3, segments = lod == 0 ? 7 : 5;
  for (int k = 0; k < pads; ++k) {
    const bool crown = k + 1 == pads;
    const float a = 2.4f * static_cast<float>(k) + static_cast<float>(v);
    const float t = static_cast<float>(k) / static_cast<float>(pads - 1);
    const float d = crown ? 0.05f : 0.42f + 0.06f * jit(seed, static_cast<uint32_t>(k), 2);
    const float y =
        crown ? 0.95f : 0.56f + 0.24f * t + 0.03f * jit(seed, static_cast<uint32_t>(k), 4);
    // Flat in the standing tree: a pad's depth a third of its width, the height's stretch undone.
    const float rx = crown ? 0.5f : 0.52f, ry = 0.35f * rx / b.aspect;
    const V c = out_at(a, d, y);
    if (!crown) {
      limb(b, {0, y - 0.12f, 0}, add(c, {0, -0.04f, 0}), 0.025f, 0.015f, 3, Stuff::wood, 0);
    }
    pad(b, c, rx, ry, rings, segments, a, seed * 17 + static_cast<uint32_t>(k));
  }
}

// THE LARCH IN WINTER: a trunk to the tip and whorls of thin arms, shorter up the tree.
void larch(Build& b, int v, int lod) {
  const uint32_t seed = 300 + static_cast<uint32_t>(v);
  limb(b, {0, -0.05f, 0}, {0, 1.0f, 0}, 0.05f, 0.008f, 5 - lod, Stuff::wood, 0);
  // Far off, the arms are a haze of twigs round the trunk.
  if (lod == 2) {
    const auto rim = ring({0, 0.12f, 0}, 0.6f, 5, static_cast<float>(v), 0.1f, seed);
    fan(b, rim, {0, 1.0f, 0}, Axis{{0, 0.12f, 0}, {0, 1.0f, 0}}, Stuff::twig);
    fan(b, rim, {0, 0.2f, 0}, Axis{{0, 1.0f, 0}, {0, 1.0f, 0}}, Stuff::twig);
    return;
  }
  const int whorls = lod == 0 ? 7 : 4;
  const int arms = lod == 0 ? 5 : 3;
  for (int i = 0; i < whorls; ++i) {
    const float f = (static_cast<float>(i) + 0.5f) / static_cast<float>(whorls);
    const float y = 0.14f + 0.8f * f;
    const float reach =
        0.08f + 0.9f * (1 - f) * (1 - 0.1f * jit(seed, static_cast<uint32_t>(i), 1));
    for (int k = 0; k < arms; ++k) {
      const float a = 2 * kPi * static_cast<float>(k) / static_cast<float>(arms) +
                      0.9f * static_cast<float>(i) + static_cast<float>(v);
      const V tip = out_at(a, reach, y - 0.06f - 0.04f * (1 - f));
      limb(b, {0, y, 0}, tip, 0.03f * (1.2f - f), 0.006f, 3, Stuff::wood, a);
    }
  }
}

// THE BROADLEAF IN WINTER: a trunk forking into stems, limbs off the stems, twigs off the limbs;
// far off a haze of twigs round the stems.
void broadleaf(Build& b, int v, int lod) {
  const uint32_t seed = 400 + static_cast<uint32_t>(v);
  const float fork = 0.38f + 0.06f * static_cast<float>(v % 2);
  limb(b, {0, -0.05f, 0}, {0, fork, 0}, 0.07f, 0.055f, 5 - lod, Stuff::wood, 0);
  const int stems = 3 + (v & 1);
  for (int s = 0; s < stems; ++s) {
    const float a =
        2 * kPi * static_cast<float>(s) / static_cast<float>(stems) + static_cast<float>(v);
    const V top = out_at(a, 0.38f + 0.08f * jit(seed, static_cast<uint32_t>(s), 1),
                         0.86f + 0.06f * jit(seed, static_cast<uint32_t>(s), 2));
    if (lod == 2) continue;
    limb(b, {0, fork, 0}, top, 0.045f, 0.02f, lod == 0 ? 4 : 3, Stuff::wood, a);
    const int limbs = lod == 0 ? 2 : 1;
    for (int l = 0; l < limbs; ++l) {
      const float t = 0.4f + 0.3f * static_cast<float>(l);
      const V from = add(V{0, fork, 0}, mul(sub(top, V{0, fork, 0}), t));
      const float la = a + (l % 2 ? 0.9f : -0.9f);
      const V end = out_at(
          la, 0.82f + 0.1f * jit(seed, static_cast<uint32_t>(s), 10 + static_cast<uint32_t>(l)),
          from[1] + 0.12f);
      limb(b, from, end, 0.02f, 0.008f, 3, Stuff::wood, la);
      if (lod != 0) continue;
      for (int g = 0; g < 2; ++g) {
        const V at = add(from, mul(sub(end, from), 0.6f + 0.25f * static_cast<float>(g)));
        const float ga = la + (g ? 0.5f : -0.5f);
        const V tip =
            add(out_at(ga, 0.22f, 0.1f + 0.05f * static_cast<float>(g)), V{at[0], at[1], at[2]});
        const float r = std::sqrt(tip[0] * tip[0] + tip[2] * tip[2]);
        const V held = r > 1 ? V{tip[0] / r, tip[1], tip[2] / r} : tip;
        limb(b, at, held, 0.008f, 0.003f, 3, Stuff::twig, ga);
      }
    }
  }
  // Far off, the twigs are a haze round the stems (painted as twigs by `tree_mesh`).
  if (lod == 2) pad(b, {0, 0.72f, 0}, 0.7f, 0.24f, 3, 5, static_cast<float>(v), seed);
}

// THE SNAG: a dead trunk broken off near the top and a few stubs of arms.
void snag(Build& b, int v, int lod) {
  const uint32_t seed = 500 + static_cast<uint32_t>(v);
  const V top{0.03f * jit(seed, 0, 1), 0.94f + 0.06f * jit(seed, 0, 2) * 0.5f,
              0.03f * jit(seed, 0, 3)};
  limb(b, {0, -0.05f, 0}, top, 0.08f, 0.035f, 5 - lod, Stuff::wood, 0);
  const int stubs = lod == 0 ? 4 : lod == 1 ? 2 : 0;
  for (int k = 0; k < stubs; ++k) {
    const float y = 0.4f + 0.12f * static_cast<float>(k);
    const float a = 2.1f * static_cast<float>(k) + static_cast<float>(v);
    limb(b, {0, y, 0}, out_at(a, 0.3f, y + 0.05f), 0.02f, 0.008f, 3, Stuff::wood, a);
  }
}

// How much snow each form's variants hold: the normal's y a face needs for a cap, and for a
// dusting (the lower, the more of the tree is white).
constexpr float kSnowCos[kTreeVariants] = {0.55f, 0.45f, 0.66f, 0.5f};

// A form's usual height over its crown's radius (the generator's trees, R14), which the
// instance's scale stretches the unit mesh by.
constexpr float kAspect[kTreeForms] = {4.5f, 4.5f, 4.5f, 4.5f, 5.0f};

// THE KINDS' COLOURS, linear rgb: needles (or a bare crown's twigs), bark, and twigs.
struct KindTone {
  float needle[3];
  float bark[3];
  float twig[3];
};
// clang-format off
constexpr KindTone kTones[kTreeKinds] = {
    {{0.030f, 0.060f, 0.040f}, {0.070f, 0.045f, 0.030f}, {0.07f, 0.05f, 0.04f}},  // spruce
    {{0.028f, 0.062f, 0.050f}, {0.090f, 0.080f, 0.070f}, {0.07f, 0.05f, 0.04f}},  // fir
    {{0.045f, 0.075f, 0.040f}, {0.120f, 0.060f, 0.035f}, {0.07f, 0.05f, 0.04f}},  // pine
    {{0.100f, 0.075f, 0.055f}, {0.100f, 0.065f, 0.045f}, {0.10f, 0.075f, 0.055f}},  // larch
    {{0.022f, 0.040f, 0.030f}, {0.060f, 0.045f, 0.035f}, {0.06f, 0.045f, 0.035f}},  // black spruce
    {{0.035f, 0.070f, 0.045f}, {0.090f, 0.060f, 0.040f}, {0.07f, 0.05f, 0.04f}},  // stone pine
    {{0.045f, 0.080f, 0.060f}, {0.090f, 0.075f, 0.065f}, {0.07f, 0.05f, 0.04f}},  // white pine
    {{0.045f, 0.070f, 0.035f}, {0.110f, 0.070f, 0.045f}, {0.07f, 0.05f, 0.04f}},  // lodgepole
    {{0.030f, 0.065f, 0.045f}, {0.085f, 0.055f, 0.040f}, {0.07f, 0.05f, 0.04f}},  // hemlock
    {{0.040f, 0.065f, 0.050f}, {0.090f, 0.060f, 0.045f}, {0.07f, 0.05f, 0.04f}},  // juniper
    {{0.040f, 0.072f, 0.040f}, {0.090f, 0.060f, 0.040f}, {0.07f, 0.05f, 0.04f}},  // dwarf pine
    {{0.110f, 0.100f, 0.090f}, {0.110f, 0.100f, 0.090f}, {0.11f, 0.10f, 0.09f}},  // snag
    {{0.120f, 0.070f, 0.060f}, {0.550f, 0.550f, 0.520f}, {0.12f, 0.07f, 0.06f}},  // birch
    {{0.130f, 0.110f, 0.090f}, {0.320f, 0.360f, 0.300f}, {0.13f, 0.11f, 0.09f}},  // aspen
    {{0.100f, 0.080f, 0.070f}, {0.120f, 0.100f, 0.090f}, {0.10f, 0.08f, 0.07f}},  // rowan
    {{0.080f, 0.065f, 0.060f}, {0.110f, 0.100f, 0.090f}, {0.08f, 0.065f, 0.06f}},  // alder
    {{0.200f, 0.130f, 0.050f}, {0.130f, 0.110f, 0.080f}, {0.20f, 0.13f, 0.05f}},  // willow
    {{0.120f, 0.090f, 0.070f}, {0.250f, 0.250f, 0.240f}, {0.12f, 0.09f, 0.07f}},  // beech
    {{0.110f, 0.090f, 0.075f}, {0.130f, 0.110f, 0.090f}, {0.11f, 0.09f, 0.075f}},  // maple
    {{0.110f, 0.095f, 0.080f}, {0.200f, 0.200f, 0.180f}, {0.11f, 0.095f, 0.08f}},  // ash
};
// clang-format on
constexpr float kSnowRgb[3] = {0.86f, 0.88f, 0.92f};

}  // namespace

TreeForm tree_form(TreeKind kind) {
  switch (kind) {
    case TreeKind::pine:
    case TreeKind::stonepine:
    case TreeKind::whitepine:
    case TreeKind::lodgepole:
    case TreeKind::dwarfpine:
      return TreeForm::pine;
    case TreeKind::larch:
      return TreeForm::larch;
    case TreeKind::snag:
      return TreeForm::snag;
    case TreeKind::birch:
    case TreeKind::aspen:
    case TreeKind::rowan:
    case TreeKind::alder:
    case TreeKind::willow:
    case TreeKind::beech:
    case TreeKind::maple:
    case TreeKind::ash:
      return TreeForm::broadleaf;
    default:
      return TreeForm::conifer;
  }
}

int tree_variant(double x, double z) {
  const auto ix = static_cast<uint32_t>(static_cast<int64_t>(std::floor(x * 4)));
  const auto iz = static_cast<uint32_t>(static_cast<int64_t>(std::floor(z * 4)));
  const float j = jit(ix, iz, 77);
  return std::min(kTreeVariants - 1, static_cast<int>((j + 1) * 0.5f * kTreeVariants));
}

render::MeshData tree_mesh(TreeForm form, int variant, int lod) {
  render::MeshData m;
  variant = std::clamp(variant, 0, kTreeVariants - 1);
  lod = std::clamp(lod, 0, kTreeLods - 1);
  const float snow = kSnowCos[variant];
  Build b{m, snow, snow - 0.15f, kAspect[static_cast<size_t>(form)]};
  switch (form) {
    case TreeForm::conifer:
      conifer(b, variant, lod);
      break;
    case TreeForm::pine:
      pine(b, variant, lod);
      break;
    case TreeForm::larch:
      larch(b, variant, lod);
      // Far off, its haze of twigs with the light between them, paler than a twig.
      if (lod == 2) {
        for (uint8_t& p : m.parts) {
          if (p == kTwig) p = kFrost;
        }
      }
      break;
    case TreeForm::broadleaf:
      broadleaf(b, variant, lod);
      // A bare crown has no needles: the haze it is far off is its twigs with the light
      // between them, paler than a twig.
      for (uint8_t& p : m.parts) {
        if (p == kNeedle || p == kNeedleShade || p == kSnow) p = kFrost;
      }
      break;
    case TreeForm::snag:
      snag(b, variant, lod);
      break;
  }
  return m;
}

std::vector<render::MeshSource> tree_meshes() {
  std::vector<render::MeshSource> out(static_cast<size_t>(kTreeForms * kTreeVariants));
  for (int f = 0; f < kTreeForms; ++f) {
    for (int v = 0; v < kTreeVariants; ++v) {
      auto& src = out[tree_mesh_index(static_cast<TreeForm>(f), v)];
      for (int l = 0; l < kTreeLods; ++l) {
        src.lods.push_back({tree_mesh(static_cast<TreeForm>(f), v, l), kTreeLodPixels[l]});
      }
    }
  }
  return out;
}

std::vector<float> tree_materials() {
  std::vector<float> out;
  out.reserve(size_t{kTreeKinds} * kTreeParts * 3);
  for (const KindTone& t : kTones) {
    for (int p = 0; p < kTreeParts; ++p) {
      for (int c = 0; c < 3; ++c) {
        float v = 0;
        switch (p) {
          case kBark:
            v = t.bark[c];
            break;
          case kNeedle:
            v = t.needle[c];
            break;
          case kNeedleShade:
            v = t.needle[c] * 0.45f;
            break;
          case kSnow:
            v = kSnowRgb[c];
            break;
          case kFrost:
            v = t.needle[c] + (kSnowRgb[c] - t.needle[c]) * 0.45f;
            break;
          case kTwig:
            v = t.twig[c];
            break;
          default:
            break;
        }
        out.push_back(v);
      }
    }
  }
  return out;
}

render::Instance tree_instance(const mapgen::TreeDef& t, float turn) {
  render::Instance in;
  in.position[0] = t.x;
  in.position[1] = t.y;
  in.position[2] = t.z;
  in.rotation[1] = std::sin(turn * 0.5f);
  in.rotation[3] = std::cos(turn * 0.5f);
  const TreeForm form = tree_form(t.kind);
  // A snag's mesh trunk is 0.08 across in radius at its foot: scaled to its own girth.
  const float across = form == TreeForm::snag ? static_cast<float>(t.radius / 0.08)
                                              : static_cast<float>(std::max(t.crown, 0.3));
  in.scale[0] = in.scale[2] = across;
  in.scale[1] = static_cast<float>(std::max(t.height, 0.5));
  in.mesh = tree_mesh_index(form, tree_variant(t.x, t.z));
  in.material = tree_material(t.kind);
  return in;
}

}  // namespace ge::mountain::app
