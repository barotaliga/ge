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

// Open sky above `p`: straight up meets nothing within the reach, or a sky surface.
bool open_sky(const Receivers& rec, const Vec3& p, const Vec3& n, const ScatterEnv& env,
              ReceiverScratch& s) {
  const Hit h = raycast(rec, mad(p, n, 0.02), {0, 1, 0}, env.sky_reach_m, s);
  if (!h.hit) return true;
  const uint16_t tag = rec.mesh->tag[h.tri];
  return tag < 64 && ((env.sky_tags >> tag) & 1u);
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
    case PropKind::pebble:
      return "pebble";
    case PropKind::rock:
      return "rock";
    case PropKind::dirt:
      return "dirt";
    case PropKind::grass:
      return "grass";
    case PropKind::twig:
      return "twig";
    case PropKind::branch:
      return "branch";
    case PropKind::shell:
      return "shell";
    case PropKind::bone:
      return "bone";
  }
  return "?";
}

std::vector<Prop> scatter_props(const TriMesh& mesh, const Receivers& rec,
                                std::span<const PropRule> rules, uint32_t seed, DampAt damp,
                                const void* ctx, ScatterReport* report) {
  ScatterEnv env;
  env.damp = damp;
  env.ctx = ctx;
  return scatter_props(mesh, rec, rules, seed, env, report);
}

