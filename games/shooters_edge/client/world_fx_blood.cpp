// THE BLOOD IN THE MODERN LOOK (ce_blood, client/world_fx.hpp § The blood): a body struck sprays
// blood shaped by what struck it (app/weapon_fx.hpp's `blood_spray`) and paints the walls and the
// floor near it (engine/detail's blood decals, pressed on the marks' faces, drying and fading).
//
// The hit's message carries only its place and its count, so the blow is worked out from what the
// effects see: the arc gun's blood is its own message; a nail that flew within reach of the wound
// a frame ago drove it (along that nail's flight); else the player whose aim runs through the
// wound fired it (the viewer's own eye and aim first, then each other player's facing): a blade's
// cut when he stands within arm's reach, a pellet's otherwise; nobody: a pellet's, from the eye. A
// blast that goes off near another player bleeds him too (Quake sends no blood for it);
// the viewer's own body does not spray into his eyes.
//
// The blood on the faces stays for ce_blood_time and darkens as it dries. What falls under the
// wound GATHERS INTO A POOL on the floor: a drop near a pool still there runs into it and the pool
// is pressed again a little larger in its own slot, spreading on from its edge, so a fight leaves
// one growing pool under the fighters and not a scatter of spots; a wet pool mirrors the room
// (render/marks.hpp's paint, ce_blood_reflect). A big splash on a wall DRIPS: a run of blood down
// the wall from it (engine/detail's blood_drip), running for a few seconds.
//
// THE WOUNDS: a blow wounds the body along its line (world_fx.cpp's `wound`): where the line first
// crosses the skin, and for a blow that goes through (a nail; a point-blank load of pellets) where
// it comes out, whose spray and spatter leave from there onto what is behind. Each wound's blood
// soaks the cloth round it and runs down it (world_fx.cpp's `body_blood`); a fresh one lets drops
// fall (`let_drop`) that mark the floor where they land (`land_drops`): a trail behind a wounded
// player, a pool under a still one. A body standing in a wet pool takes blood on its soles and
// leaves kPrints prints a stride apart as it walks on, fainter each step (`tread_blood`).
//
// ge:summary The modern look's blood: a spray by the weapon, splashes, drips and gathering pools
// ge:keywords blood ce_blood gore splatter spray decal wound by-weapon blast-blood bleed_fx
// ge:keywords blood_blow splash floor-pool blood-pool gather drip run-down ce_blood_time
// ge:keywords exit-wound entry-wound through-and-through back-spatter boot-print bloody-footprints
// ge:keywords tread_blood let_drop land_drops blood-trail
// ge:skill render shooter detail
// ge:test tests/shooters_edge_weapon_fx_test.cpp
#include <algorithm>
#include <cmath>

#include "brush/light_probe.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "detail/mesh.hpp"
#include "detail/receivers.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {

constexpr float kNailReachU = 48;    // a nail this near the wound a frame ago drove it
constexpr float kAimMissU = 40;      // a hit this far off a player's aim line is not his
constexpr float kBladeReachU = 80;   // a striker this near cut it
constexpr float kBlastBleedU = 160;  // a player this near a blast bleeds
constexpr float kChestU = 8;         // a body's middle over its origin
constexpr double kFloorUp = 0.7;     // a face turned up past this is a floor (pools)
constexpr double kWallUp = 0.35;     // one turned less than this up or down is a wall (drips)
constexpr double kPoolMaxM = 2.0;    // a pool grows no wider, m (its box; the blood ~70 % of it)
constexpr double kPoolGrow = 0.7;    // the share of a drop's area a pool gains from it
constexpr double kPoolEdge = 0.36;   // a pool's edge from its middle, as a share of its box
constexpr double kPoolRunM = 0.1;    // a drop this far off a pool's edge still runs into it, m
constexpr double kDripShare = 0.6;   // a drip's width as a share of the splash it runs from
constexpr double kDripMinM = 0.18;   // a splash smaller than this does not drip
constexpr double kDropM = 0.05;      // a drop fallen from a wound, its mark across, m
constexpr double kStrideM = 0.75;    // a print every this far a body walks, m
constexpr int kPrints = 6;           // the prints a step in wet blood leaves
constexpr double kPrintM = 0.34;     // a print's box, m
constexpr double kStepPoolM = 0.3;   // a pool smaller than this (a drop's) wets no sole, m
constexpr double kWetGloss = 0.35;   // a pool glossier than this is wet enough to step out of
constexpr double kFeetU = 24;        // a body's feet under its origin, units
constexpr double kGravity = 9.81;

