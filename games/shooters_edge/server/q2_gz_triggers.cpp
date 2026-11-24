// GROUND ZERO'S TRIGGERS, BARREL AND PLATS: the pack's changes to `trigger_push`,
// `trigger_gravity`, `misc_explobox` and `func_plat`, run only under its switch (s.opt.pack). The
// facts are the clean-room note's (porting/shooters-edge/gz-world.md § 6).
//
// - **A pusher's flags**: 2 START_OFF and 4 SILENT. A pusher with a `targetname` is switched on
//   and off by each use (starting off with START_OFF); START_OFF with no `targetname` says so on
//   the console and makes the brush a solid wall that never pushes. SILENT: no wind sound.
// - **A gravity trigger's flags**: 1 TOGGLE and 2 START_OFF (which implies it): each use switches
//   it on and off. Its `gravity` is a fraction (1 the level's gravity), and it now reaches
//   players: a player's move uses the level gravity times the factor his last frame's touch set,
//   back to 1 the frame he is out of it (or dead, or flying free).
// - **A barrel in liquid**: lava burns it 10 a level every 0.2 s (so it goes off, 0.2 s later);
//   slime and drowning never harm it; it splashes going in and out.
// - **A plat**: a walker blocked on its way to an enemy wholly above or below uses the plat it
//   stands on (or the first one under its step, within 384) when that carries it toward him:
//   up from the bottom, down at once from the top. A dead body in its way takes 100 crush first.
//   While it rises a bad area 64 high over its raised place keeps monsters from stepping onto it
//   until it is back at the bottom (one inside moves freely).
//
// ge:summary Ground Zero's pushers, gravity triggers, barrels in lava and plats monsters use
// ge:keywords q2 ground-zero trigger-push start-off silent trigger-gravity toggle misc-explobox
// ge:keywords barrel-lava func-plat plat-bad-area monster-rides-plat mission-pack
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_triggers_test.cpp
#include <cstdlib>
#include <string>

#include "brush/map.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_map_kinds.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"

