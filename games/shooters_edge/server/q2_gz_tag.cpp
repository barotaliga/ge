// Ground Zero's tag game and its random respawns (q2_gz.hpp): `gamerules 2` in deathmatch, the
// token, its holder's scores, quads, look and blows, the token thrown and moved; `randomrespawn`.
// Facts: porting/shooters-edge/q2-packs-dm-facts.md.
//
// ge:summary Ground Zero's tag game (gamerules 2, the token, its scores) and randomrespawn
// ge:keywords q2 ground-zero rogue tag gamerules dm-tag-token tag-token randomrespawn
// ge:keywords mission-pack deathmatch-mode
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_tag_test.cpp
#include <algorithm>
#include <string_view>
#include <vector>

#include "brush/trace.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_powers.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

namespace con = ge::console;
namespace sh = ge::shooter;

sh::Vec3f unpack(const int32_t v[3]) {
  return {static_cast<float>(v[0]) / 8.0f, static_cast<float>(v[1]) / 8.0f,
          static_cast<float>(v[2]) / 8.0f};
}

// A sound from player `slot`, heard by everyone.
void say(Q2Server& s, int slot, const char* sample) {
  fight_sound(s, slot + 1, unpack(s.players[slot].move.origin), sample);
}

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

// The holder's prize as he takes the token (or wins it by a kill): a full heal when he is below
// his maximum, then a body armour as if picked up.
void bonus(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  if (p.health < float(p.body.max_health))
    p.health = std::min(p.health + 200.0f, float(p.body.max_health));
  p.body.health = static_cast<int>(p.health);
  const int armour = sq_item("item_armor_body");
  if (armour >= 0 && sq_pick_up(armour, p.body, true, 0, s.level_time, 0)) {
    p.health = static_cast<float>(p.body.health);
    p.taken.push_back(armour);
    fight_took(s, slot, armour);
    say(s, slot, "misc/ar1_pkup.wav");
  }
}

void hold(Q2Server& s, int slot) {
  s.tag_holder = slot;
  s.tag_kills = 0;
  s.tag_move_at = 0;
  s.tag_moved = false;
}

// The token thrown from the holder (dead or gone): as a dropped item thrown from his view, nobody
// holding it, nobody to take it for 1 s; then moved after 30 s (3 s in lava or slime).
void throw_token(Q2Server& s, int slot) {
  s.tag_holder = -1;
  s.tag_kills = 0;
  const int token = sq_item("dm_tag_token", s.opt.pack);
  Q2Drop* d = token >= 0 ? fight_toss(s, slot, token) : nullptr;
  if (d == nullptr) return;
  d->owner = -1;
  d->dies_at = 0;
  s.tag_move_at = s.level_time + 1;  // looked at once it may be taken
  s.tag_moved = false;
}

// The deathmatch start farthest from every living player (the first, with nobody alive).
const Q2Entity* farthest_start(const Q2Server& s) {
  const Q2Entity* best = nullptr;
  double far = -1;
  for (const Q2Entity& e : s.entities) {
    if (e.classname != "info_player_deathmatch") continue;
    double near = 1e30;
    for (int i = 0; i < sh::kMaxClients; i++) {
      const Q2Player& o = s.players[i];
      if (!s.clients[i].active || !o.alive) continue;
      const sh::Vec3f d = unpack(o.move.origin) - e.origin;
      near = std::min(near, double(d.x) * d.x + double(d.y) * d.y + double(d.z) * d.z);
    }
    if (near > far) {
      far = near;
      best = &e;
    }
  }
  return best;
}

}  // namespace

void gz_tag_cvars(Q2Server& s) {
  s.cvars.add({.name = "gamerules",
               .fallback = "0",
               .flags = con::kServer | con::kLatch,
               .help = "Ground Zero's deathmatch rules: plain deathmatch or tag.",
               .values = "0: deathmatch (the default); 2: tag, where the token's holder scores 3 "
                         "a kill and whoever kills him 5; any other value plays as 0"});
  s.cvars.add({.name = "randomrespawn",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether a deathmatch item comes back as another of its kind.",
               .values = "0: as itself (the default); 1: a weapon as any weapon, ammo as any "
                         "ammo, a powerup as any powerup, health and armour by their odds"});
}