void put(const ge::camera::Vec3& v, double out[3]) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
}

double len3(const double d[3]) { return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); }

// The way from `a` to `b` (map units) as a unit one in the engine's axes; false when they meet.
bool way_of(const sh::Vec3f& a, const sh::Vec3f& b, double out[3]) {
  const ge::camera::Vec3 d = engine_point({b.x - a.x, b.y - a.y, b.z - a.z});
  out[0] = d.x, out[1] = d.y, out[2] = d.z;
  const double l = len3(out);
  if (l < 1e-9) return false;
  for (int k = 0; k < 3; ++k) out[k] /= l;
  return true;
}

// How far `at` lies off the aim from `eye` along `fwd` (map units, unit), ahead of it; -1 behind.
float off_aim(const sh::Vec3f& eye, const sh::Vec3f& fwd, const sh::Vec3f& at) {
  const sh::Vec3f d = {at.x - eye.x, at.y - eye.y, at.z - eye.z};
  const float along = d.x * fwd.x + d.y * fwd.y + d.z * fwd.z;
  if (along <= 0) return -1;
  const float x = d.x - fwd.x * along, y = d.y - fwd.y * along, z = d.z - fwd.z * along;
  return std::sqrt(x * x + y * y + z * z);
}

// A facing (pitch down positive, yaw; degrees) as a unit way in map units, z up.
sh::Vec3f facing(float pitch, float yaw) {
  const double p = pitch * ge::core::kPi / 180, y = yaw * ge::core::kPi / 180;
  const double cp = ge::core::libm::cos(p);
  return {static_cast<float>(cp * ge::core::libm::cos(y)),
          static_cast<float>(cp * ge::core::libm::sin(y)),
          static_cast<float>(-ge::core::libm::sin(p))};
}

struct BloodBlow {
  fx::BloodBy by = fx::BloodBy::any;
  double dir[3] = {0, 0, 1};
};

// What struck the body at `e.at` and the way it went (see the file's head).
BloodBlow blow_of(const FxScene& l, const FxState& s, const Effect& e, const sh::Vec3f& eye_map) {
  BloodBlow b;
  if (!way_of(eye_map, e.at, b.dir)) b.dir[1] = -1;
  if (l.staged_by != fx::BloodBy::any) {  // a staged fight's (a lab)
    b.by = l.staged_by;
    if (!way_of(l.staged_from, e.at, b.dir)) b.dir[1] = -1;
    return b;
  }
  if (e.code == 13) {
    b.by = fx::BloodBy::arc;
    return b;
  }
  // A program's burst carries its own way.
  if (e.code == -1 && (e.way.x != 0 || e.way.y != 0 || e.way.z != 0)) {
    way_of({0, 0, 0}, e.way, b.dir);
    return b;
  }
  for (const auto& [id, f] : s.flights_was) {
    if (!f.nail) continue;
    const float dx = f.at.x - e.at.x, dy = f.at.y - e.at.y, dz = f.at.z - e.at.z;
    if (dx * dx + dy * dy + dz * dz > kNailReachU * kNailReachU) continue;
    b.by = fx::BloodBy::nail;
    if (!way_of({0, 0, 0}, f.dir, b.dir)) way_of(f.at, e.at, b.dir);
    return b;
  }
  // The striker: the viewer, else the player whose aim runs nearest the wound.
  sh::Vec3f from = eye_map;
  float best = -1;
  if (l.own != 0) {
    const float miss = off_aim(eye_map, facing(l.me.v_angle.x, l.me.v_angle.y), e.at);
    if (miss >= 0 && miss < kAimMissU) best = miss;
  }
  if (best < 0 && l.placed_of) {
    for (const Body& body : l.bodies) {
      if (!body.alive || body.entity == l.own) continue;
      const Placed m = l.placed_of(body.entity);
      if (m.entity == 0) continue;
      const sh::Vec3f eye = {body.origin.x, body.origin.y, body.origin.z + 22};
      // A figure's pitch is a third of its aim's, the other way (the classic models').
      const float miss = off_aim(eye, facing(-3 * m.angles.x, m.angles.y), e.at);
      if (miss < 0 || miss >= kAimMissU || (best >= 0 && miss >= best)) continue;
      best = miss;
      from = eye;
    }
  }
  if (best < 0) return b;  // nobody's: a pellet's look from the eye
  way_of(from, e.at, b.dir);
  const float dx = from.x - e.at.x, dy = from.y - e.at.y, dz = from.z - e.at.z;
  const bool close = dx * dx + dy * dy + dz * dz < kBladeReachU * kBladeReachU;
  b.by = close && e.count <= 3 ? fx::BloodBy::blade : fx::BloodBy::pellet;
  return b;
}

