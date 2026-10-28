// Props: the rules' weights (the probes to the other surface, the damp, the clumping) and the
// scatter's per-triangle streams and spacing.
//
// ge:summary Props' weights, per-triangle streams, spacing grid and the default rules
// ge:keywords detail props weight probe scatter stream spacing default-rules
// ge:skill detail
// ge:test tests/detail_props_test.cpp
#include "detail/props.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/prng.hpp"

namespace ge::detail {
namespace {

constexpr double kFloorUp = 0.82;    // a floor's normal is within 35 degrees of up
constexpr double kWallUp = 0.35;     // a wall's within 20 degrees of level
constexpr double kProbeLift = 0.12;  // m above a floor the wall probes run at
constexpr double kBuriedReach = 50;  // m: how far a buried face's solid is looked through

bool place_ok(PropPlace place, const Vec3& n) {
  return place == PropPlace::floor ? n.y >= kFloorUp : (n.y < kWallUp && n.y > -kWallUp);
}

// How near the other surface is: the floor's nearest wall along eight level rays, or the wall's
// floor straight down; `far` when none is within it.
double other_surface(const Receivers& rec, const Vec3& p, const Vec3& n, PropPlace place,
                     double far, ReceiverScratch& s) {
  if (place == PropPlace::wall) {
    const Hit h = raycast(rec, mad(p, n, 0.02), {0, -1, 0}, far, s);
    return h.hit ? h.t : far;
  }
  double best = far;
  const Vec3 from = mad(p, n, kProbeLift);
  for (int k = 0; k < 8; ++k) {
    const double a = k * core::kTau / 8;
    const Hit h = raycast(rec, from, {core::libm::cos(a), 0, core::libm::sin(a)}, best, s);
    if (h.hit) best = std::min(best, h.t);
  }
  return best;
}

int64_t cell_of(const Vec3& p, double cell) {
  const auto x = static_cast<int64_t>(std::floor(p.x / cell));
  const auto y = static_cast<int64_t>(std::floor(p.y / cell));
  const auto z = static_cast<int64_t>(std::floor(p.z / cell));
  return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
}

}  // namespace

std::string_view prop_name(PropKind kind) {
  switch (kind) {
    case PropKind::rubble:
      return "rubble";
    case PropKind::debris:
      return "debris";
    case PropKind::moss:
      return "moss";
  }
  return "?";
}

std::vector<Prop> scatter_props(const TriMesh& mesh, const Receivers& rec,
                                std::span<const PropRule> rules, uint32_t seed, DampAt damp,
                                const void* ctx, ScatterReport* report) {
  std::vector<Prop> out;
  ReceiverScratch scratch;
  for (size_t ri = 0; ri < rules.size(); ++ri) {
    const PropRule& rule = rules[ri];
    const int kind = static_cast<int>(rule.kind);
    const double most = std::max(1.0, rule.open_weight) + rule.damp_weight;
    // The kept props of this rule by spacing cell (the cell is the spacing, so a neighbour
    // closer than it is in one of the 27 cells round).
    std::unordered_multimap<int64_t, Vec3> kept;
    for (uint32_t t = 0; t < mesh.triangles(); ++t) {
      const uint16_t tag = mesh.tag[t];
      if (tag < 64 && !((rule.tags >> tag) & 1u)) continue;
      const Vec3 n = normal_of(mesh, t);
      if (!place_ok(rule.place, n)) continue;
      core::Rng rng = core::make_rng(seed ^ (static_cast<uint32_t>(ri) * 2654435761u) ^
                                     (t * 2246822519u + 0x9e3779b9u));
      const double expect = area_of(mesh, t) * rule.per_m2 * most;
      const auto count = static_cast<int>(std::floor(expect + core::next(rng)));
      const Vec3& a = corner(mesh, t, 0);
      const Vec3 e1 = sub(corner(mesh, t, 1), a);
      const Vec3 e2 = sub(corner(mesh, t, 2), a);
      for (int i = 0; i < count; ++i) {
        double u = core::next(rng), v = core::next(rng);
        if (u + v > 1) {
          u = 1 - u;
          v = 1 - v;
        }
        const Vec3 p = mad(mad(a, e1, u), e2, v);
        const double keep = core::next(rng);
        const double size =
            core::lerp(rule.min_size_m, rule.max_size_m, core::next(rng) * core::next(rng));
        const double yaw = core::range(rng, 0, core::kTau);
        const auto variant = static_cast<uint8_t>(core::int_in(rng, 0, rule.variants - 1));
        if (report) ++report->tried[kind];
        double w = rule.open_weight;
        if (rule.near_m > 0) {
          const double d = other_surface(rec, p, n, rule.place, rule.near_m, scratch);
          w = std::max(w, 1 - d / rule.near_m);
        }
        const double wet = damp ? damp(ctx, p) : 0;
        w = w * (rule.dry_share + (1 - rule.dry_share) * wet) + rule.damp_weight * wet;
        const double heap = core::value_noise(p.x + p.y * 0.7, p.z - p.y * 0.3, rule.clump_m,
                                              seed + static_cast<double>(ri) * 31);
        w *= core::clamp(1 - rule.clump + rule.clump * 2 * heap, 0, 2);
        if (keep * most >= w) continue;
        if (buried_at(rec, p, n, kBuriedReach, scratch)) {
          if (report) ++report->buried[kind];
          continue;
        }
        // Spacing: a kept prop of this rule nearer than `spacing_m`.
        bool near = false;
        const double c = rule.spacing_m;
        for (int dz = -1; dz <= 1 && !near; ++dz) {
          for (int dy = -1; dy <= 1 && !near; ++dy) {
            for (int dx = -1; dx <= 1 && !near; ++dx) {
              const auto range =
                  kept.equal_range(cell_of({p.x + dx * c, p.y + dy * c, p.z + dz * c}, c));
              for (auto it = range.first; it != range.second; ++it) {
                if (length(sub(it->second, p)) < c) {
                  near = true;
                  break;
                }
              }
            }
          }
        }
        if (near) {
          if (report) ++report->too_close[kind];
          continue;
        }
        Prop pr;
        pr.kind = rule.kind;
        pr.variant = variant;
        pr.rule = static_cast<uint16_t>(ri);
        pr.tri = t;
        pr.up = n;
        pr.at = mad(p, n, -size * rule.sink_share);
        pr.yaw_rad = yaw;
        pr.size_m = size;
        kept.insert({cell_of(p, c), p});
        out.push_back(pr);
        if (report) ++report->placed[kind];
      }
    }
  }
  return out;
}

std::vector<PropRule> default_rules() {
  std::vector<PropRule> r(4);
  // Rubble: heaped against walls, a stone now and then in the open.
  r[0].kind = PropKind::rubble;
  r[0].per_m2 = 22;
  r[0].near_m = 0.7;
  r[0].open_weight = 0.03;
  r[0].clump = 0.7;
  r[0].min_size_m = 0.04;
  r[0].max_size_m = 0.32;
  r[0].spacing_m = 0.07;
  // Debris: splinters and shards spread thin over the floor, a little more near walls.
  r[1].kind = PropKind::debris;
  r[1].per_m2 = 3;
  r[1].near_m = 1.2;
  r[1].open_weight = 0.25;
  r[1].clump = 0.4;
  r[1].clump_m = 2.5;
  r[1].min_size_m = 0.06;
  r[1].max_size_m = 0.3;
  r[1].spacing_m = 0.15;
  r[1].sink_share = 0.05;
  // Moss on floors: in the damp, and in the corners where the damp stays.
  r[2].kind = PropKind::moss;
  r[2].per_m2 = 16;
  r[2].near_m = 0.6;
  r[2].open_weight = 0;
  r[2].damp_weight = 1.5;
  r[2].dry_share = 0.04;
  r[2].clump = 0.8;
  r[2].clump_m = 1.2;
  r[2].min_size_m = 0.16;
  r[2].max_size_m = 0.5;
  r[2].spacing_m = 0.14;
  r[2].sink_share = 0.35;
  // Moss low on walls, climbing out of the damp.
  r[3] = r[2];
  r[3].place = PropPlace::wall;
  r[3].per_m2 = 7;
  r[3].near_m = 0.9;
  r[3].damp_weight = 1.2;
  r[3].sink_share = 0.4;
  return r;
}

}  // namespace ge::detail
