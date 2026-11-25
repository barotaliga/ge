// GROUND ZERO'S DOPPLEGANGER: the decoy a deathmatch player sets down from the inventory, run only
// under the pack's switch (s.opt.pack). The facts are the clean-room note's
// (porting/shooters-edge/gz-weapons.md § 15).
//
// - **Used**: a place 48 units ahead of the player (his yaw alone) where the decoy's box fits (or
//   fits dropped from 32 above) over solid, dry, nearly flat ground within 64 below. None: nothing
//   happens and he keeps it, with no word.
// - **The decoy**: a box ±16 across, −24 to 32 high, that falls, takes shots and blasts (30
//   health) and shows through IR goggles; on it, 8 units up, the player's own figure as he looked
//   when he used it (model, skin, weapon), standing through his 40 standing frames at 10 a second
//   and now and then (one frame in ten once it has finished turning, at most once a second)
//   turning 30° a frame to a new yaw between 0 and 350.
// - **Its end**: 30 s on, or at 0 health, it vanishes in a harmless burst (TE 5). Killed after
//   someone other than its owner had hurt it without killing it, it lets out a sphere at that
//   attacker: a hunter when he is more than 768 away, else a vengeance sphere.
//
// One decoy row per player slot is kept from the map's start; a second decoy of the same player
// takes his first one's place.
//
// ge:summary Ground Zero's Doppleganger: the decoy set down, its figure, its end and its spheres
// ge:keywords q2 ground-zero doppleganger decoy item-use hunter-sphere vengeance-sphere
// ge:keywords ir-visible spawn-spot mission-pack
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_decoy_test.cpp
#include <cmath>
#include <string>
#include <string_view>

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr std::string_view kDecoy = "doppleganger";  // a row the rules made, never a map's
constexpr sh::Vec3f kMins{-16, -16, -24}, kMaxs{16, 16, 32};
constexpr float kAhead = 48, kGround = 64, kFigureUp = 8, kTurn = 30, kHunterFar = 768;
constexpr double kLife = 30, kTurnEvery = 1;
constexpr int kHealth = 30, kStandFrames = 40;
constexpr int kIrVisible = 0x8000;

// The fields a decoy keeps on its entity: `owner` its player, `until` its end, `touched_at` its
// last turn, `adest.y` the yaw it turns to, `activator` the last who hurt it and lived to see it
// stand (-1 none), and the look it copied: `modelindex`, `count` the skin, `needs` the weapon
// model, `frame` the level frame it was set down on.
bool pack(const Q2Server& s) { return s.opt.pack == Q2Pack::ground_zero; }

bool is_decoy(const Q2Entity& e) { return e.classname == kDecoy; }

int decoy_of(const Q2Server& s, int slot) {
  for (size_t i = 0; i < s.entities.size(); i++)
    if (is_decoy(s.entities[i]) && s.entities[i].owner == slot) return static_cast<int>(i);
  return -1;
}

void vanish(Q2Server& s, Q2Entity& e) {
  fight_effect(s, "te_explosion1", e.origin);
  e.gone = true;
  e.box = false;
  e.health = 0;
}

// A frame's fall: gravity, then the box swept down to what it lands on.
void fall(Q2Server& s, int i, Q2Entity& e) {
  e.velocity.z -= float(s.cvars.find("sv_gravity")->number) * 0.1f;
  const sh::Vec3f to = e.origin + e.velocity * 0.1f;
  const ge::classic::Q2Trace t =
      map_trace(s, e.origin, e.mins, e.maxs, to, monster_ai::kMonsterMask, i);
  if (t.start_solid || t.all_solid) return;
  e.origin = t.end;
  if (t.fraction < 1) e.velocity = {};
}

}  // namespace

void gz_decoys_reserve(Q2Server& s) {
  if (!pack(s) || !s.cvars.find("deathmatch")->on()) return;
  for (int slot = 0; slot < sh::kMaxClients; slot++) {
    Q2Entity e;
    e.classname = std::string(kDecoy);
    e.owner = slot;
    e.gone = true;
    e.mins = kMins;
    e.maxs = kMaxs;
    s.entities.push_back(e);
  }
}

