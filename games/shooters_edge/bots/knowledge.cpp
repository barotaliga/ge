// What a fighter knows of the map's items (knowledge.hpp): sight of their spots, the beliefs it
// keeps, the timing by how well it knows the map, and the novice's walk to learn it.
//
// ge:summary A fighter's item knowledge: sight of each spot, beliefs, timing, recall, exploring
// ge:keywords knowledge_init knowledge_update item_in_mind explore_node map_sense_of item_call
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/knowledge.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "shooter/senses.hpp"

namespace shooters_edge::bots {

namespace {

constexpr int32_t kWRockets = 32;   // the rocket launcher's W_ bit (games/shooters_edge/qc)
constexpr float kOwnPickup = 64;    // an item gone this near the player is its own pickup, units
constexpr float kExploreFar = 600;  // a novice explores to a node at least this far, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

}  // namespace

bool parse_vec(const char* text, sh::Vec3f& out) {
  if (!text) return false;
  char* end = nullptr;
  out.x = std::strtof(text, &end);
  out.y = std::strtof(end, &end);
  out.z = std::strtof(end, &end);
  return true;
}

// The map's item from its classname, or false.
bool item_of(const char* cn, int spawnflags, ItemSpot& it) {
  const bool twice = (spawnflags & 1) != 0;
  struct Ammo4 {
    const char* cn;
    ItemKind kind;
    float amount;
  };
  static const Ammo4 kAmmo[4] = {{"item_shells", ItemKind::shells, 20},
                                 {"item_spikes", ItemKind::nails, 25},
                                 {"item_rockets", ItemKind::rockets, 5},
                                 {"item_cells", ItemKind::cells, 6}};
  for (const Ammo4& a : kAmmo)
    if (std::strcmp(cn, a.cn) == 0) {
      it.kind = a.kind;
      it.amount = a.amount * (twice ? 2.f : 1.f);
      it.name = a.cn;
      return true;
    }
  if (std::strcmp(cn, "item_artifact_invulnerability") == 0) {
    it.kind = ItemKind::pent;
    it.respawn_s = 60;
    it.name = "pent";
    return true;
  }
  if (std::strcmp(cn, "item_artifact_super_damage") == 0) {
    it.kind = ItemKind::quad;
    it.respawn_s = 60;
    it.name = "quad";
    return true;
  }
  if (std::strcmp(cn, "item_health") == 0) {
    it.kind = (spawnflags & 2) ? ItemKind::mega : ItemKind::health;
    it.amount = (spawnflags & 2) ? 100.f : (spawnflags & 1) ? 15.f : 25.f;
    it.name = (spawnflags & 2) ? "mega health" : "health";
    return true;
  }
  if (std::strcmp(cn, "item_armor1") == 0)
    it.kind = ItemKind::armor1;
  else if (std::strcmp(cn, "item_armor2") == 0)
    it.kind = ItemKind::armor2;
  else if (std::strcmp(cn, "item_armorInv") == 0)
    it.kind = ItemKind::armor3;
  if (std::strncmp(cn, "item_armor", 10) == 0) {
    it.name = cn;
    return true;
  }
  struct Gun {
    const char* cn;
    int32_t bit;
  };
  static const Gun kGuns[6] = {{"weapon_supershotgun", 2},    {"weapon_nailgun", 4},
                               {"weapon_supernailgun", 8},    {"weapon_grenadelauncher", 16},
                               {"weapon_rocketlauncher", 32}, {"weapon_lightning", 64}};
  for (const Gun& g : kGuns)
    if (std::strcmp(cn, g.cn) == 0) {
      it.kind = ItemKind::weapon;
      it.weapon = g.bit;
      it.respawn_s = 30;
      it.name = cn;
      return true;
    }
  return false;
}

// The callout for an enemy at `there`: by the landmark nearest it that the player knows (a key item
// or a weapon: "enemy at RA", "enemy at RL"), the key items counted nearer; empty with none near.
std::string enemy_call(const Fighter& f, sh::Vec3f there) {
  int32_t best = -1;
  float best_d = kCallNear;
  for (size_t i = 0; i < f.items.size(); i++) {
    const ItemSpot& it = f.items[i];
    if (!it.known || (!key_item(it) && it.kind != ItemKind::weapon)) continue;
    const float d = dist(it.at, there) * (key_item(it) ? 0.5f : 1.f);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int32_t>(i);
    }
  }
  return best < 0 ? std::string()
                  : std::string("enemy at ") + item_call(f.items[static_cast<size_t>(best)]);
}

MapSense map_sense_of(sh::AimSkill skill) {
  switch (skill) {
    case sh::AimSkill::novice:
      return MapSense::novice;
    case sh::AimSkill::average:
      return MapSense::average;
    default:
      return MapSense::expert;
  }
}

const char* map_sense_name(MapSense s) {
  switch (s) {
    case MapSense::novice:
      return "novice";
    case MapSense::average:
      return "average";
    case MapSense::expert:
      break;
  }
  return "expert";
}