namespace shooters_edge::server {

namespace {

constexpr int kStartOff = 2, kSilent = 4, kToggle = 1;
constexpr float kPlatArea = 64;  // the bad area's height over a rising plat
constexpr float kPlatReach = 384;

bool pack(const Q2Server& s) { return s.opt.pack == Q2Pack::ground_zero; }

bool is_gravity(const Q2Entity& e) { return e.classname == "trigger_gravity"; }

bool overlap(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y && alo.z < bhi.z &&
         ahi.z > blo.z;
}

bool is_plat(const Q2Entity& e) { return e.classname.rfind("func_pla", 0) == 0; }

}  // namespace

// ---- Pushers ----

void gz_push_spawn(Q2Server& s, Q2Entity& e) {
  if (!pack(s) || (e.spawnflags & kStartOff) == 0) return;
  e.trigger = false;
  if (!e.targetname.empty()) return;  // off until its first use
  s.console.emplace_back("trigger_push is START_OFF but not targeted.");
  e.solid = true;  // a wall that never pushes
}

bool gz_push_use(Q2Server& s, Q2Entity& e) {
  if (!pack(s) || e.classname != "trigger_push" || e.targetname.empty() || e.solid) return false;
  e.trigger = !e.trigger;
  return true;
}

bool gz_push_silent(const Q2Server& s, const Q2Entity& e) {
  return pack(s) && (e.spawnflags & kSilent) != 0;
}

// ---- Gravity ----

void gz_gravity_spawn(const Q2Server& s, Q2Entity& e) {
  if (!pack(s) || e.gone || e.block < 0 || size_t(e.block) >= s.map->entities.size()) return;
  const std::string& g = s.map->entities[size_t(e.block)].get_any_case("gravity");
  e.random = std::strtof(g.c_str(), nullptr);  // a fraction now
  e.hidden = (e.spawnflags & kStartOff) != 0;  // off: nothing touches it
}

bool gz_gravity_use(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  if (!is_gravity(e)) return false;
  if (pack(s) && (e.spawnflags & (kToggle | kStartOff)) != 0) e.hidden = !e.hidden;
  return true;  // without the flags (and outside the pack) a use does nothing
}

void gz_gravity_touch(Q2Server& s, int slot) {
  if (!pack(s)) return;
  Q2Player& p = s.players[size_t(slot)];
  p.gravity = 1;
  if (!p.alive || p.noclip) return;
  const sh::Vec3f at =
      sh::Vec3f{float(p.move.origin[0]), float(p.move.origin[1]), float(p.move.origin[2])} * 0.125f;
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  for (const Q2Entity& e : s.entities)
    if (!e.gone && !e.hidden && is_gravity(e) &&
        overlap(lo, hi, e.origin + e.mins, e.origin + e.maxs))
      p.gravity = e.random;
}

// ---- The barrel in liquid ----

void gz_barrel_liquids(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  if (!pack(s) || e.health <= 0) return;
  sh::Vec3f p = e.origin + sh::Vec3f{0, 0, e.mins.z + 1};
  const int type = map_contents(s, p) & sh::kSqMaskWater;
  int level = 0;
  if (type != 0) {
    level = 1;
    p.z += 26;
    if ((map_contents(s, p) & sh::kSqMaskWater) != 0) {
      level = 2;
      p.z += 22;
      if ((map_contents(s, p) & sh::kSqMaskWater) != 0) level = 3;
    }
  }
  const bool was_in = e.state != 0;
  if (level == 0) {
    if (was_in) monster_ai::sound(s, i, "player/watr_out.wav");
    e.state = 0;
    return;
  }
  // Lava's burn comes before the splash, which clears its timer: the first burn on this frame.
  if ((type & sh::kSqLava) != 0 && s.level_time + 0.001 >= e.touched_at) {
    e.touched_at = s.level_time + 0.2;
    map_damage(s, i, -1, 10 * level);
  }
  if (!was_in) {
    const bool lava = (type & sh::kSqLava) != 0;
    monster_ai::sound(
        s, i,
        lava ? (monster_ai::random01(s) < 0.5 ? "player/lava1.wav" : "player/lava2.wav")
             : "player/watr_in.wav");
    e.touched_at = 0;
    e.state = 1;
  }
}

// ---- Plats ----

void gz_plat_blocked(Q2Server& s, const Q2Entity& plat, int slot) {
  if (!pack(s) || q2map::class_of_name(plat.classname) != q2map::kPlat) return;
  if (slot >= 0) {
    if (s.players[size_t(slot)].health < 1) fight_hurt(s, slot, -1, {}, 100, 1, kSqByCrush);
    return;
  }
  const int mi = monster_index(s, -2 - slot);
  if (mi >= 0 && s.entities[size_t(-2 - slot)].health < 1)
    monster_damage(s, -2 - slot, -1, 100, {});
}

void gz_plat_areas(Q2Server& s) {
  if (!pack(s)) return;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& e = s.entities[i];
    if (e.gone || q2map::class_of_name(e.classname) != q2map::kPlat) continue;
    const int at = static_cast<int>(i);
    const bool rising = e.state == q2map::kGoing && e.arrive != q2map::kNoAct;
    if (e.state == q2map::kAtFirst)  // at the bottom: its areas go
      std::erase_if(s.bad_areas, [&](const Q2BadArea& a) { return a.plat == at; });
    if (!rising) continue;
    bool has = false;
    for (const Q2BadArea& a : s.bad_areas) has = has || a.plat == at;
    if (has) continue;
    // The raised plat's footprint, from its foot to 64 over it, fixed in the world.
    const sh::Vec3f lo = e.closed + e.mins;
    const sh::Vec3f hi{e.closed.x + e.maxs.x, e.closed.y + e.maxs.y, lo.z + kPlatArea};
    s.bad_areas.push_back({lo, hi, {}, 1e30, at});
  }
}

bool gz_plat_ride(Q2Server& s, Q2Monster& m, sh::Vec3f move) {
  if (!pack(s) || m.enemy < 0 || m.dead) return false;
  const Q2Entity& b = s.entities[size_t(m.entity)];
  sh::Vec3f fmin, fmax;
  monster_ai::foe_box(s, m.enemy, fmin, fmax);
  const sh::Vec3f foe = monster_ai::foe_at(s, m.enemy);
  const bool above = foe.z + fmin.z > b.origin.z + b.maxs.z;
  const bool below = foe.z + fmax.z < b.origin.z + b.mins.z;
  if (!above && !below) return false;
  // The plat it stands on, or the first thing under its step.
  int plat = -1;
  bool on = false;
  const auto under = [&](sh::Vec3f from, float reach) {
    const ge::classic::Q2Trace t =
        map_trace(s, from, b.mins, b.maxs, from - sh::Vec3f{0, 0, reach},
                  sh::kSqSolid | sh::kSqWindow | sh::kSqMonster, m.entity);
    const int hit = t.fraction < 1 ? map_entity_of_row(s, t.entity) : -1;
    return hit >= 0 && is_plat(s.entities[size_t(hit)]) ? hit : -1;
  };
  if (m.on_ground) plat = under(b.origin, 1);
  on = plat >= 0;
  if (!on) plat = under(b.origin + move, kPlatReach);
  if (plat < 0) return false;
  Q2Entity& p = s.entities[size_t(plat)];
  const bool bottom = p.state == q2map::kAtFirst, top = p.state == q2map::kAtSecond;
  const bool go = above ? (on ? bottom : top) : (on ? top : bottom);
  if (!go) return false;
  p.think = bottom ? q2map::kPlatUp : q2map::kPlatDown;  // at once, on its next think
  p.think_at = s.level_time + 0.001;
  return true;
}

}  // namespace shooters_edge::server