// The map's light at `p` (engine m) as the marks are lit (1 as painted); false when not known.
bool light_at(const FxState& s, const double p[3], float rgb[3]) {
  if (!s.receivers || s.marked == nullptr) return false;
  const float q[3] = {static_cast<float>(p[0] / ge::brush::kUnitM),
                      static_cast<float>(p[2] / ge::brush::kUnitM),
                      static_cast<float>(p[1] / ge::brush::kUnitM)};
  if (!ge::brush::light_below(s.receivers->probe, *s.marked, q, rgb)) return false;
  for (int k = 0; k < 3; ++k) rgb[k] *= 2;
  return true;
}

// How the blood of each blow paints what is near: spatters thrown on along the blow (how many a
// hit, their cone, how far they reach, m, and their size, m) and the pool under the wound (m; 0
// none).
struct Splash {
  int spatters;
  double cone, reach_m, size_m, pool_m;
};
Splash splash_of(fx::BloodBy by) {
  switch (by) {
    case fx::BloodBy::nail:
      return {2, 0.2, 4.0, 0.24, 0.22};
    case fx::BloodBy::blade:
      return {3, 0.6, 2.0, 0.3, 0.3};
    case fx::BloodBy::arc:
      return {1, 0.8, 1.0, 0.14, 0};
    case fx::BloodBy::blast:
      return {7, 0.9, 4.0, 0.55, 0.8};
    case fx::BloodBy::drip:
    case fx::BloodBy::any:
    case fx::BloodBy::pellet:
      break;
  }
  return {1, 0.5, 3.0, 0.24, 0.18};
}

// Blood falling on the floor at `at` (normal `n`): into the pool there is one near, pressed
// again larger in its slot and spreading on from its edge, else a new pool `size` m across.
void pool_at(FxState& s, const ge::detail::Vec3& at, const ge::detail::Vec3& n, double size,
             double t, ge::core::Rng& r) {
  namespace gd = ge::detail;
  const gd::DecalPool& marks = s.marks;
  int best = -1;
  double best_d = 1e9;
  for (uint32_t i = 0; i < marks.capacity; ++i) {
    const gd::Decal& d = marks.decals[i];
    if (!d.live || d.kind != gd::DecalKind::blood_pool) continue;
    if (std::fabs(d.at.y - at.y) > 0.06 || gd::look_of(d, t).opacity < 0.2) continue;
    const double dx = d.at.x - at.x, dz = d.at.z - at.z;
    const double dist = std::sqrt(dx * dx + dz * dz);
    if (dist > kPoolEdge * (d.size_m + size) + kPoolRunM || dist >= best_d) continue;
    best = static_cast<int>(i);
    best_d = dist;
  }
  gd::DecalAt m;
  m.kind = gd::DecalKind::blood_pool;
  m.normal = n;
  m.now_s = t;
  m.life_s = s.on.blood_life_s;
  if (best >= 0) {
    const gd::Decal& d = marks.decals[static_cast<size_t>(best)];
    const double grown =
        std::min(kPoolMaxM, std::sqrt(d.size_m * d.size_m + kPoolGrow * size * size));
    // Its middle drawn toward the drop by the share of the area it brings.
    const double share = 0.5 * (1 - d.size_m * d.size_m / (grown * grown));
    m.at = {d.at.x + (at.x - d.at.x) * share, d.at.y, d.at.z + (at.z - d.at.z) * share};
    m.normal = d.normal;
    m.size_m = grown;
    m.spin_rad = d.spin_rad;
    m.variant = d.variant;
    m.slot = best;
    m.serial = d.serial;
    m.spread_from = gd::look_of(d, t).spread * d.size_m / grown;
  } else {
    m.at = at;
    m.size_m = size;
    m.spin_rad = ge::core::range(r, 0, ge::core::kTau);
    m.variant = static_cast<uint32_t>(ge::core::int_in(r, 0, 3));
  }
  press_mark(s, m);
}

