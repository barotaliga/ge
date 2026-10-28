// Decals: the kinds' table, their masks and looks, the atlas, the clip onto receivers and the pool.
//
// ge:summary Decals' kinds, masks, ageing looks, atlas bake, box clip and pool
// ge:keywords detail decal mask scorch bullet-hole blood clip atlas pool expire
// ge:skill detail
// ge:test tests/detail_decals_test.cpp
#include "detail/decals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/prng.hpp"

namespace ge::detail {
namespace {

constexpr DecalKindSpec kSpecs[kDecalKinds] = {
    {"scorch", 0, 0, 0, 0.55, 80, 1.5},
    {"bullet_hole", 0, 0, 0, 0.5, 60, 0},
    {"blood", 120, 60, 45, 0.4, 85, 0},
};

// The lift off the surface that keeps a decal in front of it, m.
constexpr double kLift = 0.001;
// How far a buried face's solid is looked through, m.
constexpr double kBuriedReach = 8;

double fbm(double x, double y, double scale, double seed) {
  double sum = 0, amp = 0.5, norm = 0;
  for (int o = 0; o < 3; ++o) {
    sum += amp * core::value_noise(x, y, scale, seed + o * 17);
    norm += amp;
    amp *= 0.5;
    scale *= 0.5;
  }
  return sum / norm;
}

// Noise round a circle at angle `a`, so the ragged edge closes on itself.
double round_noise(double a, double scale, double seed) {
  return fbm(core::libm::cos(a) * 4, core::libm::sin(a) * 4, scale, seed);
}

MaskSample scorch(uint32_t var, double x, double y, double r, double a) {
  const double seed = 101 + var * 7;
  // The burnt core, ragged; tongues thrown out by the blast; and a faint soot halo round both.
  const double edge = 0.3 + 0.1 * (round_noise(a, 1.6, seed) - 0.5);
  const double streak = ramp(0.5, 0.8, round_noise(a, 0.9, seed + 3));
  const double reach = edge + 0.12 * streak;
  const double core_cover = ramp(reach, reach - 0.26, r);
  const double halo = 0.8 * ramp(0.5, 0.16, r) * (0.65 + 0.7 * fbm(x * 5, y * 5, 1, seed + 4));
  double cover = std::max(core_cover, halo);
  cover *= 0.82 + 0.18 * fbm(x * 8, y * 8, 1, seed + 9);
  const double shade = -0.35 * ramp(0.3, 0, r) + 0.15 * (fbm(x * 20, y * 20, 1, seed) - 0.5);
  // The embers: patches of the burnt core and the blast's tongues, not the soot halo.
  const double ember = std::max(core_cover * ramp(0.42, 0.7, fbm(x * 14, y * 14, 1, seed + 21)),
                                0.8 * core_cover * streak * ramp(0.12, 0.3, r));
  return {core::clamp(cover, 0, 1), shade, core::clamp(ember, 0, 1)};
}

MaskSample bullet(uint32_t var, double x, double y, double r, double a) {
  const double seed = 211 + var * 11;
  const double hole = 0.07 + 0.015 * round_noise(a, 1, seed);
  if (r < hole) return {1, -1};
  // The crater: dark falling to a pale rim, ragged.
  const double rim = 0.17 + 0.05 * round_noise(a, 0.8, seed + 1);
  if (r < rim) {
    const double k = (r - hole) / (rim - hole);
    return {1, -0.7 + 1.1 * k};
  }
  // Cracks: a few rays, each a thin wedge of angle that thins out with distance.
  core::Rng rng = core::make_rng(var * 2654435761u + 7);
  const int cracks = core::int_in(rng, 3, 6);
  double crack = 0;
  for (int i = 0; i < cracks; ++i) {
    const double ca = core::range(rng, 0, core::kTau);
    const double len = core::range(rng, 0.28, 0.46);
    const double d = std::fabs(
        core::angle_diff(a, ca + 0.25 * (r - rim) * (fbm(r * 10, ca, 1, seed + i) - 0.5) * 4));
    const double width = 0.06 * (1 - (r - rim) / (len - rim));
    if (r < len && width > 0 && d < width) crack = std::max(crack, 1 - d / width);
  }
  // The dust the round threw: a faint pale halo.
  const double halo = 0.35 * ramp(0.42, 0.2, r) * fbm(x * 12, y * 12, 1, seed + 5);
  const double cover = std::max(crack * 0.85, halo);
  return {cover, crack > halo ? -0.5 : 0.35};
}

MaskSample blood(uint32_t var, double x, double y, double r, double a) {
  const double seed = 307 + var * 13;
  const double edge = 0.24 + 0.1 * (round_noise(a, 1.2, seed) - 0.5) * 2;
  // A run in one direction, the way a splash leaves a wound.
  core::Rng rng = core::make_rng(var * 2246822519u + 3);
  const double run_a = core::range(rng, 0, core::kTau);
  const double run = std::max(0.0, core::libm::cos(core::angle_diff(a, run_a)));
  const double reach = edge + 0.12 * run * run * run * run;
  double cover = ramp(reach, reach - 0.04, r);
  double rim = cover > 0 ? ramp(reach - 0.09, reach - 0.02, r) : 0;
  // The droplets round it.
  const int drops = core::int_in(rng, 6, 11);
  for (int i = 0; i < drops; ++i) {
    const double da = run_a + core::range(rng, -1.3, 1.3);
    const double dr = core::range(rng, 0.3, 0.47);
    const double size = core::range(rng, 0.012, 0.035) * (1.3 - dr);
    const double dx = x - core::libm::cos(da) * dr, dy = y - core::libm::sin(da) * dr;
    const double d = std::sqrt(dx * dx + dy * dy);
    const double c = ramp(size, size * 0.6, d);
    if (c > cover) {
      cover = c;
      rim = 0.5;
    }
  }
  return {cover, -0.15 + 0.6 * rim};
}

// Sutherland-Hodgman against dot(n, p) <= d.
void clip_loop(const std::vector<Vec3>& in, const Vec3& n, double d, std::vector<Vec3>& out) {
  out.clear();
  const size_t k = in.size();
  for (size_t i = 0; i < k; ++i) {
    const Vec3& a = in[i];
    const Vec3& b = in[(i + 1) % k];
    const double da = dot(n, a) - d, db = dot(n, b) - d;
    if (da <= 0) out.push_back(a);
    if ((da < 0 && db > 0) || (da > 0 && db < 0)) out.push_back(mad(a, sub(b, a), da / (da - db)));
  }
}

// A clipped piece wholly inside a solid: its middle and each corner (drawn a centimetre in toward
// the middle, off the edges a neighbour may stand on) all buried. A piece half under a block
// stays; the block's own faces hide that half.
bool piece_buried(const Receivers& rec, const std::vector<Vec3>& poly, const Vec3& n,
                  ReceiverScratch& s) {
  Vec3 mid{0, 0, 0};
  for (const Vec3& v : poly) mid = add(mid, v);
  mid = mul(mid, 1.0 / static_cast<double>(poly.size()));
  if (!buried_at(rec, mid, n, kBuriedReach, s)) return false;
  for (const Vec3& v : poly) {
    const Vec3 in = sub(mid, v);
    const double d = length(in);
    const Vec3 p = d > 0.02 ? mad(v, in, 0.01 / d) : mid;
    if (!buried_at(rec, p, n, kBuriedReach, s)) return false;
  }
  return true;
}

}  // namespace

const DecalKindSpec& decal_spec(DecalKind kind) { return kSpecs[static_cast<int>(kind)]; }

DecalLook look_at(DecalKind kind, double age_s) {
  const DecalKindSpec& s = decal_spec(kind);
  switch (kind) {
    case DecalKind::scorch: {
      // Embers: white-orange at the blast, cooling through red to nothing over `ember_s`; the
      // square of what is left so the glow falls fast and lingers as a dull red.
      const double hot = 1 - ramp(0, s.ember_s, age_s);
      const double k = hot * hot;
      return {0.035, 0.03, 0.025, 0.92, 0, 2.6 * k, 0.9 * k * hot, 0.2 * k * hot * hot};
    }
    case DecalKind::bullet_hole:
      return {0.09, 0.085, 0.08, 1, 0};
    case DecalKind::blood: {
      const double dry = ramp(0, s.dry_s, age_s);
      const double fade = core::clamp((s.life_s - age_s) / s.fade_s, 0, 1);
      return {core::lerp(0.32, 0.12, dry), core::lerp(0.012, 0.03, dry),
              core::lerp(0.012, 0.02, dry), 0.95 * fade, core::lerp(0.85, 0.08, dry)};
    }
  }
  return {0, 0, 0, 0, 0};
}

MaskSample mask_at(DecalKind kind, uint32_t variant, double u, double v) {
  if (u < 0 || u > 1 || v < 0 || v > 1) return {0, 0};
  const double x = u - 0.5, y = v - 0.5;
  const double r = std::sqrt(x * x + y * y);
  if (r >= 0.5) return {0, 0};
  const double a = core::libm::atan2(y, x);
  switch (kind) {
    case DecalKind::scorch:
      return scorch(variant, x, y, r, a);
    case DecalKind::bullet_hole:
      return bullet(variant, x, y, r, a);
    case DecalKind::blood:
      return blood(variant, x, y, r, a);
  }
  return {0, 0};
}

std::vector<uint8_t> bake_atlas(int size, int variants) {
  const int w = size * variants, h = size * kDecalKinds;
  std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4, 0);
  for (int k = 0; k < kDecalKinds; ++k) {
    for (int var = 0; var < variants; ++var) {
      for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
          const MaskSample m = mask_at(static_cast<DecalKind>(k), static_cast<uint32_t>(var),
                                       (x + 0.5) / size, (y + 0.5) / size);
          const size_t o =
              ((static_cast<size_t>(k) * size + y) * w + static_cast<size_t>(var) * size + x) * 4;
          const auto c = static_cast<uint8_t>(std::lround(m.cover * 255));
          px[o] = c;
          px[o + 1] = static_cast<uint8_t>(std::lround((m.shade * 0.5 + 0.5) * 255));
          px[o + 2] = static_cast<uint8_t>(std::lround(m.ember * 255));
          px[o + 3] = c;
        }
      }
    }
  }
  return px;
}

