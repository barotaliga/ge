// The guns' spent cases each frame (world_fx.hpp's step_cases and held_cases, shells.hpp).
//
// ge:summary The frame's spent cases: the shooters' shots found, thrown, stepped, drawn and heard
// ge:keywords shells spent-case step_cases held_cases case-sound case-model ce_shells
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_shells_test.cpp
#include <algorithm>
#include <cmath>
#include <vector>

#include "brush/map.hpp"
#include "detail/surfaces.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/client/shells.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace {

namespace gd = ge::detail;

void put(const ge::camera::Vec3& v, double out[3]) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
}

// The texture classes of the map's textures, once a map.
void class_textures(Cases& c, const ge::brush::Map* map) {
  if (map == c.classed) return;
  c.classed = map;
  c.texture_class.clear();
  if (map == nullptr) return;
  for (const ge::brush::Texture& t : map->textures)
    c.texture_class.push_back(static_cast<uint8_t>(gd::classify_name(t.name)));
}

// A gun's point from a shooter's origin and yaw (map units: ahead, up) and its way, engine.
void gun_of(const sh::Vec3f& origin, float yaw_deg, float ahead_u, float up_u, double at[3],
            double fwd[3]) {
  const double yaw = yaw_deg * ge::core::kPi / 180;
  const sh::Vec3f gun = {origin.x + float(std::cos(yaw) * ahead_u),
                         origin.y + float(std::sin(yaw) * ahead_u), origin.z + up_u};
  put(engine_point(gun), at);
  const ge::camera::Vec3 f = engine_point({float(std::cos(yaw)), float(std::sin(yaw)), 0});
  const ge::camera::Vec3 o = engine_point({0, 0, 0});
  fwd[0] = f.x - o.x;
  fwd[1] = f.y - o.y;
  fwd[2] = f.z - o.z;
  const double n = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
  for (int k = 0; k < 3; ++k) fwd[k] /= n;
}

constexpr double kRefireS = 0.2;  // a shooter's flash seen again within this is the same shot
constexpr float kPlayerGunU = 18, kPlayerGunUpU = 16;    // another player's gun from his origin
constexpr float kMonsterGunU = 22, kMonsterGunUpU = 10;  // a Quake monster's
constexpr int kFlashBit = 2;                             // an entity's muzzle-flash effect bit

}  // namespace

