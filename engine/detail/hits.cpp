// The marks a fight's events leave: bullet holes from shots, blood from wounds.
//
// ge:summary Bullet holes where shots land and blood scaled by the health a wound took
// ge:keywords detail hit shot bullet-hole wound bleed blood spatter pool damage
// ge:skill detail
// ge:test tests/detail_hits_test.cpp
#include "detail/hits.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"

namespace ge::detail {
namespace {

// How far under a wound the floor is looked for, m.
constexpr double kFloorReach = 3;

// A direction within `cone_rad` of `dir`, drooped by `droop` (a drop's fall over its flight).
Vec3 in_cone(core::Rng& rng, const Vec3& dir, double cone_rad, double droop) {
  const Vec3 u = any_perpendicular(dir);
  const Vec3 v = cross(dir, u);
  const double a = core::range(rng, 0, core::kTau);
  const double off = cone_rad * std::sqrt(core::next(rng));
  const double so = core::libm::sin(off);
  Vec3 d = add(mul(dir, core::libm::cos(off)),
               add(mul(u, core::libm::cos(a) * so), mul(v, core::libm::sin(a) * so)));
  d.y -= droop;
  return normalized(d);
}

// The decal of `kind` pressed where a ray from `from` along `dir` lands within `far_m`, facing
// back along the ray. `hit` says whether the ray met a surface at all; the slot is -1 when it met
// none or the surface took no mark.
int press_along(DecalPool& pool, const Receivers& rec, DecalKind kind, const Vec3& from,
                const Vec3& dir, double far_m, double size_m, double now_s, core::Rng& rng,
                Hit& hit, Vec3& normal) {
  hit = raycast(rec, from, dir, far_m, pool.scratch);
  if (!hit.hit) return -1;
  normal = normal_of(*rec.mesh, hit.tri);
  if (dot(normal, dir) > 0) normal = mul(normal, -1);
  DecalAt d;
  d.kind = kind;
  d.at = hit.at;
  d.normal = normal;
  d.size_m = size_m;
  d.spin_rad = core::range(rng, 0, core::kTau);
  d.variant = static_cast<uint32_t>(core::int_in(rng, 0, 3));
  d.now_s = now_s;
  return place_decal(pool, rec, d);
}

int press_along(DecalPool& pool, const Receivers& rec, DecalKind kind, const Vec3& from,
                const Vec3& dir, double far_m, double size_m, double now_s, core::Rng& rng) {
  Hit hit;
  Vec3 normal;
  return press_along(pool, rec, kind, from, dir, far_m, size_m, now_s, rng, hit, normal);
}

}  // namespace

ShotResult shot_hit(DecalPool& pool, const Receivers& rec, const ShotAt& s) {
  ShotResult r;
  if (rec.mesh == nullptr) return r;
  core::Rng rng = core::make_rng(s.seed * 2654435761u + 0x51ed);
  const double size = s.size_m * core::range(rng, 0.85, 1.15);
  Hit hit;
  r.slot = press_along(pool, rec, DecalKind::bullet_hole, s.from, normalized(s.dir), s.far_m, size,
                       s.now_s, rng, hit, r.normal);
  r.hit = hit.hit;
  r.at = hit.at;
  if (hit.hit) r.tri = static_cast<int>(hit.tri);
  return r;
}

BleedReport bleed(DecalPool& pool, const Receivers& rec, const WoundAt& w) {
  BleedReport r;
  if (rec.mesh == nullptr || w.max_health <= 0) return r;
  const double s = core::clamp(w.health_lost / w.max_health, 0, 1);
  r.share = s;
  if (s <= 0) return r;
  core::Rng rng = core::make_rng(w.seed * 2246822519u + 0xb100d);
  const Vec3 dir = normalized(w.shot_dir);
  const Vec3 down{0, -1, 0};
  auto took = [&](int slot) { r.pressed += slot >= 0 ? 1 : 0; };

  // The spatter thrown on along the shot: more, larger, wider and further the more it took.
  r.spatters = 1 + static_cast<int>(std::floor(5 * s + 0.5));
  const double cone = 0.3 + 0.45 * s, reach = 0.8 + 2.7 * s;
  for (int i = 0; i < r.spatters; ++i) {
    const Vec3 d = in_cone(rng, dir, cone, core::range(rng, 0, 0.3));
    const double size = (0.25 + 0.75 * s) * core::range(rng, 0.7, 1.1);
    took(press_along(pool, rec, DecalKind::blood, w.at, d, reach, size, w.now_s, rng));
  }
  // The pool under the wound.
  took(press_along(pool, rec, DecalKind::blood, w.at, down, kFloorReach, 0.35 + 1.75 * s, w.now_s,
                   rng));
  // The drops round the feet.
  r.drops = static_cast<int>(std::floor(4 * s + 0.5));
  for (int i = 0; i < r.drops; ++i) {
    const double a = core::range(rng, 0, core::kTau);
    const double off = core::range(rng, 0.15, 0.5 + 0.5 * s);
    const Vec3 from{w.at.x + core::libm::cos(a) * off, w.at.y, w.at.z + core::libm::sin(a) * off};
    took(press_along(pool, rec, DecalKind::blood, from, down, kFloorReach,
                     core::range(rng, 0.12, 0.22), w.now_s, rng));
  }
  return r;
}

}  // namespace ge::detail