// A splash `size` m across at `at` on a wall (normal `n`) running down it.
void drip_at(FxState& s, const ge::detail::Vec3& at, const ge::detail::Vec3& n, double size,
             double t, ge::core::Rng& r) {
  namespace gd = ge::detail;
  // The way down the wall, laid flat on it.
  gd::Vec3 down{n.x * n.y, -1 + n.y * n.y, n.z * n.y};
  const double l = gd::length(down);
  if (l < 1e-6) return;
  down = gd::mul(down, 1 / l);
  gd::DecalAt m;
  m.kind = gd::DecalKind::blood_drip;
  m.normal = n;
  m.size_m = size * kDripShare * ge::core::range(r, 0.85, 1.15);
  m.length_m = gd::kDripAspect * m.size_m;
  m.along = down;
  // The blot at the top of its box on the splash.
  m.at = gd::mad(at, down, (0.5 - gd::kDripBlotU) * m.length_m);
  m.variant = static_cast<uint32_t>(ge::core::int_in(r, 0, 3));
  m.now_s = t;
  m.life_s = s.on.blood_life_s;
  press_mark(s, m);
}

// The share of a blow's splashes on a wall that run down it.
double drip_share(fx::BloodBy by) {
  switch (by) {
    case fx::BloodBy::blast:
      return 0.8;
    case fx::BloodBy::blade:
      return 0.7;
    case fx::BloodBy::nail:
      return 0.4;
    case fx::BloodBy::arc:
      return 0;
    case fx::BloodBy::drip:
    case fx::BloodBy::any:
    case fx::BloodBy::pellet:
      break;
  }
  return 0.55;
}

// The blood of blow `by` at `p` (engine m) along `dir` (unit) painted onto the faces near.
void splatter(FxState& s, fx::BloodBy by, const double p[3], const double dir[3], int count,
              double t) {
  if (!s.receivers) return;
  const ge::detail::Receivers& rec = s.receivers->receivers;
  ge::core::Rng r = ge::core::make_rng(s.seed++ * 2246822519u + 0xb100du);
  const Splash k = splash_of(by);
  // A blade's spatter flies sideways across the cut.
  double axis[3] = {dir[0], dir[1], dir[2]};
  if (by == fx::BloodBy::blade) axis[0] = -dir[2], axis[1] = 0.2, axis[2] = dir[0];
  const auto paint = [&](const ge::detail::Vec3& from, const ge::detail::Vec3& d, double reach,
                         double size, bool pool) {
    const ge::detail::Hit hit = ge::detail::raycast(rec, from, d, reach, s.marks.scratch);
    if (!hit.hit) return;
    ge::detail::Vec3 n = ge::detail::normal_of(*rec.mesh, hit.tri);
    if (n.x * d.x + n.y * d.y + n.z * d.z > 0) n = {-n.x, -n.y, -n.z};
    if (pool && n.y > kFloorUp) {
      pool_at(s, hit.at, n, size, t, r);
      return;
    }
    ge::detail::DecalAt m;
    m.kind = ge::detail::DecalKind::blood;
    m.at = hit.at;
    m.normal = n;
    // Further off, a spatter spreads wider.
    m.size_m = size * (0.7 + 0.5 * hit.t / std::max(reach, 0.1)) * ge::core::range(r, 0.7, 1.1);
    m.spin_rad = ge::core::range(r, 0, ge::core::kTau);
    m.variant = static_cast<uint32_t>(ge::core::int_in(r, 0, 3));
    m.now_s = t;
    m.life_s = s.on.blood_life_s;
    press_mark(s, m);
    // A big splash on a wall runs down it.
    if (std::fabs(n.y) < kWallUp && m.size_m >= kDripMinM && ge::core::next(r) < drip_share(by))
      drip_at(s, hit.at, n, m.size_m, t, r);
  };
  const ge::detail::Vec3 from{p[0], p[1], p[2]};
  const int spatters = std::min(k.spatters * std::clamp(count, 1, 4), 10);
  for (int i = 0; i < spatters; ++i) {
    double d[3];
    for (int j = 0; j < 3; ++j) d[j] = axis[j] + k.cone * ge::core::range(r, -1, 1);
    d[1] -= 0.15;  // it falls as it flies
    const double l = len3(d);
    if (l < 1e-6) continue;
    paint(from, {d[0] / l, d[1] / l, d[2] / l}, k.reach_m, k.size_m, false);
  }
  if (k.pool_m > 0)
    paint(from, {ge::core::range(r, -0.15, 0.15), -1, ge::core::range(r, -0.15, 0.15)}, 2.5,
          k.pool_m * (1 + 0.2 * (std::clamp(count, 1, 6) - 1)), true);
}