bool gz_use_decoy(Q2Server& s, int slot, int item) {
  if (!pack(s) || std::string_view(kSqItems[item].classname) != "item_doppleganger") return false;
  const int i = decoy_of(s, slot);
  if (i < 0) return true;  // outside deathmatch: kept, as it was never to be held
  Q2Player& p = s.players[slot];
  const sh::Vec3f ahead =
      monster_ai::unpack(p.move.origin) + monster_ai::forward_of(p.view_angles.y) * kAhead;
  sh::Vec3f at;
  if (!monster_ai::gz_spawn_spot(s, i, kMins, kMaxs, ahead, kGround, at))
    return true;  // no room: kept, with no word
  p.body.counts[item]--;
  Q2Entity& e = s.entities[size_t(i)];
  if (!e.gone) vanish(s, e);  // his last one goes
  const WorldEntity& look = s.world.entities[size_t(1 + slot)];
  e.gone = false;
  e.box = true;
  e.health = kHealth;
  e.origin = at;
  e.velocity = {};
  e.angles = {0, p.view_angles.y, 0};
  e.adest = e.angles;
  e.until = s.level_time + kLife;
  e.touched_at = s.level_time;
  e.activator = -1;
  e.modelindex = look.modelindex;
  e.count = look.skin;
  e.needs = look.carried_model;
  e.frame = static_cast<int>(s.frame_number);
  return true;
}

bool gz_decoy_damage(Q2Server& s, int i, int attacker, int damage) {
  Q2Entity& e = s.entities[size_t(i)];
  if (!is_decoy(e)) return false;
  if (e.gone || damage <= 0) return true;
  e.health -= damage;
  if (e.health > 0) {
    if (attacker >= 0 && attacker < sh::kMaxClients && attacker != e.owner) e.activator = attacker;
    return true;
  }
  const int by = e.activator;
  const sh::Vec3f from = e.origin;
  vanish(s, e);
  if (by >= 0 && s.players[by].alive) {
    const float d = sh::length(monster_ai::unpack(s.players[by].move.origin) - from);
    gz_release_sphere(s, e.owner, by, from, d > kHunterFar);
  }
  return true;
}

void gz_decoys_frame(Q2Server& s) {
  if (!pack(s)) return;
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    if (!is_decoy(e) || e.gone) continue;
    if (s.level_time + 0.001 >= e.until) {
      vanish(s, e);
      continue;
    }
    fall(s, static_cast<int>(i), e);
    float left = e.adest.y - e.angles.y;  // the turn, the short way round, 30° a frame
    left = left - 360 * std::floor((left + 180) / 360);
    if (left != 0) {
      e.angles.y += left > kTurn ? kTurn : left < -kTurn ? -kTurn : left;
    } else if (s.level_time + 0.001 >= e.touched_at + kTurnEvery && monster_ai::random01(s) < 0.1) {
      e.adest.y = float(monster_ai::random01(s) * 350);
      e.touched_at = s.level_time;
    }
  }
}

void gz_decoy_rows(Q2Server& s) {
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& e = s.entities[i];
    if (!is_decoy(e)) continue;
    const size_t row = size_t(map_row(static_cast<int>(i)));
    if (row >= s.world.entities.size()) continue;
    WorldEntity& r = s.world.entities[row];
    r.free = e.gone;
    if (e.gone) continue;
    r.origin = e.origin + sh::Vec3f{0, 0, kFigureUp};
    r.angles = e.angles;
    r.mins = e.mins - sh::Vec3f{0, 0, kFigureUp};
    r.maxs = e.maxs - sh::Vec3f{0, 0, kFigureUp};
    r.absmin = e.origin + e.mins;
    r.absmax = e.origin + e.maxs;
    r.modelindex = e.modelindex;
    r.skin = e.count;
    r.carried_model = e.needs;
    r.frame = static_cast<int>((s.frame_number - e.frame) % kStandFrames);
    r.render_fx = kIrVisible;
    r.health = float(e.health);
  }
}

}  // namespace shooters_edge::server
