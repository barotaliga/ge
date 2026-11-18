// THE BLOOD IN THE MODERN LOOK (ce_blood, client/world_fx.hpp § The blood): a body struck sprays
// blood shaped by what struck it (app/weapon_fx.hpp's `blood_spray`) and paints the walls and the
// floor near it (engine/detail's blood decals, pressed on the marks' faces, drying and fading).
//
// The hit's message carries only its place and its count, so the blow is worked out from what the
// effects see: the arc gun's blood is its own message; a nail that flew within reach of the wound
// a frame ago drove it (along that nail's flight); else the player whose aim runs through the
// wound fired it (the viewer's own eye and aim first, then each other player's facing): a blade's
// cut when he stands within arm's reach, a pellet's otherwise; nobody: a pellet's, from the eye. A
// blast that goes off near another player bleeds him too (the classic game sends no blood for it);
// the viewer's own body does not spray into his eyes.
//
// ge:summary The modern look's blood: a spray by the weapon that struck, splashes walls and floor
// ge:keywords blood ce_blood gore splatter spray decal wound by-weapon blast-blood bleed_fx
// ge:keywords blood_blow splash floor-pool
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
    case fx::BloodBy::any:
    case fx::BloodBy::pellet:
      break;
  }
  return {1, 0.5, 3.0, 0.24, 0.18};
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
                         double size) {
    const ge::detail::Hit hit = ge::detail::raycast(rec, from, d, reach, s.marks.scratch);
    if (!hit.hit) return;
    ge::detail::Vec3 n = ge::detail::normal_of(*rec.mesh, hit.tri);
    if (n.x * d.x + n.y * d.y + n.z * d.z > 0) n = {-n.x, -n.y, -n.z};
    ge::detail::DecalAt m;
    m.kind = ge::detail::DecalKind::blood;
    m.at = hit.at;
    m.normal = n;
    // Further off, a spatter spreads wider.
    m.size_m = size * (0.7 + 0.5 * hit.t / std::max(reach, 0.1)) * ge::core::range(r, 0.7, 1.1);
    m.spin_rad = ge::core::range(r, 0, ge::core::kTau);
    m.variant = static_cast<uint32_t>(ge::core::int_in(r, 0, 3));
    m.now_s = t;
    press_mark(s, m);
  };
  const ge::detail::Vec3 from{p[0], p[1], p[2]};
  const int spatters = std::min(k.spatters * std::clamp(count, 1, 4), 10);
  for (int i = 0; i < spatters; ++i) {
    double d[3];
    for (int j = 0; j < 3; ++j) d[j] = axis[j] + k.cone * ge::core::range(r, -1, 1);
    d[1] -= 0.15;  // it falls as it flies
    const double l = len3(d);
    if (l < 1e-6) continue;
    paint(from, {d[0] / l, d[1] / l, d[2] / l}, k.reach_m, k.size_m);
  }
  if (k.pool_m > 0)
    paint(from, {ge::core::range(r, -0.15, 0.15), -1, ge::core::range(r, -0.15, 0.15)}, 2.5,
          k.pool_m * (1 + 0.2 * (std::clamp(count, 1, 6) - 1)));
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
  light_at(s, p, h.light);
  fx::blood_spray(h, s.seed++, &out.births);
  splatter(s, b.by, p, b.dir, h.count, t);
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
    light_at(s, h.at, h.light);
    for (int i = 0; i < h.count; ++i) fx::blood_spray(h, s.seed++, &out.births);
    splatter(s, fx::BloodBy::blast, h.at, h.dir, h.count, t);
  }
}

}  // namespace shooters_edge::client
