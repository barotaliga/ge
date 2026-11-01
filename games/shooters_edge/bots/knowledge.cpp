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
#include <string>

#include "shooter/senses.hpp"
#include "shooters_edge/profiles/items.hpp"

namespace shooters_edge::bots {

namespace pf = shooters_edge::profiles;

namespace {

constexpr int32_t kWRockets = 32;  // the rocket launcher's W_ bit (games/shooters_edge/qc)
constexpr int32_t kWGrenades = 16, kWArc = 64;  // the grenade launcher's and the arc's
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

// The map's item from its classname, by the game profile's rows (profiles/items.hpp), or false.
bool item_of(const char* cn, int spawnflags, ItemSpot& it) {
  const pf::ItemRow* r = pf::item_row(pf::kClassicTables.items, cn, spawnflags);
  if (r == nullptr) return false;
  if (r->sets_kind) it.kind = r->kind;
  if (r->amount >= 0) it.amount = r->amount * (r->doubles && (spawnflags & 1) != 0 ? 2.f : 1.f);
  if (r->respawn_s >= 0) it.respawn_s = r->respawn_s;
  if (r->weapon != 0) it.weapon = r->weapon;
  it.name = r->name.empty() ? std::string(cn) : std::string(r->name);
  return true;
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
  const pf::ItemWords* w = pf::item_words(pf::kClassicTables.words, it.kind, it.weapon);
  return w != nullptr ? w->call.data() : "";
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
  map_weights(f);
}

void mega_taken(Fighter& f, int32_t i, double t, bool by_me) {
  ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (it.kind != ItemKind::mega) return;
  // Its clock starts when its taker is back down to 100: nobody else can know when that is.
  it.timed = false;
  it.back_at = t + kMegaGuess_s;
  if (by_me && f.sense == MapSense::expert) f.mega_watch = i;
}

void mega_watch(Fighter& f, float health, double t) {
  if (f.mega_watch < 0) return;
  ItemSpot& it = f.items[static_cast<size_t>(f.mega_watch)];
  if (health > 100) {
    it.back_at = std::fmax(it.back_at, t + it.respawn_s + 1);  // not before its clock starts
    return;
  }
  it.back_at = t + it.respawn_s;  // down to 100 (or dead): back after its own wait, to the second
  it.timed = true;
  f.mega_watch = -1;
}

void map_weights(Fighter& f) {
  for (ItemSpot& it : f.items) it.weight = 1;
  if (f.sense != MapSense::expert) return;  // the rest rate an item the same on every map
  int n[13] = {};
  int launchers = 0;
  bool lg = false;
  for (const ItemSpot& it : f.items) {
    n[static_cast<int>(it.kind)]++;
    launchers += it.kind == ItemKind::weapon && it.weapon == kWRockets;
    lg |= it.kind == ItemKind::weapon && it.weapon == kWArc;
  }
  const bool rl = launchers > 0;
  const int ra = n[static_cast<int>(ItemKind::armor3)], ya = n[static_cast<int>(ItemKind::armor2)];
  for (ItemSpot& it : f.items) {
    const int same = n[static_cast<int>(it.kind)];
    switch (it.kind) {
      case ItemKind::armor3:
        it.weight = same == 1 ? 1.4f : 1.f;  // the one 200 on the map: whoever holds it wins
        break;
      case ItemKind::armor2:
        if (ra == 0) it.weight = same == 1 ? 1.6f : 1.3f;  // the best armour there is
        break;
      case ItemKind::mega:
        if (ra + ya == 0) it.weight = 1.5f;  // no big armour: the mega is the stack
        break;
      case ItemKind::weapon:
        if (!rl && it.weapon == kWGrenades) it.weight = 2.f;  // the splash there is
        if (!rl && it.weapon == kWArc) it.weight = 1.4f;
        if (it.weapon == kWRockets) it.weight = (lg ? 1.f : 1.2f) * (launchers == 1 ? 1.2f : 1.f);
        break;
      default:
        break;
    }
  }
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
      mega_taken(f, static_cast<int32_t>(i), t, news.by_me);
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
  if (g.nodes.empty()) return -1;
  for (int k = 0; k < 64; k++) {
    const auto c = static_cast<int32_t>(ge::core::pick_index(f.rng, g.nodes.size()));
    if (ok(c) && dist(g.nodes[static_cast<size_t>(c)].origin, me) >= kExploreFar) {
      f.explore_node = c;
      return c;
    }
  }
  f.explore_node = -1;
  return -1;
}

}  // namespace shooters_edge::bots