DecalPool make_pool(uint32_t capacity, uint32_t verts_per_decal) {
  DecalPool p;
  p.capacity = capacity;
  p.verts_per_decal = verts_per_decal - verts_per_decal % 3;
  p.decals.resize(capacity);
  p.verts.resize(static_cast<size_t>(capacity) * p.verts_per_decal);
  p.poly.reserve(16);
  p.next.reserve(16);
  p.staged.reserve(size_t{p.verts_per_decal} * 4);
  p.order.reserve(size_t{p.verts_per_decal} * 4 / 3);
  p.kept.reserve(p.verts_per_decal);
  return p;
}

int place_decal(DecalPool& pool, const Receivers& rec, const DecalAt& d) {
  if (pool.capacity == 0 || rec.mesh == nullptr) return -1;
  const DecalKindSpec& spec = decal_spec(d.kind);
  const Vec3 n = normalized(d.normal);
  const Vec3 t0 = any_perpendicular(n);
  const Vec3 b0 = cross(n, t0);
  const double cs = core::libm::cos(d.spin_rad), sn = core::libm::sin(d.spin_rad);
  const Vec3 t = add(mul(t0, cs), mul(b0, sn));
  const Vec3 b = cross(n, t);
  const double half = d.size_m * 0.5;
  const double depth = std::max(0.02, d.size_m * spec.depth_share);
  const double reach = half * 1.4143 + depth;
  triangles_in(rec, {d.at.x - reach, d.at.y - reach, d.at.z - reach},
               {d.at.x + reach, d.at.y + reach, d.at.z + reach}, pool.scratch);
  const double min_cos = core::libm::cos(spec.max_angle_deg * core::kPi / 180);
  const Vec3 planes[6] = {t, mul(t, -1), b, mul(b, -1), n, mul(n, -1)};
  const double dists[6] = {dot(t, d.at) + half,  -dot(t, d.at) + half, dot(b, d.at) + half,
                           -dot(b, d.at) + half, dot(n, d.at) + depth, -dot(n, d.at) + depth};
  pool.staged.clear();
  for (uint32_t tri : pool.scratch.found) {
    const Vec3 tn = normal_of(*rec.mesh, tri);
    const double facing = dot(tn, n);
    if (facing < min_cos) continue;
    pool.poly = {corner(*rec.mesh, tri, 0), corner(*rec.mesh, tri, 1), corner(*rec.mesh, tri, 2)};
    for (int k = 0; k < 6 && pool.poly.size() >= 3; ++k) {
      clip_loop(pool.poly, planes[k], dists[k], pool.next);
      pool.poly.swap(pool.next);
    }
    if (pool.poly.size() < 3) continue;
    if (piece_buried(rec, pool.poly, tn, pool.scratch_up)) continue;
    const double fade = ramp(min_cos, min_cos + (1 - min_cos) * 0.5, facing);
    auto vert = [&](const Vec3& p) {
      const Vec3 rel = sub(p, d.at);
      return DecalVertex{mad(p, tn, kLift), dot(rel, t) / d.size_m + 0.5,
                         dot(rel, b) / d.size_m + 0.5, fade};
    };
    for (size_t k = 1; k + 1 < pool.poly.size(); ++k) {
      pool.staged.push_back(vert(pool.poly[0]));
      pool.staged.push_back(vert(pool.poly[k]));
      pool.staged.push_back(vert(pool.poly[k + 1]));
    }
  }
  if (pool.staged.empty()) return -1;
  if (pool.staged.size() > pool.verts_per_decal) {
    // Too many for a slot: keep the triangles nearest the centre (ties by their order).
    const size_t tris = pool.staged.size() / 3;
    auto& order = pool.order;
    order.clear();
    for (size_t i = 0; i < tris; ++i) {
      const Vec3 c = mul(
          add(add(pool.staged[i * 3].pos, pool.staged[i * 3 + 1].pos), pool.staged[i * 3 + 2].pos),
          1.0 / 3);
      order.push_back({length(sub(c, d.at)), i});
    }
    std::sort(order.begin(), order.end());
    auto& kept = pool.kept;
    kept.clear();
    for (size_t k = 0; k < order.size() && kept.size() + 3 <= pool.verts_per_decal; ++k) {
      for (int j = 0; j < 3; ++j) kept.push_back(pool.staged[order[k].second * 3 + j]);
    }
    pool.staged.swap(kept);
  }
  // A free slot, else the oldest placed.
  uint32_t slot = 0;
  uint32_t oldest = UINT32_MAX;
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    if (!pool.decals[i].live) {
      slot = i;
      break;
    }
    if (pool.decals[i].serial < oldest) {
      oldest = pool.decals[i].serial;
      slot = i;
    }
  }
  Decal& dc = pool.decals[slot];
  dc = {true,
        d.kind,
        d.variant,
        pool.next_serial++,
        d.now_s,
        d.at,
        n,
        d.size_m,
        static_cast<uint32_t>(pool.staged.size())};
  std::copy(pool.staged.begin(), pool.staged.end(),
            pool.verts.begin() + static_cast<std::ptrdiff_t>(slot) * pool.verts_per_decal);
  return static_cast<int>(slot);
}

void expire(DecalPool& pool, double now_s) {
  for (Decal& d : pool.decals) {
    const double life = decal_spec(d.kind).life_s;
    if (d.live && life > 0 && now_s - d.born_s >= life) d.live = false;
  }
}

uint32_t live_count(const DecalPool& pool) {
  uint32_t n = 0;
  for (const Decal& d : pool.decals) n += d.live ? 1 : 0;
  return n;
}

std::span<const DecalVertex> decal_verts(const DecalPool& pool, uint32_t i) {
  return {pool.verts.data() + static_cast<size_t>(i) * pool.verts_per_decal, pool.decals[i].count};
}

}  // namespace ge::detail