bool gz_tag_on(const Q2Server& s) {
  if (s.opt.pack != Q2Pack::ground_zero || !deathmatch(s)) return false;
  const con::Cvar* c = s.cvars.find("gamerules");
  return c != nullptr && static_cast<int>(c->number) == 2;
}

bool gz_tag_token(int item) {
  return item >= 0 && item < kSqItemCount &&
         std::string_view(kSqItems[item].classname) == "dm_tag_token";
}

bool gz_tag_left_out(const Q2Server& s, int item) { return gz_tag_token(item) && !gz_tag_on(s); }

bool gz_tag_take(Q2Server& s, int slot, int item) {
  if (!gz_tag_token(item)) return false;
  Q2Player& p = s.players[slot];
  if (!p.alive) return true;  // the dead cannot take it
  bonus(s, slot);
  p.body.counts[item]++;
  p.taken.push_back(item);
  fight_took(s, slot, item);  // shown last, so the bar names the token
  say(s, slot, "items/pkup.wav");
  hold(s, slot);
  return true;
}

void gz_tag_start(Q2Server& s) {
  s.tag_holder = -1;
  s.tag_kills = 0;
  s.tag_move_at = 0;
  if (!gz_tag_on(s)) return;
  for (const Q2Entity& e : s.entities)
    if (!e.gone && gz_tag_token(e.item)) return;  // the map placed its own
  const Q2Entity* spot = fight_dmflag(s, 512) ? farthest_start(s) : nullptr;
  if (spot == nullptr) {
    std::vector<const Q2Entity*> spots;
    for (const Q2Entity& e : s.entities)
      if (e.classname == "info_player_deathmatch") spots.push_back(&e);
    if (!spots.empty()) spot = spots[ge::core::pick_index(s.rng, spots.size())];
  }
  if (spot == nullptr)
    for (const Q2Entity& e : s.entities)
      if (e.classname == "info_player_start") {
        spot = &e;
        break;
      }
  if (spot == nullptr) return;
  Q2Drop* d = fight_drop_at(s, spot->origin + sh::Vec3f{0, 0, 9}, "dm_tag_token");
  if (d != nullptr) d->velocity = {};
}

bool gz_tag_kill(Q2Server& s, int slot, int killer, bool by_killer) {
  if (!gz_tag_on(s)) return false;
  const int holder = s.tag_holder;
  bool scored = false;
  if (by_killer && killer == holder && holder >= 0) {
    if (fight_teammates(s, killer, slot)) return false;  // a teamkill scores as ever
    s.players[killer].frags += 3;
    scored = true;
    if (++s.tag_kills >= 5) {  // every fifth kill: a quad used at once
      s.tag_kills = 0;
      Q2Player& k = s.players[killer];
      if (const SqPower* q = sq_power("item_quad"))
        k.*q->until = std::max(k.*q->until, s.level_time) + 30;
      say(s, killer, "items/damage.wav");
    }
  } else if (by_killer && slot == holder && killer >= 0 && killer != slot) {
    s.players[killer].frags += 5;
    scored = true;
  }
  if (slot == holder) {
    if (by_killer && killer >= 0 && killer != slot && s.players[killer].alive) {
      hold(s, killer);  // the killer wins it at once, with the prize; no token thrown
      bonus(s, killer);
    } else {
      throw_token(s, slot);
    }
  }
  return scored;
}

void gz_tag_left(Q2Server& s, int slot) {
  if (gz_tag_on(s) && s.tag_holder == slot) throw_token(s, slot);
}

int gz_tag_damage(const Q2Server& s, int target, int attacker, int damage) {
  const int h = s.tag_holder;
  if (!gz_tag_on(s) || (h >= 0 && (target == h || attacker == h))) return damage;
  return damage * 3 / 4;  // toward zero: 1 point becomes none
}

bool gz_tag_holder(const Q2Server& s, int slot) { return gz_tag_on(s) && s.tag_holder == slot; }