// How deep a blow `by` of `count` pellets wounds: whether it goes through, and its wounds' soaked
// patch and run (m).
WoundCut cut_of(fx::BloodBy by, int count) {
  switch (by) {
    case fx::BloodBy::nail:
      return {true, 0.08f, 0.4f};
    case fx::BloodBy::blade:
      return {false, 0.12f, 0.55f};
    case fx::BloodBy::blast:
      return {false, 0.12f, 0.45f};
    case fx::BloodBy::arc:
      return {false, 0.03f, 0.1f};
    case fx::BloodBy::drip:
    case fx::BloodBy::any:
    case fx::BloodBy::pellet:
      break;
  }
  // A shotgun's pellets lodge; only a point-blank load of them goes through.
  return {count >= 8, 0.065f + 0.005f * static_cast<float>(std::min(count, 10)), 0.35f};
}

}  // namespace

void bleed_fx(const FxScene& l, FxState& s, const Effect& e, const double p[3],
              const sh::Vec3f& eye_map, double floor_y, double t, FxFrame& out) {
  const BloodBlow b = blow_of(l, s, e, eye_map);
  fx::BloodHit h;
  for (int k = 0; k < 3; ++k) {
    h.at[k] = p[k];
    h.dir[k] = b.dir[k];
  }
  // A program's burst counts its drops; a match's message its pellets.
  h.count = e.code == -1 ? std::clamp(e.count / 8, 1, 6) : std::max(e.count, 1);
  h.by = b.by;
  h.floor_y = floor_y;
  // The wound on the body along the blow's line (not the arc's sizzle), and its way out.
  if (b.by != fx::BloodBy::arc) {
    const ge::camera::Vec3 from{p[0] - b.dir[0] * 3, p[1] - b.dir[1] * 3, p[2] - b.dir[2] * 3};
    h.through = wound(s, out.models, e.at, p, from, std::clamp(h.count, 1, 8), t,
                      cut_of(b.by, h.count), h.exit);
  }
  light_at(s, p, h.light);
  fx::blood_spray(h, s.seed++, &out.births);
  // A round through the body throws its spatter out of the exit, onto what is behind.
  splatter(s, b.by, h.through ? h.exit : p, b.dir, h.count, t);
}

void blast_blood(const FxScene& l, FxState& s, const sh::Vec3f& boom, double t, FxFrame& out) {
  for (const Body& body : l.bodies) {
    if (!body.alive || body.entity == l.own) continue;  // the eye's own: not in its face
    const sh::Vec3f mid = {body.origin.x, body.origin.y, body.origin.z + kChestU};
    const float dx = mid.x - boom.x, dy = mid.y - boom.y, dz = mid.z - boom.z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > kBlastBleedU * kBlastBleedU) continue;
    fx::BloodHit h;
    put(engine_point(mid), h.at);
    if (!way_of(boom, mid, h.dir)) h.dir[1] = 1;
    // Nearer, harder: up to three bursts' worth.
    h.count = 1 + static_cast<int>(2 * (1 - std::sqrt(d2) / kBlastBleedU));
    h.by = fx::BloodBy::blast;
    if (l.map) h.floor_y = map_floor_under(*l.map, mid, mid);
    // The side toward the blast torn: a wound along the line from it.
    const ge::camera::Vec3 from = engine_point(boom);
    wound(s, out.models, mid, h.at, from, 4, t, cut_of(fx::BloodBy::blast, h.count));
    light_at(s, h.at, h.light);
    for (int i = 0; i < h.count; ++i) fx::blood_spray(h, s.seed++, &out.births);
    splatter(s, fx::BloodBy::blast, h.at, h.dir, h.count, t);
  }
}

void let_drop(FxState& s, const double at[3], double t, FxFrame& out) {
  if (!s.receivers) return;
  const ge::detail::Receivers& rec = s.receivers->receivers;
  const ge::detail::Vec3 from{at[0], at[1], at[2]};
  const ge::detail::Hit hit = ge::detail::raycast(rec, from, {0, -1, 0}, 3.0, s.marks.scratch);
  fx::BloodHit h;
  for (int k = 0; k < 3; ++k) h.at[k] = at[k];
  h.dir[0] = h.dir[1] = h.dir[2] = 0;
  h.by = fx::BloodBy::drip;
  h.floor_y = hit.hit ? hit.at.y : -1e9;
  light_at(s, at, h.light);
  fx::blood_spray(h, s.seed++, &out.births);
  if (!hit.hit) return;
  ge::detail::Vec3 n = ge::detail::normal_of(*rec.mesh, hit.tri);
  if (n.y < 0) n = {-n.x, -n.y, -n.z};
  if (n.y < kFloorUp) return;
  s.dropping.push_back({hit.at, n, t + std::sqrt(2 * hit.t / kGravity)});
}

