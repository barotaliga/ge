// Quake II's gibs (q2_gibs.hpp).
//
// ge:summary Quake II's gibs thrown, moved, rowed and gone
// ge:keywords q2 gibs throw spin toss bounce rows
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gibs_test.cpp
#include "shooters_edge/server/q2_gibs.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>

#include "brush/trace.hpp"
#include "core/prng.hpp"

namespace shooters_edge::server {

namespace {

// The pieces, by their place in `Q2Server::gib_models`.
enum Piece : uint8_t { kMeat, kBone, kChest, kMetal, kGear, kHead, kSkull };
constexpr const char* kPieceModels[kSqGibModels] = {
    "models/objects/gibs/sm_meat/tris.md2", "models/objects/gibs/bone/tris.md2",
    "models/objects/gibs/chest/tris.md2",   "models/objects/gibs/sm_metal/tris.md2",
    "models/objects/gibs/gear/tris.md2",    "models/objects/gibs/head2/tris.md2",
    "models/objects/gibs/skull/tris.md2"};

constexpr int32_t kWorldMask = sh::kSqSolid | sh::kSqWindow;
constexpr int kGibEffect = 2;  // the gib bit: a client draws a blood trail behind it

float uniform(Q2Server& s, float lo, float hi) {
  return static_cast<float>(ge::core::range(s.rng, lo, hi));
}

// The random throw a blow of `damage` gives: across ±100, up 200..300, × 0.7 under 50, else × 1.2.
sh::Vec3f throw_of(Q2Server& s, int damage) {
  const sh::Vec3f v{uniform(s, -100, 100), uniform(s, -100, 100), uniform(s, 200, 300)};
  return v * (damage < 50 ? 0.7f : 1.2f);
}

// One piece of `t` (model piece `piece`), a head spinning about its yaw alone.
void piece(Q2Server& s, const SqGibThrow& t, int which, bool metal, bool head) {
  Q2Gib& g = s.gibs[s.next_gib];
  s.next_gib = (s.next_gib + 1) % kQ2Gibs;
  g = Q2Gib{};
  g.live = true;
  g.model = s.gib_models[which];
  g.metal = metal;
  const sh::Vec3f mid = (t.lo + t.hi) * 0.5f, half = (t.hi - t.lo) * 0.5f;
  g.origin =
      head ? mid
           : sh::Vec3f{mid.x + uniform(s, -half.x, half.x), mid.y + uniform(s, -half.y, half.y),
                       mid.z + uniform(s, -half.z, half.z)};
  sh::Vec3f v = t.velocity + throw_of(s, t.damage) * (metal ? 1.0f : 0.5f);
  v.x = std::clamp(v.x, -300.0f, 300.0f);
  v.y = std::clamp(v.y, -300.0f, 300.0f);
  v.z = std::clamp(v.z, 200.0f, 500.0f);
  g.velocity = v;
  g.spin = head ? sh::Vec3f{0, uniform(s, -600, 600), 0}
                : sh::Vec3f{uniform(s, 0, 600), uniform(s, 0, 600), uniform(s, 0, 600)};
  g.dies_at = s.level_time + double(uniform(s, 10, 20));
}

void pieces(Q2Server& s, const SqGibThrow& t, int which, int count, bool metal) {
  for (int k = 0; k < count; k++) piece(s, t, which, metal, false);
}

}  // namespace

SqGibSet sq_gib_set(std::string_view classname) {
  if (classname == "monster_soldier" || classname == "monster_soldier_light" ||
      classname == "monster_soldier_ss")
    return SqGibSet::soldier;
  if (classname == "monster_flipper" || classname == "monster_hover") return SqGibSet::small;
  if (classname == "monster_tank" || classname == "monster_tank_commander" ||
      classname == "monster_commander_body")
    return SqGibSet::tank;
  if (classname == "monster_makron") return SqGibSet::makron;
  if (classname == "monster_supertank" || classname == "monster_boss2" ||
      classname == "monster_jorg" || classname == "monster_carrier")
    return SqGibSet::wreck;
  return SqGibSet::flesh;
}

void gibs_precache(Q2Server& s) {
  for (int k = 0; k < kSqGibModels; k++) {
    const auto found = std::find(s.models.begin(), s.models.end(), kPieceModels[k]);
    if (found != s.models.end()) {
      s.gib_models[k] = static_cast<int>(found - s.models.begin());
      continue;
    }
    s.models.emplace_back(kPieceModels[k]);
    s.gib_models[k] = static_cast<int>(s.models.size()) - 1;
  }
}

void gibs_throw(Q2Server& s, const SqGibThrow& in) {
  SqGibThrow t = in;
  switch (t.set) {
    case SqGibSet::player:
      pieces(s, t, kMeat, 4, false);
      return;
    case SqGibSet::corpse:
      pieces(s, t, kMeat, 4, false);
      piece(s, t, kHead, false, true);
      return;
    case SqGibSet::flesh:
      pieces(s, t, kBone, 2, false);
      pieces(s, t, kMeat, 4, false);
      piece(s, t, kHead, false, true);
      return;
    case SqGibSet::soldier:
      pieces(s, t, kMeat, 3, false);
      pieces(s, t, kChest, 1, false);
      piece(s, t, kHead, false, true);
      return;
    case SqGibSet::small:
      pieces(s, t, kBone, 2, false);
      pieces(s, t, kMeat, 2, false);
      piece(s, t, kMeat, false, true);
      return;
    case SqGibSet::tank:
    case SqGibSet::makron:
      pieces(s, t, kMeat, 1, false);
      pieces(s, t, kMetal, 4, true);
      if (t.set == SqGibSet::tank) pieces(s, t, kChest, 1, true);
      piece(s, t, kGear, true, true);
      return;
    case SqGibSet::wreck:
      t.damage = 500;
      pieces(s, t, kMeat, 4, false);
      pieces(s, t, kMetal, 8, true);
      pieces(s, t, kChest, 1, true);
      piece(s, t, kGear, true, true);
      return;
  }
}

void gibs_frame(Q2Server& s) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  const float point[3] = {0, 0, 0};
  for (Q2Gib& g : s.gibs) {
    if (!g.live) continue;
    if (s.level_time + 0.001 >= g.dies_at) {
      g.live = false;
      continue;
    }
    if (g.resting) continue;
    q2_hold_speed(s, g.velocity);
    g.velocity.z -= gravity * s.step;
    g.angles = g.angles + g.spin * s.step;
    const sh::Vec3f to = g.origin + g.velocity * s.step;
    const float a[3] = {g.origin.x, g.origin.y, g.origin.z}, b[3] = {to.x, to.y, to.z};
    const ge::brush::Trace t = s.tracer->trace(a, b, point, point, kWorldMask);
    if (t.start_solid || t.all_solid) {  // stuck in the world: it lies where it is
      g.velocity = g.spin = {};
      g.resting = true;
      continue;
    }
    g.origin = {t.end[0], t.end[1], t.end[2]};
    if (t.fraction >= 1) continue;
    // Clipped against what it met: flesh loses its push into the surface, metal bounces half.
    const sh::Vec3f n{t.plane.n[0], t.plane.n[1], t.plane.n[2]};
    const float back = sh::dot(g.velocity, n) * (g.metal ? 1.5f : 1.0f);
    g.velocity = g.velocity - n * back;
    for (float* c : {&g.velocity.x, &g.velocity.y, &g.velocity.z})
      if (*c > -0.1f && *c < 0.1f) *c = 0;
    if (n.z > 0.7f && (!g.metal || g.velocity.z < 60)) {  // a floor: it lies there
      g.velocity = g.spin = {};
      g.resting = true;
    }
  }
}