bool key_item(const ItemSpot& it) {
  return it.kind == ItemKind::armor2 || it.kind == ItemKind::armor3 || it.kind == ItemKind::mega ||
         it.kind == ItemKind::quad || it.kind == ItemKind::pent ||
         (it.kind == ItemKind::weapon && it.weapon == kWRockets);
}

const char* item_call(const ItemSpot& it) {
  switch (it.kind) {
    case ItemKind::health:
      return "health";
    case ItemKind::mega:
      return "mh";
    case ItemKind::armor1:
      return "ga";
    case ItemKind::armor2:
      return "ya";
    case ItemKind::armor3:
      return "ra";
    case ItemKind::shells:
      return "shells";
    case ItemKind::nails:
      return "nails";
    case ItemKind::rockets:
      return "rox";
    case ItemKind::cells:
      return "cells";
    case ItemKind::quad:
      return "quad";
    case ItemKind::pent:
      return "pent";
    case ItemKind::weapon:
      break;
  }
  switch (it.weapon) {
    case 2:
      return "ssg";
    case 4:
      return "ng";
    case 8:
      return "sng";
    case 16:
      return "gl";
    case 32:
      return "rl";
    default:
      return "lg";
  }
}

void knowledge_init(Fighter& f) {
  for (ItemSpot& it : f.items) {
    it.present = true;  // every item stands at the match's start, as every player knows
    it.confirmed = true;
    it.timed = false;
    it.known = f.sense != MapSense::novice;
    it.sightings = 0;
    it.last_seen = -1e9;
    it.in_sight = it.watched = false;
  }
  f.explore_node = -1;
}

ItemNews knowledge_update(Fighter& f, const server::QcServer& s, int me, sh::Vec3f eye, double t) {
  ItemNews news;
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  sh::SenseSettings look = f.bot.senses.set;
  look.range = kItemSight;
  const sh::Vec3f angles{f.bot.pitch, f.bot.yaw, 0};
  for (size_t i = 0; i < f.items.size(); i++) {
    ItemSpot& it = f.items[i];
    if (it.ent < 0) continue;
    bool sent = false;
    for (const sh::ViewEntity& v : f.bot.view.entities) sent |= v.ent == it.ent;
    // The spot in sight (as a player's middle, a little over the floor), or underfoot.
    const bool underfoot = dist(it.at, self) < kOwnPickup;
    const sh::Vec3f spot{it.at.x, it.at.y, it.at.z + 8};
    const bool looks = underfoot || sh::sees_player(s.sv, look, me, eye, angles,
                                                    {spot.x, spot.y, spot.z - look.view_height});
    it.in_sight = looks;
    if (looks && sent) {
      if (t - it.last_seen >= kSightingGap_s) it.sightings++;
      it.last_seen = t;
      it.present = it.confirmed = true;
      it.timed = false;
      if (!it.known && it.sightings >= kLearnSightings) {
        it.known = true;
        news.learned = static_cast<int32_t>(i);
      }
    } else if (looks && it.present) {
      // Gone from its spot in sight: taken now (seen going, or its own pickup), or found gone.
      // Seen going only when it was there at the last look (one found gone is not timed).
      const bool saw_it_go = it.watched;
      const bool times =
          f.sense == MapSense::expert || (f.sense == MapSense::average && !key_item(it));
      it.present = it.confirmed = false;
      it.timed = saw_it_go && times;
      it.back_at = t + it.respawn_s * (it.timed ? 1.f : kGuessLate);
      f.stats.taken++;
      news.taken = static_cast<int32_t>(i);
      news.by_me = underfoot && saw_it_go;
      if (news.by_me) f.stats.pickups++;
    } else if (!it.present && t >= it.back_at) {
      // Out of sight and its time come (or the guess's): believed back, not yet seen back.
      it.present = true;
      it.confirmed = it.timed;
      it.timed = false;
    }
    it.watched = looks && sent;
  }
  return news;
}

bool item_in_mind(const Fighter& f, int32_t i) {
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  return it.known || it.in_sight;
}

int32_t items_known(const Fighter& f) {
  int32_t n = 0;
  for (const ItemSpot& it : f.items) n += it.known ? 1 : 0;
  return n;
}

int32_t explore_node(Fighter& f, sh::Vec3f me) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  const auto ok = [&](int32_t n) {
    return n >= 0 && static_cast<size_t>(n) < g.nodes.size() &&
           (f.reach_at < 0 || f.reach.reached(n));
  };
  if (ok(f.explore_node) && dist(g.nodes[static_cast<size_t>(f.explore_node)].origin, me) > 96)
    return f.explore_node;
  // A new far node: draws off the fighter's own stream until one is reachable and far enough.
  const auto n = static_cast<uint32_t>(g.nodes.size());
  if (n == 0) return -1;
  for (int k = 0; k < 64; k++) {
    f.rng = f.rng * 1664525u + 1013904223u;
    const auto c = static_cast<int32_t>((f.rng >> 8) % n);
    if (ok(c) && dist(g.nodes[static_cast<size_t>(c)].origin, me) >= kExploreFar) {
      f.explore_node = c;
      return c;
    }
  }
  f.explore_node = -1;
  return -1;
}

}  // namespace shooters_edge::bots