void gz_tag_frame(Q2Server& s) {
  if (!gz_tag_on(s) || s.tag_move_at <= 0 || s.level_time + 0.001 < s.tag_move_at) return;
  const int token = sq_item("dm_tag_token", s.opt.pack);
  Q2Drop* d = nullptr;
  for (Q2Drop& k : s.drops)
    if (k.live && k.item == token) d = &k;
  if (d == nullptr) {
    s.tag_move_at = 0;
    return;
  }
  if (!s.tag_moved && s.tag_move_at > 0 && d->owner == -1 && d->takeable_at <= s.level_time &&
      s.tag_move_at <= d->takeable_at + 0.001) {
    // A second in: in lava or slime it is moved in 3 s, else in 30.
    const float p[3] = {d->origin.x, d->origin.y, d->origin.z};
    const bool bad = s.tracer != nullptr && (s.tracer->contents(p) & (8 | 16)) != 0;
    s.tag_move_at = s.level_time + (bad ? 3 : 30);
    s.tag_moved = true;  // the move is now set, once
    return;
  }
  const Q2Entity* start = farthest_start(s);
  if (start == nullptr) {
    s.tag_move_at = s.level_time + 1;  // no deathmatch start yet: again in a second
    return;
  }
  d->origin = start->origin;
  d->velocity = {};
  d->resting = false;
  s.tag_move_at = 0;
}

bool gz_random_respawn(Q2Server& s, Q2Entity& e) {
  const con::Cvar* c = s.cvars.find("randomrespawn");
  if (c == nullptr || !c->on() || !e.picked || e.item < 0) return false;
  e.picked = false;
  const SqItem& it = kSqItems[e.item];
  const std::string_view cn = it.classname;
  if (cn == "item_power_screen" || cn == "item_power_shield" || cn == "item_health_small" ||
      cn == "item_armor_shard" || cn == "item_bandolier" || cn == "item_pack" ||
      cn == "item_ancient_head")
    return false;
  // The dmflags' own: a mine, the bomb or a sphere that comes back comes back as itself.
  const int flags = s.cvars.find("dmflags")->integer();
  if (((flags & 131072) != 0 && (cn == "ammo_prox" || cn == "ammo_tesla")) ||
      ((flags & 524288) != 0 && cn == "ammo_nuke") ||
      ((flags & 1048576) != 0 && (cn == "item_sphere_vengeance" || cn == "item_sphere_hunter")))
    return false;
  const auto by_odds = [&](std::initializer_list<std::pair<const char*, double>> odds) {
    double roll = ge::core::next(s.rng);
    for (const auto& [name, p] : odds) {
      if (roll < p) return sq_item(name, s.opt.pack);
      roll -= p;
    }
    return sq_item(odds.begin()->first, s.opt.pack);
  };
  int to = -1;
  if (it.kind == SqKind::health || cn == "item_adrenaline") {
    to = by_odds({{"item_health", 0.60},
                  {"item_health_large", 0.30},
                  {"item_adrenaline", 0.09},
                  {"item_health_mega", 0.01}});
  } else if (it.kind == SqKind::armour) {
    to = by_odds(
        {{"item_armor_jacket", 0.60}, {"item_armor_combat", 0.30}, {"item_armor_body", 0.10}});
  } else if (it.kind == SqKind::weapon || it.kind == SqKind::ammo || it.kind == SqKind::powerup ||
             it.kind == SqKind::key) {
    std::vector<int> kin;
    for (int i = 0; i < kSqItemCount; i++) {
      const SqItem& o = kSqItems[i];
      const std::string_view on = o.classname;
      if (o.kind != it.kind || !q2_pack_has(s.opt.pack, o.pack)) continue;
      if (on == "weapon_disintegrator" || on == "ammo_disruptor" || on == "dm_tag_token" ||
          on == "item_bandolier" || on == "item_pack" || on == "item_ancient_head" ||
          on == "item_adrenaline")
        continue;
      kin.push_back(i);
    }
    if (!kin.empty()) to = kin[ge::core::pick_index(s.rng, kin.size())];
  }
  if (to < 0 || to == e.item) return false;
  e.item = to;
  e.classname = kSqItems[to].classname;
  e.modelindex = s.item_models[size_t(to)];
  return true;
}

}  // namespace shooters_edge::server