void land_drops(FxState& s, double t) {
  if (s.dropping.empty()) return;
  ge::core::Rng r = ge::core::make_rng(s.seed++ * 2654435761u + 0xd7u);
  std::erase_if(s.dropping, [&](const FxState::Dropping& d) {
    if (d.land_s > t) return false;
    pool_at(s, d.at, d.n, kDropM * ge::core::range(r, 0.7, 1.3), t, r);
    return true;
  });
}

void tread_blood(const FxScene& l, FxState& s, std::span<const Placed> placed, double t) {
  namespace gd = ge::detail;
  if (!s.receivers) return;
  const gd::Receivers& rec = s.receivers->receivers;
  for (auto it = s.treads.begin(); it != s.treads.end();) {
    const bool there = std::any_of(placed.begin(), placed.end(),
                                   [&](const Placed& m) { return m.entity == it->first; });
    it = there ? std::next(it) : s.treads.erase(it);
  }
  (void)l;
  for (const Placed& m : placed) {
    if (m.entity < 1) continue;
    // Its feet on the floor under it.
    const ge::camera::Vec3 o = engine_point({m.origin.x, m.origin.y, m.origin.z - 12});
    const gd::Hit hit = gd::raycast(rec, {o.x, o.y, o.z}, {0, -1, 0},
                                    (kFeetU + 12) * ge::brush::kUnitM, s.marks.scratch);
    if (!hit.hit) continue;
    const gd::Vec3 feet = hit.at;
    // Standing in a wet pool: its soles take blood.
    bool wet = false;
    for (const gd::Decal& d : s.marks.decals) {
      if (!d.live || d.kind != gd::DecalKind::blood_pool || d.size_m < kStepPoolM ||
          std::fabs(d.at.y - feet.y) > 0.1)
        continue;
      const gd::DecalLook look = gd::look_of(d, t);
      if (look.gloss < kWetGloss || look.opacity < 0.3) continue;
      const double dx = d.at.x - feet.x, dz = d.at.z - feet.z;
      const double edge = kPoolEdge * d.size_m * look.spread;
      if (dx * dx + dz * dz < edge * edge) {
        wet = true;
        break;
      }
    }
    FxState::Tread* tr = nullptr;
    if (wet) {
      tr = &s.treads[m.entity];
      tr->left = kPrints;
      tr->from[0] = feet.x, tr->from[1] = feet.y, tr->from[2] = feet.z;
      continue;
    }
    const auto found = s.treads.find(m.entity);
    if (found == s.treads.end()) continue;
    tr = &found->second;
    if (tr->left <= 0) continue;
    const double dx = feet.x - tr->from[0], dz = feet.z - tr->from[2];
    const double moved = std::sqrt(dx * dx + dz * dz);
    if (moved < kStrideM) continue;
    // A print beside the line it walks, the feet in turn, toe on along it; fainter each step.
    const double fx_ = dx / moved, fz = dz / moved;
    const double side = (tr->right ? 1 : -1) * 0.09;
    gd::DecalAt d;
    d.kind = gd::DecalKind::blood_print;
    d.at = {feet.x - fz * side, feet.y, feet.z + fx_ * side};
    d.normal = {0, 1, 0};
    d.size_m = kPrintM;
    d.spin_rad = ge::core::libm::atan2(fx_, fz) + ge::core::kPi / 2;
    d.variant = (tr->right ? 1u : 0u) + 2u * static_cast<uint32_t>(s.seed++ & 1u);
    d.now_s = t;
    d.life_s = s.on.blood_life_s;
    d.strength = 0.35 + 0.65 * static_cast<double>(tr->left) / kPrints;
    press_mark(s, d);
    tr->right = !tr->right;
    tr->left--;
    tr->from[0] = feet.x, tr->from[1] = feet.y, tr->from[2] = feet.z;
  }
}

}  // namespace shooters_edge::client
