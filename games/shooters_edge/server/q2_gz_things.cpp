// Ground Zero's map things (q2_gz.hpp states the rules; the numbers are the clean-room note's,
// porting/shooters-edge/gz-world.md § 4).
//
// ge:summary Ground Zero's map things: the reactor core, the black light and the orb, the steam
// ge:summary jet, the player killer and the disguise trigger
// ge:keywords q2 ground-zero rogue mission-pack misc-nuke-core target-blacklight target-orb
// ge:keywords target-steam steam-jet target-killplayers trigger-disguise disguise violator
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_things_test.cpp
#include "shooters_edge/server/q2_gz.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "brush/map.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr uint32_t kEveryone = 0xffffffffu;

enum Thing : uint8_t {
  kNoThing,
  kCore,
  kBlackLight,
  kOrb,
  kKillPlayers,
  kSteam,
  kDisguise,
  kLavaStart,
  kAnger,
  kHint
};

Thing thing_of(std::string_view c) {
  if (c == "misc_nuke_core") return kCore;
  if (c == "target_blacklight") return kBlackLight;
  if (c == "target_orb") return kOrb;
  if (c == "target_killplayers") return kKillPlayers;
  if (c == "target_steam") return kSteam;
  if (c == "trigger_disguise") return kDisguise;
  if (c == "info_player_coop_lava") return kLavaStart;
  if (c == "target_anger") return kAnger;
  if (c == "hint_path") return kHint;
  return kNoThing;
}

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

std::string key(const Q2Server& s, const Q2Entity& e, const char* name) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get(name);
}
float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
  const std::string v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

void add_model(Q2Server& s, Q2Entity& e, const char* model) {
  const auto found = std::find(s.models.begin(), s.models.end(), model);
  e.modelindex = static_cast<int>(found - s.models.begin());
  if (found == s.models.end()) s.models.emplace_back(model);
}

sh::Vec3f unpack(const int32_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

// The middle of entity `t` (a brush's box about its origin, a point's origin).
sh::Vec3f middle(const Q2Entity& t) { return t.origin + (t.mins + t.maxs) * 0.5f; }

constexpr const char* kCoreModel = "models/objects/core/tris.md2";
constexpr const char* kSpawnModel = "models/items/spawngro2/tris.md2";
// The figures' effect bits: the dark pulse and its trail; the see-through sphere.
constexpr int kTrackerEffects = (1 << 26) | static_cast<int>(1u << 31);
constexpr int kSphereEffect = 1 << 28;

// A steam jet's direction toward its target's middle, or along its angles without one.
void steam_aim(Q2Server& s, Q2Entity& e) {
  if (e.target.empty()) {
    e.dir = sh::angle_vectors(e.angles).forward;
    return;
  }
  for (const Q2Entity& t : s.entities) {
    if (t.targetname != e.target || t.gone) continue;
    sh::Vec3f d = middle(t) - e.origin;
    sh::normalize(d);
    e.dir = d;
    return;
  }
  s.console.push_back("target_steam at " + q2_pos(e.origin) + ": " + e.target + " is a bad target");
}

// A steam jet's puff (temporary entity 40): the long form when it lasts past 100 ms.
void steam_puff(Q2Server& s, Q2Entity& e) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.time = s.level_time;
  ev.text = "te_steam";
  ev.origin = e.origin;
  const bool lasts = e.wait > 100;
  ev.ent = lasts ? s.gz_steam_id : -1;
  if (lasts) s.gz_steam_id = (s.gz_steam_id + 1) % 20000;
  ev.values[0] = e.dir.x;
  ev.values[1] = e.dir.y;
  ev.values[2] = e.dir.z;
  ev.values[3] = static_cast<float>(e.count);
  ev.level = e.sounds;
  ev.dir = {e.speed, lasts ? e.wait : 0, 0};
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// Whether any living player sees `at` (nothing solid between his eye and it).
bool seen_by_a_player(const Q2Server& s, sh::Vec3f at) {
  for (int p = 0; p < sh::kMaxClients; p++) {
    if (!s.clients[p].active || !s.players[p].alive) continue;
    const Q2Player& pl = s.players[p];
    const sh::Vec3f eye = unpack(pl.move.origin) + sh::Vec3f{0, 0, pl.view_height};
    if (map_trace(s, eye, {}, {}, at, sh::kSqSolid, -1).fraction >= 1) return true;
  }
  return false;
}

}  // namespace

bool gz_thing(std::string_view classname) {
  return thing_of(classname) != kNoThing || classname == "func_plat2";  // the plat: q2_gz_plat.cpp
}