std::vector<Prop> scatter_props(const TriMesh& mesh, const Receivers& rec,
                                std::span<const PropRule> rules, uint32_t seed,
                                const ScatterEnv& env, ScatterReport* report) {
  const DampAt damp = env.damp;
  const void* ctx = env.ctx;
  std::vector<Prop> out;
  ReceiverScratch scratch;
  for (size_t ri = 0; ri < rules.size(); ++ri) {
    const PropRule& rule = rules[ri];
    const int kind = static_cast<int>(rule.kind);
    const bool by_sky = rule.outdoor_weight != 1 || rule.indoor_weight != 1;
    const double sky_most = std::max({1.0, rule.outdoor_weight, rule.indoor_weight});
    const double most = (std::max(1.0, rule.open_weight) + rule.damp_weight) * sky_most;
    // The kept props of this rule by spacing cell (the cell is the spacing, so a neighbour
    // closer than it is in one of the 27 cells round).
    std::unordered_multimap<int64_t, Vec3> kept;
    for (uint32_t t = 0; t < mesh.triangles(); ++t) {
      const uint16_t tag = mesh.tag[t];
      if (tag < 64 && !((rule.tags >> tag) & 1u)) continue;
      const SurfaceClass cls =
          tag < env.class_of_tag.size() ? env.class_of_tag[tag] : SurfaceClass::unknown;
      if (!(rule.classes & class_bit(cls))) continue;
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
        if (by_sky && w > keep * most) {
          w *= open_sky(rec, p, n, env, scratch) ? rule.outdoor_weight : rule.indoor_weight;
        }
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
  r[2].sink_share = 0.04;  // a cushion is a fifth of its width high: it sits on the surface
  // Moss low on walls, climbing out of the damp.
  r[3] = r[2];
  r[3].place = PropPlace::wall;
  r[3].per_m2 = 7;
  r[3].near_m = 0.9;
  r[3].damp_weight = 1.2;
  r[3].sink_share = 0.06;
  return r;
}

std::vector<PropRule> clutter_rules() {
  using C = SurfaceClass;
  const uint32_t grass = class_bit(C::grass), dirt = class_bit(C::dirt);
  const uint32_t stone = class_bit(C::stone), sand = class_bit(C::sand);
  const uint32_t wood = class_bit(C::wood);
  const uint32_t floors = ~(class_bit(C::sky) | class_bit(C::liquid));
  auto rule = [](PropKind kind, uint32_t classes, double per_m2, double sizes[2]) {
    PropRule r;
    r.kind = kind;
    r.classes = classes;
    r.per_m2 = per_m2;
    r.min_size_m = sizes[0];
    r.max_size_m = sizes[1];
    return r;
  };
  std::vector<PropRule> r;
  {
    // Grass: tufts all over a grass floor under the sky, thick in patches; a few under a roof.
    double s[2] = {0.14, 0.42};
    PropRule g = rule(PropKind::grass, grass, 30, s);
    g.near_m = 0;
    g.open_weight = 1;
    g.clump = 0.6;
    g.clump_m = 2;
    g.spacing_m = 0.1;
    g.sink_share = 0.05;
    g.indoor_weight = 0.06;
    r.push_back(g);
    // And along the edges of bare earth out in the open.
    PropRule e = g;
    e.classes = dirt;
    e.per_m2 = 10;
    e.near_m = 0.8;
    e.open_weight = 0.12;
    e.indoor_weight = 0;
    r.push_back(e);
  }
  {
    // Twigs and branches: what trees drop, so out in the open only, gathered under them.
    double s[2] = {0.14, 0.4};
    PropRule t = rule(PropKind::twig, grass | dirt, 2.5, s);
    t.near_m = 0;
    t.open_weight = 1;
    t.clump = 0.75;
    t.clump_m = 3;
    t.spacing_m = 0.1;
    t.sink_share = 0.08;
    t.indoor_weight = 0.04;
    r.push_back(t);
    double b[2] = {0.6, 1.4};
    PropRule br = rule(PropKind::branch, grass | dirt, 0.22, b);
    br.near_m = 0;
    br.open_weight = 1;
    br.clump = 0.7;
    br.clump_m = 4;
    br.spacing_m = 0.8;
    br.sink_share = 0.06;
    br.indoor_weight = 0;
    r.push_back(br);
  }
  {
    // Stones: small ones gathered at walls' feet and scattered thin; rocks fewer and larger.
    double s[2] = {0.03, 0.11};
    PropRule p = rule(PropKind::pebble, stone | dirt | sand, 16, s);
    p.near_m = 0.6;
    p.open_weight = 0.15;
    p.clump = 0.6;
    p.spacing_m = 0.04;
    p.sink_share = 0.25;
    r.push_back(p);
    double k[2] = {0.14, 0.55};
    PropRule rk = rule(PropKind::rock, stone | dirt | grass, 0.7, k);
    rk.near_m = 1;
    rk.open_weight = 0.15;
    rk.clump = 0.6;
    rk.clump_m = 3;
    rk.spacing_m = 0.45;
    rk.sink_share = 0.3;
    rk.indoor_weight = 0.4;
    r.push_back(rk);
  }
  {
    // Clods on bare earth.
    double s[2] = {0.07, 0.24};
    PropRule d = rule(PropKind::dirt, dirt, 5, s);
    d.near_m = 0;
    d.open_weight = 0.6;
    d.clump = 0.7;
    d.spacing_m = 0.1;
    d.sink_share = 0.35;
    r.push_back(d);
  }
  {
    // Moss in the damp, on stone, earth, grass and timber.
    PropRule m = default_rules()[2];
    m.classes = stone | dirt | grass | wood;
    r.push_back(m);
  }
  {
    // Under a roof, on the floors no grass or stone takes: the splinters and shards of a used
    // room, thicker at its walls.
    PropRule d = default_rules()[1];
    d.classes = class_bit(C::wood) | class_bit(C::metal) | class_bit(C::tile) | stone;
    d.per_m2 = 4;
    d.outdoor_weight = 0.15;
    r.push_back(d);
  }
  {
    // What a fight leaves: spent shells in tight heaps where someone stood and fired, bones thin
    // and near walls; on every floor.
    double s[2] = {0.04, 0.06};
    PropRule sh = rule(PropKind::shell, floors, 2, s);
    sh.near_m = 0;
    sh.open_weight = 1;
    sh.clump = 0.9;
    sh.clump_m = 1.4;
    sh.spacing_m = 0.025;
    sh.sink_share = 0;
    r.push_back(sh);
    double b[2] = {0.12, 0.42};
    PropRule bo = rule(PropKind::bone, floors, 0.18, b);
    bo.near_m = 1;
    bo.open_weight = 0.3;
    bo.clump = 0.6;
    bo.clump_m = 2;
    bo.spacing_m = 0.25;
    bo.sink_share = 0.1;
    r.push_back(bo);
  }
  return r;
}

}  // namespace ge::detail
