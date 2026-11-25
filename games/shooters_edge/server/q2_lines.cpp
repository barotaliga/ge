// Quake II's server console lines about the map (q2_lines.hpp).
//
// ge:summary Quake II's server console lines: a map's faults at load, the inhibited and teams count
// ge:keywords q2 console server-console spawn-warnings inhibited teams picktarget position
// ge:skill shooter
// ge:test tests/shooters_edge_q2_lines_test.cpp
#include "shooters_edge/server/q2_lines.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {
namespace {

// One coordinate: to the nearest whole number, halves to the even one (the default rounding
// mode), `-0` kept for a small negative.
std::string coord(float v) {
  const double r = std::nearbyint(double(v));
  if (r == 0) return std::signbit(v) ? "-0" : "0";
  return std::to_string(static_cast<long long>(r));
}

bool dm(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

void say(Q2Server& s, std::string line) { s.console.push_back(std::move(line)); }

// Every class name the game can spawn (its items, monsters and map entities), in order.
constexpr std::string_view kSpawnable[] = {
    "ammo_bullets",
    "ammo_cells",
    "ammo_grenades",
    "ammo_rockets",
    "ammo_shells",
    "ammo_slugs",
    "func_areaportal",
    "func_button",
    "func_clock",
    "func_conveyor",
    "func_door",
    "func_door_rotating",
    "func_door_secret",
    "func_explosive",
    "func_group",
    "func_killbox",
    "func_object",
    "func_plat",
    "func_rotating",
    "func_timer",
    "func_train",
    "func_wall",
    "func_water",
    "info_notnull",
    "info_null",
    "info_player_coop",
    "info_player_deathmatch",
    "info_player_intermission",
    "info_player_start",
    "item_adrenaline",
    "item_ancient_head",
    "item_armor_body",
    "item_armor_combat",
    "item_armor_jacket",
    "item_armor_shard",
    "item_bandolier",
    "item_breather",
    "item_enviro",
    "item_health",
    "item_health_large",
    "item_health_mega",
    "item_health_small",
    "item_invulnerability",
    "item_pack",
    "item_power_screen",
    "item_power_shield",
    "item_quad",
    "item_silencer",
    "key_airstrike_target",
    "key_blue_key",
    "key_commander_head",
    "key_data_cd",
    "key_data_spinner",
    "key_pass",
    "key_power_cube",
    "key_pyramid",
    "key_red_key",
    "light",
    "light_mine1",
    "light_mine2",
    "misc_actor",
    "misc_banner",
    "misc_bigviper",
    "misc_blackhole",
    "misc_deadsoldier",
    "misc_easterchick",
    "misc_easterchick2",
    "misc_eastertank",
    "misc_explobox",
    "misc_gib_arm",
    "misc_gib_head",
    "misc_gib_leg",
    "misc_insane",
    "misc_satellite_dish",
    "misc_strogg_ship",
    "misc_teleporter",
    "misc_teleporter_dest",
    "misc_viper",
    "misc_viper_bomb",
    "monster_berserk",
    "monster_boss2",
    "monster_boss3_stand",
    "monster_brain",
    "monster_chick",
    "monster_commander_body",
    "monster_flipper",
    "monster_floater",
    "monster_flyer",
    "monster_gladiator",
    "monster_gunner",
    "monster_hover",
    "monster_infantry",
    "monster_jorg",
    "monster_makron",
    "monster_medic",
    "monster_mutant",
    "monster_parasite",
    "monster_soldier",
    "monster_soldier_light",
    "monster_soldier_ss",
    "monster_supertank",
    "monster_tank",
    "monster_tank_commander",
    "path_corner",
    "point_combat",
    "target_actor",
    "target_blaster",
    "target_changelevel",
    "target_character",
    "target_crosslevel_target",
    "target_crosslevel_trigger",
    "target_earthquake",
    "target_explosion",
    "target_goal",
    "target_help",
    "target_laser",
    "target_lightramp",
    "target_secret",
    "target_spawner",
    "target_speaker",
    "target_splash",
    "target_string",
    "target_temp_entity",
    "trigger_always",
    "trigger_counter",
    "trigger_elevator",
    "trigger_gravity",
    "trigger_hurt",
    "trigger_key",
    "trigger_monsterjump",
    "trigger_multiple",
    "trigger_once",
    "trigger_push",
    "trigger_relay",
    "turret_base",
    "turret_breach",
    "turret_driver",
    "viewthing",
    "weapon_bfg",
    "weapon_blaster",
    "weapon_chaingun",
    "weapon_grenadelauncher",
    "weapon_hyperblaster",
    "weapon_machinegun",
    "weapon_railgun",
    "weapon_rocketlauncher",
    "weapon_shotgun",
    "weapon_supershotgun",
    "worldspawn",
};

// Every key an entity may carry (`light` taken and set aside), lower case.
constexpr std::string_view kKeys[] = {
    "accel",    "angle",       "angles",     "attenuation", "classname",   "combattarget",
    "count",    "deathtarget", "decel",      "delay",       "distance",    "dmg",
    "gravity",  "health",      "height",     "item",        "killtarget",  "light",
    "lip",      "map",         "mass",       "maxpitch",    "maxyaw",      "message",
    "minpitch", "minyaw",      "model",      "move_angles", "move_origin", "musictrack",
    "nextmap",  "noise",       "origin",     "pathtarget",  "pausetime",   "random",
    "sky",      "skyaxis",     "skyrotate",  "sounds",      "spawnflags",  "speed",
    "style",    "target",      "targetname", "team",        "volume",      "wait",
};
constexpr std::string_view kVectors[] = {"angles", "move_angles", "move_origin", "origin",
                                         "skyaxis"};

std::string lower(std::string_view k) {
  std::string out(k);
  for (char& ch : out)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return out;
}

// Whether `v` starts with three numbers (what follows them is not read).
bool three_numbers(const std::string& v) {
  const char* at = v.c_str();
  for (int k = 0; k < 3; k++) {
    char* end = nullptr;
    std::strtof(at, &end);
    if (end == at) return false;
    at = end;
  }
  return true;
}

// The lines an entity's keys earn as the map is read, before anything is decided about it: a key
// the game does not know, and a position or angles that are not three numbers. A key starting
// with `_` is the map tools' own and passes unremarked.
void key_lines(Q2Server& s, const ge::brush::Entity& b) {
  for (const auto& [key, value] : b.pairs) {
    if (!key.empty() && key[0] == '_') continue;
    const std::string k = lower(key);
    if (!std::binary_search(std::begin(kKeys), std::end(kKeys), std::string_view(k)))
      say(s, "ED_ParseEdict: " + key + " is not a field");
    else if (std::find(std::begin(kVectors), std::end(kVectors), std::string_view(k)) !=
                 std::end(kVectors) &&
             !three_numbers(value))
      say(s, "ED_ParseField: couldn't parse '" + key + "'");
  }
}

// The Reckoning's own map things (its items and monsters are found by their tables).
constexpr std::string_view kReckoningSpawnable[] = {
    "func_object_repair", "misc_amb4",          "misc_crashviper", "misc_nuke",
    "misc_transport",     "misc_viper_missile", "rotating_light",  "target_mal_laser"};
constexpr std::string_view kGroundZeroSpawnable[] = {"hint_path", "turret_invisible_brain"};

// The game's own, or one the remix brings in from another game (Quake's monsters, its items), or
// the pack's that plays.
bool spawnable(const Q2Server& s, std::string_view c) {
  return std::binary_search(std::begin(kSpawnable), std::end(kSpawnable), c) ||
         monster_class(c, s.opt.pack) || sq_item(c, s.opt.pack) >= 0 ||
         (s.opt.pack == Q2Pack::reckoning &&
          std::find(std::begin(kReckoningSpawnable), std::end(kReckoningSpawnable), c) !=
              std::end(kReckoningSpawnable)) ||
         (s.opt.pack == Q2Pack::ground_zero &&
          (std::find(std::begin(kGroundZeroSpawnable), std::end(kGroundZeroSpawnable), c) !=
               std::end(kGroundZeroSpawnable) ||
           gz_thing(c)));
}

}  // namespace

std::string q2_pos(sh::Vec3f p) {
  return "(" + coord(p.x) + " " + coord(p.y) + " " + coord(p.z) + ")";
}

void q2_pick_missing(Q2Server& s, const std::string& target) {
  say(s, target.empty() ? std::string("G_PickTarget called with NULL targetname")
                        : "G_PickTarget: target " + target + " not found");
}

void q2_pick_missing(Q2Server& s, const std::string& target, const std::string& then) {
  if (target.empty()) return;
  q2_pick_missing(s, target);
  if (!then.empty()) say(s, then);
}

int q2_find_named(const Q2Server& s, const std::string& name, const char* classname) {
  if (name.empty()) return -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].targetname == name && s.entities[i].classname == classname)
      return static_cast<int>(i);
  return -1;
}