bool gz_thing_spawn(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  if (s.opt.pack != Q2Pack::ground_zero) return false;  // the pack's names only under the pack
  switch (thing_of(e.classname)) {
    case kNoThing:
      return false;
    case kCore:  // shown, a use hides it and the next shows it; passable
      add_model(s, e, kCoreModel);
      e.mins = {-16, -16, -16};
      e.maxs = {16, 16, 16};
      return true;
    case kBlackLight:
    case kOrb:  // single player's: a figure turning at random every tenth
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      add_model(s, e, kSpawnModel);
      e.frame = thing_of(e.classname) == kBlackLight ? 1 : 2;
      e.mins = e.maxs = {};
      return true;
    case kKillPlayers:
      return true;
    case kAnger:  // needs both its monsters and the one thing they turn on
      if (e.target.empty() || key(s, e, "killtarget").empty()) {
        s.console.push_back(e.target.empty() ? "target_anger without target!"
                                             : "target_anger without killtarget!");
        e.gone = true;
      }
      e.killtarget.clear();  // the map's use would remove it: the anger reads it instead
      return true;
    case kHint:  // single player's and co-op's, and only when linked to another
      if (deathmatch(s) || (e.targetname.empty() && e.target.empty())) e.gone = true;
      e.wait = number(s, e, "wait", 0);
      e.mins = {-8, -8, -8};
      e.maxs = {8, 8, 8};
      return true;
    case kLavaStart:  // co-op's
      if (deathmatch(s) || !s.cvars.find("coop")->on()) e.gone = true;
      e.mins = {-16, -16, -24};
      e.maxs = {16, 16, 32};
      return true;
    case kSteam: {
      e.speed = number(s, e, "speed", 75);
      e.count = std::clamp(static_cast<int>(number(s, e, "count", 32)), 0, 255);
      e.sounds = std::clamp(static_cast<int>(number(s, e, "sounds", 8)), 0, 255);
      e.wait = number(s, e, "wait", 0) * 1000;  // ms; 0 set by the first use
      if (e.target.empty())
        steam_aim(s, e);
      else
        e.tick_at = s.level_time + 1;  // its target there by then
      return true;
    }
    case kDisguise:  // a brush field, on only with START_ON (2)
      e.trigger = true;
      e.on = (e.spawnflags & 2) != 0;
      if (e.model.size() > 1 && e.model[0] == '*' && e.modelindex > 0 &&
          size_t(e.modelindex) < s.map->models.size()) {
        const ge::brush::Model& m = s.map->models[size_t(e.modelindex)];
        e.mins = sh::Vec3f{m.mins[0], m.mins[1], m.mins[2]};
        e.maxs = sh::Vec3f{m.maxs[0], m.maxs[1], m.maxs[2]};
      }
      e.modelindex = 0;  // never drawn
      return true;
  }
  return false;
}

bool gz_thing_use(Q2Server& s, int i, int activator, int user) {
  Q2Entity& e = s.entities[size_t(i)];
  if (s.opt.pack != Q2Pack::ground_zero) return false;
  switch (thing_of(e.classname)) {
    case kNoThing:
      return false;
    case kBlackLight:
    case kOrb:
    case kLavaStart:
    case kHint:
      return true;
    case kCore:
      e.hidden = !e.hidden;
      return true;
    case kKillPlayers:  // every player dies, then whatever a player sees
      for (int p = 0; p < sh::kMaxClients; p++)
        if (s.clients[p].active && s.players[p].alive)
          fight_hurt(s, p, -1, {}, 100000, 0, kSqByTelefrag, 1 | 2);
      for (const Q2Monster& m : s.monsters) {
        const Q2Entity& b = s.entities[size_t(m.entity)];
        if (m.dead || b.gone || b.hidden || b.health <= 0) continue;
        if (seen_by_a_player(s, middle(b))) monster_damage(s, m.entity, -1, b.health, {});
      }
      return true;
    case kAnger: {  // its monsters turn on the first thing its killtarget names, at once
      const std::string prey = key(s, e, "killtarget");
      int victim = -1;
      for (size_t t = 0; t < s.entities.size() && victim < 0; t++)
        if (!s.entities[t].gone && s.entities[t].targetname == prey) victim = static_cast<int>(t);
      if (victim < 0) return true;
      s.entities[size_t(victim)].health = 300;  // a thing to fight
      for (Q2Monster& m : s.monsters) {
        const Q2Entity& b = s.entities[size_t(m.entity)];
        if (b.gone || b.targetname != e.target) continue;
        if (b.health < 0) break;  // a dead one ends the use
        m.enemy = -2 - victim;
        m.anger = victim;
        m.seen_at = s.level_time;
        m.last_sight = gz_anger_at(s, victim);
        m.sound_target = m.lost = m.on_trail = false;
        m.attack_finished = s.level_time;
      }
      return true;
    }
    case kSteam:
      if (e.gone) return true;
      if (e.wait <= 0) {  // a first use sets its length: its user's wait (a timer's), or 1 s
        const float user_wait =
            user >= 0 && size_t(user) < s.entities.size() ? s.entities[size_t(user)].wait : 0;
        e.wait = user >= 0 ? user_wait * 1000 : 1000;
      }
      if (!e.target.empty()) steam_aim(s, e);
      steam_puff(s, e);
      (void)activator;
      return true;
    case kDisguise:  // every use turns it on or off (its TOGGLE flag is never read)
      e.on = !e.on;
      return true;
  }
  return false;
}