void gibs_clear(Q2Server& s) {
  for (Q2Gib& g : s.gibs) g = Q2Gib{};
  s.next_gib = 0;
}

void gibs_rows(Q2Server& s, size_t first) {
  RulesWorld& w = s.world;
  if (w.entities.size() < first + kQ2Gibs) return;
  for (int k = 0; k < kQ2Gibs; k++) {
    const Q2Gib& g = s.gibs[k];
    WorldEntity& e = w.entities[first + size_t(k)];
    e = WorldEntity{};
    if (!g.live) continue;
    e.free = false;
    e.origin = g.origin;
    e.velocity = g.velocity;
    e.angles = g.angles;
    e.modelindex = g.model;
    e.skin = g.skin;
    e.effects = g.debris ? 0 : kGibEffect;
  }
}

int gibs_part_model(Q2Server& s, int part) {
  constexpr const char* kParts[3] = {"models/objects/gibs/arm/tris.md2",
                                     "models/objects/gibs/leg/tris.md2",
                                     "models/objects/gibs/head/tris.md2"};
  const std::string name = kParts[std::clamp(part, 0, 2)];
  const auto found = std::find(s.models.begin(), s.models.end(), name);
  if (found != s.models.end()) return static_cast<int>(found - s.models.begin());
  s.models.push_back(name);
  return static_cast<int>(s.models.size()) - 1;
}

void gibs_part(Q2Server& s, int part, sh::Vec3f at, sh::Vec3f angles, sh::Vec3f velocity) {
  Q2Gib& g = s.gibs[s.next_gib];
  s.next_gib = (s.next_gib + 1) % kQ2Gibs;
  g = Q2Gib{};
  g.live = true;
  g.model = gibs_part_model(s, part);
  g.origin = at;
  g.angles = angles;
  g.velocity = velocity;
  g.spin = {uniform(s, 0, 200), uniform(s, 0, 200), uniform(s, 0, 200)};
  g.dies_at = s.level_time + 30;
}

void gibs_debris(Q2Server& s, int model, sh::Vec3f at, sh::Vec3f velocity, float factor) {
  Q2Gib& g = s.gibs[s.next_gib];
  s.next_gib = (s.next_gib + 1) % kQ2Gibs;
  g = Q2Gib{};
  g.live = true;
  g.debris = true;
  g.metal = true;
  g.model = model;
  g.origin = at;
  const sh::Vec3f r{uniform(s, -1, 1), uniform(s, -1, 1), uniform(s, -1, 1)};
  g.velocity = velocity + sh::Vec3f{100 * r.x, 100 * r.y, 100 + 100 * r.z} * factor;
  g.spin = {uniform(s, 0, 600), uniform(s, 0, 600), uniform(s, 0, 600)};
  g.dies_at = s.level_time + 5 + double(uniform(s, 0, 5));
}

sh::Vec3f gibs_head_throw(Q2Server& s, sh::Vec3f velocity, int damage) {
  return velocity + throw_of(s, damage);
}

void gibs_player_head(const Q2Server& s, int slot, int& model, int& skin) {
  const bool skull = s.players[slot].skull;
  model = s.gib_models[skull ? kSkull : kHead];
  skin = skull ? 0 : 1;
}

}  // namespace shooters_edge::server