// THE FRAME'S CASES (world_fx.hpp): the shooters' new shots, the owed cases thrown, the pool
// stepped against the marks' receivers, each case placed to draw and each landing heard.
void step_cases(const FxScene& l, FxState& s, double t, double dt, FxFrame& out) {
  Cases& c = s.cases;
  {
    const PlayerState& me = l.me;
    const ge::camera::Vec3 v = engine_point(me.velocity), o = engine_point({0, 0, 0});
    c.carry[0] = v.x - o.x;
    c.carry[1] = v.y - o.y;
    c.carry[2] = v.z - o.z;
  }
  if (!s.on.shells) {
    c.due.clear();
    c.fired.clear();
    c.effects_seen = l.effects.size();
    if (!c.pool.pieces.empty()) c.pool = {};
    return;
  }
  const double up[3] = {0, 1, 0};
  const double still[3] = {0, 0, 0};
  // A shooter's flash, once a shot.
  const auto fresh = [&](int entity) {
    auto [it, first] = c.fired.try_emplace(entity, t);
    if (first) return true;
    if (t - it->second < kRefireS) return false;
    it->second = t;
    return true;
  };
  if (s.models != nullptr && !l.q2) {
    for (const Placed& m : out.models) {
      if (m.entity <= 0 || (m.effects & kFlashBit) == 0 || m.modelindex <= 0 ||
          m.modelindex >= static_cast<int>(s.models->kinds.size()))
        continue;
      const ModelKind& k = s.models->kinds[size_t(m.modelindex)];
      const bool player = m.entity <= static_cast<int>(sh::kMaxClients);
      if (player && (m.entity == l.own || !l.flash_bit)) continue;  // the own is the hand's
      const Ejection* e = nullptr;
      if (player && k.player && m.frame >= 0 && m.frame < static_cast<int>(k.frame_names.size()))
        e = q1_player_ejection(k.frame_names[size_t(m.frame)]);
      else if (!player)
        e = q1_monster_ejection(k.path);
      if (e == nullptr || !fresh(m.entity)) continue;
      double at[3], fwd[3], port[3];
      gun_of(m.origin, m.angles.y, player ? kPlayerGunU : kMonsterGunU,
             player ? kPlayerGunUpU : kMonsterGunUpU, at, fwd);
      case_port(*e, at, fwd, up, port);
      owe_cases(c, *e, port, fwd, up, false, t);
    }
  }
  // Quake II's flashes (another player's, a monster's): at the gun's point the flash names.
  if (l.effects.size() < c.effects_seen) c.effects_seen = 0;  // a new map
  for (; c.effects_seen < l.effects.size(); c.effects_seen++) {
    const Effect& e = l.effects[c.effects_seen];
    if (e.flash < 0) continue;
    const Ejection* ej = q2_flash_ejection(e.flash, e.flash_monster);
    if (ej == nullptr) continue;
    double at[3], fwd[3], port[3];
    put(engine_point(e.at), at);
    const ge::camera::Vec3 to = engine_point(e.end);
    fwd[0] = to.x - at[0];
    fwd[1] = to.y - at[1];
    fwd[2] = to.z - at[2];
    const double n = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (n < 1e-9) continue;
    for (double& v : fwd) v /= n;
    case_port(*ej, at, fwd, up, port);
    owe_cases(c, *ej, port, fwd, up, false, t);
  }
  // The owed cases due (the held gun's are thrown by held_cases, from the hand as it is then).
  for (const DueCase& d : c.due)
    if (!d.held && d.due <= t) throw_case(c, *d.e, d.port, d.fwd, d.up, still, t);
  std::erase_if(c.due, [&](const DueCase& d) { return !d.held && d.due <= t; });
  if (c.pool.pieces.empty()) return;
  // Flown, bounced and heard.
  const ge::brush::Map* map = l.map ? &l.map->bsp.map : nullptr;
  class_textures(c, map);
  thread_local std::vector<gd::DebrisLanding> landings;
  landings.clear();
  const MarkFaces* faces = s.receivers.get();
  gd::step_debris(c.pool, faces != nullptr ? &faces->receivers : nullptr, s.receiver_scratch, t, dt,
                  &landings, kCaseGravity);
  int heard = 0;
  for (const gd::DebrisLanding& land : landings) {
    if (heard >= kCaseSoundsAFrame) break;
    gd::SurfaceClass on = gd::SurfaceClass::stone;
    if (faces != nullptr && land.tri < faces->mesh.tag.size()) {
      const uint16_t tex = faces->mesh.tag[land.tri];
      if (tex < c.texture_class.size()) on = static_cast<gd::SurfaceClass>(c.texture_class[tex]);
    }
    if (on == gd::SurfaceClass::unknown || on == gd::SurfaceClass::sky)
      on = gd::SurfaceClass::stone;
    DemoSound d;
    d.entity = 0;
    d.channel = 0;
    d.name = case_sound_name(static_cast<Casing>(land.kind), on,
                             static_cast<int>(land.serial * 3u + land.bounce));
    d.volume = static_cast<float>(std::clamp(land.speed / 3.0, 0.08, 1.0));
    d.attenuation = 3;
    d.origin = {static_cast<float>(land.at.x / ge::brush::kUnitM),
                static_cast<float>(land.at.z / ge::brush::kUnitM),
                static_cast<float>(land.at.y / ge::brush::kUnitM)};
    out.sounds.push_back(d);
    heard++;
  }
  // Drawn where they are.
  if (l.effect_models <= 0) return;
  for (size_t i = 0; i < c.pool.pieces.size(); ++i) {
    const gd::Debris& d = c.pool.pieces[i];
    if (d.serial == 0) continue;
    const double shown = gd::debris_shown(c.pool, d, t);
    if (shown <= 0) continue;
    Placed p;
    p.entity = kCaseEntity + static_cast<int>(i);
    p.modelindex = l.effect_models + static_cast<int>(EffectModel::case_hull) + d.kind;
    p.origin = {static_cast<float>(d.at.x / ge::brush::kUnitM),
                static_cast<float>(d.at.z / ge::brush::kUnitM),
                static_cast<float>(d.at.y / ge::brush::kUnitM)};
    p.angles = angles_of_turn(d.turn);
    p.alpha = static_cast<float>(shown);
    p.no_shadow = true;
    out.models.push_back(p);
  }
}

void held_cases(Cases& c, const HeldFire& f, const view::HandFrame& hand, bool on, double t) {
  if (!on) {
    std::erase_if(c.due, [](const DueCase& d) { return d.held; });
    return;
  }
  const auto w = static_cast<size_t>(hand.weapon);
  if (hand.fired && hand.drawn && w < f.ejects.size() && f.ejects[w] != nullptr)
    owe_cases(c, *f.ejects[w], hand.muzzle_at, hand.muzzle_dir, hand.muzzle_up, true, t);
  for (const DueCase& d : c.due) {
    if (!d.held || d.due > t) continue;
    double port[3];
    case_port(*d.e, hand.muzzle_at, hand.muzzle_dir, hand.muzzle_up, port);
    throw_case(c, *d.e, port, hand.muzzle_dir, hand.muzzle_up, c.carry, t);
  }
  std::erase_if(c.due, [&](const DueCase& d) { return d.held && d.due <= t; });
}

void hear_fx(FxFrame& fx, std::vector<DemoSound>& into, std::size_t most) {
  for (const DemoSound& d : fx.sounds)
    if (into.size() < most) into.push_back(d);
  fx.sounds.clear();
}

void hand_fx(FxState& s, HeldFire& f, const view::HandFrame& hand, bool lit, double now_s, double t,
             FxFrame& fx, std::vector<DemoSound>& heard, std::size_t most) {
  if (hand.flashing && lit)
    fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
  fire_held(f, hand, s.on.muzzles, now_s, fx);
  held_cases(s.cases, f, hand, s.on.shells, t);
  hear_fx(fx, heard, most);
}

}  // namespace shooters_edge::client