bool q2_named(const Q2Server& s, const std::string& name) {
  for (const Q2Entity& e : s.entities)
    if (!e.gone && e.targetname == name) return true;
  return false;
}

void q2_lost_combat(Q2Server& s, Q2Monster& m) {
  const std::string t = std::move(m.lost_combat);
  m.lost_combat.clear();
  if (q2_named(s, t)) return;
  const Q2Entity& e = s.entities[size_t(m.entity)];
  q2_pick_missing(s, t,
                  e.classname + " at " + q2_pos(e.origin) + ", combattarget " + t + " not found");
}

void q2_combat_point_missing(Q2Server& s, int at) {
  Q2Entity& e = s.entities[size_t(at)];
  if (e.target.empty() || q2_named(s, e.target)) return;
  q2_pick_missing(
      s, e.target,
      e.classname + " at " + q2_pos(e.origin) + " target " + e.target + " does not exist");
  e.target.clear();
}

void q2_spawn_lines(Q2Server& s, const std::vector<char>& inhibited) {
  if (s.map == nullptr) return;
  const sh::Vec3f zero{};
  const size_t n = std::min(s.entities.size(), s.map->entities.size() - 1);
  if (!s.map->entities.empty()) key_lines(s, s.map->entities[0]);
  for (size_t i = 0; i < n; i++) {
    key_lines(s, s.map->entities[i + 1]);
    if (i < inhibited.size() && inhibited[i] != 0) continue;
    Q2Entity& e = s.entities[i];
    const ge::brush::Entity& b = s.map->entities[i + 1];
    const auto has = [&b](const char* k) { return !b.get_any_case(k).empty(); };
    const std::string at = q2_pos(e.origin);
    const std::string& c = e.classname;
    if (c.empty() || !spawnable(s, c)) {
      say(s, c.empty() ? std::string("ED_CallSpawn: NULL classname")
                       : c + " doesn't have a spawn function");
      e.gone = true;
    } else if ((monster_class(c, s.opt.pack) || c == "turret_driver") && !dm(s) && has("item") &&
               sq_item(b.get_any_case("item"), s.opt.pack) < 0) {
      say(s, std::string(c)
                 .append(" at ")
                 .append(at)
                 .append(" has bad item: ")
                 .append(b.get_any_case("item")));
    } else if (c == "misc_actor" && !dm(s) && (e.targetname.empty() || e.target.empty())) {
      say(s, e.targetname.empty() ? "untargeted misc_actor at " + at
                                  : "misc_actor with no target at " + at);
      e.gone = true;
    } else if (c == "target_actor" && e.targetname.empty()) {
      say(s, "target_actor with no targetname at " + at);
    } else if (c == "path_corner" && e.targetname.empty()) {
      say(s, "path_corner with no targetname at " + at);
      e.gone = true;
    } else if (c == "viewthing") {
      say(s, "viewthing spawned");
    } else if (c == "func_wall" && (e.spawnflags & 4) != 0 && (e.spawnflags & 2) == 0) {
      say(s, "func_wall START_ON without TOGGLE");
      e.spawnflags |= 2;
    } else if (c == "misc_teleporter" && e.target.empty()) {
      say(s, "teleporter without a target.");
    } else if (c == "func_train" && e.target.empty()) {
      sh::Vec3f lo = e.origin - sh::Vec3f{1, 1, 1};
      if (e.modelindex > 0 && size_t(e.modelindex) < s.map->models.size()) {
        const float* m = s.map->models[size_t(e.modelindex)].mins;
        lo = lo + sh::Vec3f{m[0], m[1], m[2]};
      }
      say(s, "func_train without a target at " + q2_pos(lo));
    } else if (c == "func_timer") {
      const float wait = has("wait") ? std::strtof(b.get_any_case("wait").c_str(), nullptr) : 1;
      const float random = std::strtof(b.get_any_case("random").c_str(), nullptr);
      if (random >= (wait == 0 ? 1.0f : wait))
        say(s, "func_timer at " + at + " has random >= wait");
    } else if (c == "target_speaker" && !has("noise")) {
      say(s, "target_speaker with no noise set at " + at);
    } else if (c == "target_changelevel" && !has("map")) {
      say(s, "target_changelevel with no map at " + at);
    } else if (c == "target_lightramp") {
      const std::string r = b.get_any_case("message");
      const bool bad =
          r.size() != 2 || r[0] < 'a' || r[0] > 'z' || r[1] < 'a' || r[1] > 'z' || r[0] == r[1];
      if (bad)
        say(s, "target_lightramp has bad ramp (" + (has("message") ? r : "(null)") + ") at " + at);
      else if (!dm(s) && e.target.empty())
        say(s, "target_lightramp with no target at " + at);
    } else if (c == "target_earthquake" && e.targetname.empty()) {
      say(s, "untargeted target_earthquake at " + at);
    } else if (c == "trigger_once" && (e.spawnflags & 1) != 0) {
      // Meant as the box's centre, but said before the box is known.
      say(s, "fixed TRIGGERED flag on trigger_once at " + q2_pos(zero));
    } else if (c == "trigger_key") {
      const std::string item = b.get_any_case("item");
      if (item.empty())
        say(s, "no key item for trigger_key at " + at);
      else if (sq_item(item, s.opt.pack) < 0)
        say(s,
            std::string("item ").append(item).append(" not found for trigger_key at ").append(at));
      if (e.target.empty()) say(s, "trigger_key at " + at + " has no target");
    } else if (c == "trigger_gravity" && !has("gravity")) {
      say(s, "trigger_gravity without gravity set at " + at);
    }
  }
}