bool gz_anger_alive(const Q2Server& s, int e) {
  return e >= 0 && size_t(e) < s.entities.size() && !s.entities[size_t(e)].gone &&
         s.entities[size_t(e)].health > 0;
}
sh::Vec3f gz_anger_at(const Q2Server& s, int e) {
  if (e < 0 || size_t(e) >= s.entities.size()) return {};
  const Q2Entity& t = s.entities[size_t(e)];
  return t.origin + (t.mins + t.maxs) * 0.5f;  // a wall's middle, not its origin
}
void gz_anger_box(const Q2Server& s, int e, sh::Vec3f& mins, sh::Vec3f& maxs) {
  maxs = e >= 0 && size_t(e) < s.entities.size()
             ? (s.entities[size_t(e)].maxs - s.entities[size_t(e)].mins) * 0.5f
             : sh::Vec3f{};
  mins = maxs * -1.0f;
}
bool gz_anger_holds(const Q2Server& s, Q2Monster& m) {
  if (m.anger < 0) return false;
  const Q2Entity& b = s.entities[size_t(m.entity)];
  if (m.enemy == -2 - m.anger && gz_anger_alive(s, m.anger) && b.health * 3 > b.max_health)
    return true;
  m.anger = -1;  // spent for good: the monster reacts as ever
  return false;
}
bool gz_anger_point(const Q2Server& s, const Q2Monster& m) {
  if (m.anger < 0 || m.enemy != -2 - m.anger) return false;
  const Q2Entity& t = s.entities[size_t(m.anger)];
  return t.mins.x == t.maxs.x && t.mins.y == t.maxs.y && t.mins.z == t.maxs.z;
}

const Q2Entity* gz_lava_start(const Q2Server& s) {
  if (s.opt.pack != Q2Pack::ground_zero) return nullptr;
  std::string map = s.map_name;
  for (char& c : map) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  if (map != "rmine2" && map != "rmine2p") return nullptr;
  // The lava line: 64 over the highest rising liquid (smart water: a func_door or func_water with
  // flag 2 whose middle is in water, slime or lava).
  bool any = false;
  float top = 0;
  for (const Q2Entity& d : s.entities) {
    if (!gz_rising_liquid(s, d)) continue;
    top = any ? std::max(top, d.origin.z + d.maxs.z) : d.origin.z + d.maxs.z;
    any = true;
  }
  if (!any) return nullptr;
  const float line = top + 64;
  const Q2Entity* best = nullptr;
  int seen = 0;
  for (const Q2Entity& e : s.entities) {
    if (e.gone || e.classname != "info_player_coop_lava") continue;
    if (++seen > 64) break;
    if (e.origin.z < line || (best != nullptr && e.origin.z >= best->origin.z)) continue;
    bool taken = false;
    for (int p = 0; p < sh::kMaxClients && !taken; p++)
      taken = s.clients[p].active && s.players[p].alive &&
              sh::length(unpack(s.players[p].move.origin) - e.origin) < 32;
    if (!taken) best = &e;
  }
  return best;
}

void gz_thing_touch(Q2Server& s, int i, int slot) {
  Q2Entity& e = s.entities[size_t(i)];
  if (thing_of(e.classname) != kDisguise || !e.on) return;
  s.players[slot].disguised = (e.spawnflags & 4) == 0;  // REMOVE (4) takes it away
}

void gz_things_frame(Q2Server& s) {
  for (size_t n = 0; n < s.entities.size(); n++) {
    Q2Entity& e = s.entities[n];
    if (e.gone) continue;
    if (e.classname == "func_water") gz_smart_water_frame(s, e);  // rising after the players
    switch (thing_of(e.classname)) {
      case kBlackLight:
      case kOrb:  // random angles every tenth
        e.angles = {static_cast<float>(ge::core::int_in(s.rng, 0, 359)),
                    static_cast<float>(ge::core::int_in(s.rng, 0, 359)),
                    static_cast<float>(ge::core::int_in(s.rng, 0, 359))};
        break;
      case kSteam:
        if (e.tick_at > 0 && s.level_time + 0.001 >= e.tick_at) {
          e.tick_at = 0;
          steam_aim(s, e);
        }
        break;
      default:
        break;
    }
  }
}

void gz_things_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  for (size_t n = 0; n < s.entities.size(); n++) {
    const Q2Entity& e = s.entities[n];
    const Thing t = thing_of(e.classname);
    if (t != kCore && t != kBlackLight && t != kOrb) continue;
    const size_t row = size_t(map_row(static_cast<int>(n)));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    r.free = e.gone || e.hidden;
    r.origin = e.origin;
    r.angles = e.angles;
    r.modelindex = e.modelindex;
    r.frame = e.frame;
    r.mins = e.mins;
    r.maxs = e.maxs;
    r.effects = t == kBlackLight ? kTrackerEffects : t == kOrb ? kSphereEffect : 0;
  }
}

}  // namespace shooters_edge::server