int q2_ramp_light(Q2Server& s, int i) {
  int light = -1;
  const Q2Entity& e = s.entities[size_t(i)];
  for (size_t t = 0; t < s.entities.size(); t++) {
    const Q2Entity& l = s.entities[t];
    if (l.gone || l.targetname != e.target) continue;
    if (l.classname == "light")
      light = static_cast<int>(t);
    else
      say(s, e.classname + " at " + q2_pos(e.origin) + " target " + e.target + " (" + l.classname +
                 " at " + q2_pos(l.origin) + ") is not a light");
  }
  if (light < 0) {
    say(s, e.classname + " target " + e.target + " not found at " + q2_pos(e.origin));
    s.entities[size_t(i)].gone = true;
  }
  return light;
}

void q2_start_lines(Q2Server& s) {
  for (const Q2Entity& e : s.entities) {  // the elevators set up
    if (e.gone || e.classname != "trigger_elevator") continue;
    if (e.target.empty()) {
      say(s, "trigger_elevator has no target");
      continue;
    }
    int t = -1;
    for (size_t k = 0; k < s.entities.size() && t < 0; k++)
      if (!s.entities[k].gone && s.entities[k].targetname == e.target) t = static_cast<int>(k);
    if (t < 0)
      q2_pick_missing(s, e.target, "trigger_elevator unable to find target " + e.target);
    else if (s.entities[size_t(t)].classname != "func_train")
      say(s, "trigger_elevator target " + e.target + " is not a train");
  }
  for (const Q2Monster& m : s.monsters) {  // the monsters started
    const Q2Entity& e = s.entities[size_t(m.entity)];
    if (e.gone || m.spare || e.block < 0) continue;  // a boss's or a spawner's spare
    const std::string at = e.classname + " at " + q2_pos(e.origin);
    bool point = false, other = false;
    const std::string combattarget = s.map->entities[size_t(e.block)].get_any_case("combattarget");
    for (const Q2Entity& t : s.entities) {
      if (t.gone) continue;
      if (!e.target.empty() && t.targetname == e.target) {
        point = point || t.classname == "point_combat";
        other = other || t.classname != "point_combat";
      }
      if (!combattarget.empty() && t.targetname == combattarget && t.classname != "point_combat")
        say(s, std::string(at)
                   .append(" has a bad combattarget ")
                   .append(combattarget)
                   .append(" : ")
                   .append(t.classname)
                   .append(" at ")
                   .append(q2_pos(t.origin)));
    }
    if (point && other) say(s, at + " has target with mixed types");
    if (!e.target.empty() && !point && !other)
      q2_pick_missing(s, e.target,
                      e.classname + " can't find target " + e.target + " at " + q2_pos(e.origin));
    if (m.start_solid) say(s, e.classname + " in solid at " + q2_pos(e.origin));
  }
}

void q2_count_lines(Q2Server& s, int inhibited) {
  say(s, std::to_string(inhibited) + " entities inhibited");
  std::set<std::string> teams;
  int members = 0;
  for (const Q2Entity& e : s.entities) {
    if (e.gone || e.team.empty() || e.block < 0) continue;
    teams.insert(e.team);
    members++;
  }
  say(s, std::to_string(teams.size()) + " teams with " + std::to_string(members) + " entities");
}

}  // namespace shooters_edge::server
