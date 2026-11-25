// Tests: Quake II's items on its runner (games/shooters_edge/server/q2_server.hpp): an item
// is dropped onto the floor at the map's start, one that starts in a solid is removed, one flagged
// out of deathmatch is not spawned there; a player walking into items takes them by their rule
// (the HUD's numbers follow), a taken item hides and shows again after its respawn time in
// deathmatch, and outside deathmatch it is gone for good.
//
// ge:summary Tests: Quake II's items on the runner: dropped, left out, taken, hidden, respawned
// ge:keywords test q2 items pickup respawn drop-to-floor spawnflags deathmatch q2-server
// ge:skill shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A floored hall; a start at -200 facing +x; rockets in the air ahead (they fall to the floor), a
// railgun past them, a jacket only outside deathmatch, a health sunk in the floor.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-200 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-200 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "ammo_rockets"}, {"origin", "-100 0 60"}}));
  w.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "0 0 24"}}));
  w.entities.push_back(
      entity({{"classname", "item_armor_jacket"}, {"origin", "100 0 24"}, {"spawnflags", "2048"}}));
  w.entities.push_back(entity({{"classname", "item_health"}, {"origin", "0 200 -8"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

sv::RulesRunner start(const char* deathmatch) {
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(map, error), error);
  r.connect(0, "player", false, false);
  return r;
}

// Host ticks of 13 ms with `forward` along +x.
void run(const sv::RulesRunner& r, uint32_t& sequence, int ticks, int16_t forward) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.forward = forward;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = c;
    r.frame(&p, 1);
  }
}

const sv::Q2Entity* named(const sv::Q2Server& s, const char* classname) {
  for (const sv::Q2Entity& e : s.entities)
    if (e.classname == classname) return &e;
  return nullptr;
}

}  // namespace

TEST(items_drop_to_the_floor_and_the_ones_in_a_solid_or_left_out_are_not_there) {
  sv::RulesRunner r = start("1");
  const sv::Q2Server& s = *sv::q2_of(r);
  uint32_t sequence = 0;
  run(r, sequence, 30, 0);  // the items drop 0.2 s in, once the movers stand where they start
  const sv::Q2Entity* rockets = named(s, "ammo_rockets");
  CHECK(rockets != nullptr && !rockets->gone);
  if (rockets) CHECK(rockets->origin.z > 15 && rockets->origin.z < 15.2f);  // resting on z 0
  CHECK(named(s, "item_health")->gone);                                     // started in the floor
  CHECK(named(s, "item_armor_jacket")->gone);  // flagged out of deathmatch
  const sv::RulesRunner single = start("0");
  CHECK(!named(*sv::q2_of(single), "item_armor_jacket")->gone);
  // The items' models are in the list before the player's, which stays last.
  const std::vector<std::string>& models = r->world.models;
  CHECK(models.back() == sv::kQ2PlayerModel);
  CHECK(models[size_t(rockets->modelindex)] == "models/items/ammo/rockets/medium/tris.md2");
}

TEST(a_player_walking_into_items_takes_them_and_they_respawn_in_deathmatch) {
  sv::RulesRunner r = start("1");
  const sv::Q2Server& s = *sv::q2_of(r);
  uint32_t sequence = 0;
  run(r, sequence, 80, 0);     // landed at the start
  run(r, sequence, 120, 300);  // through the rockets and onto the railgun
  const sv::Q2Player& me = s.players[0];
  CHECK(me.taken.size() == 2);
  CHECK(me.body.ammo[int(sv::SqAmmo::rockets)] == 5 && me.body.ammo[int(sv::SqAmmo::slugs)] == 10);
  CHECK(r->world.clients[0].numbers.ammo[2] == 5);  // the HUD's rockets
  const sv::Q2Entity* rockets = named(s, "ammo_rockets");
  CHECK(rockets->hidden && !rockets->gone);
  const size_t row = 1 + sh::kMaxClients + size_t(rockets - s.entities.data());
  CHECK(r->world.entities[row].free);
  run(r, sequence, 2400, 0);  // 31 s
  CHECK(!rockets->hidden && !r->world.entities[row].free);
}

TEST(outside_deathmatch_a_taken_item_is_gone) {
  sv::RulesRunner r = start("0");
  const sv::Q2Server& s = *sv::q2_of(r);
  uint32_t sequence = 0;
  run(r, sequence, 80, 0);
  run(r, sequence, 120, 300);
  CHECK(named(s, "ammo_rockets")->gone);
}

namespace {

// A floored hall with a start at -200 and one of each kind the item flags leave out, plus a quad
// on the start's line and a shotgun that every flag keeps.
std::shared_ptr<const br::Map> flag_hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-200 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "item_quad"}, {"origin", "-100 0 24"}}));
  int y = 100;
  for (const char* c : {"item_health", "item_adrenaline", "item_invulnerability", "item_armor_body",
                        "item_power_shield", "ammo_shells", "weapon_bfg", "weapon_shotgun"}) {
    w.entities.push_back(entity({{"classname", c}, {"origin", "0 " + std::to_string(y) + " 24"}}));
    y += 40;
  }
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

sv::RulesRunner start_flags(int dmflags) {
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 1");
  r.console("dmflags " + std::to_string(dmflags));
  sv::MapSpec map;
  map.name = "flags";
  map.brushes = flag_hall();
  std::string error;
  CHECK_MSG(r.start(map, error), error);
  r.connect(0, "player", false, false);
  return r;
}

// Which of the hall's items are on the map, as "1" (there) or "0" (left out), in the list's order.
std::string present(const sv::Q2Server& s) {
  std::string out;
  for (const char* c :
       {"item_health", "item_adrenaline", "item_invulnerability", "item_armor_body",
        "item_power_shield", "ammo_shells", "weapon_bfg", "weapon_shotgun", "item_quad"})
    out += named(s, c) != nullptr && !named(s, c)->gone ? '1' : '0';
  return out;
}

}  // namespace

TEST(dmflags_leave_health_powerups_armour_and_with_infinite_ammo_the_ammo_off_the_map) {
  // 1 health (adrenaline too), 2 powerups, 2048 armour and power armour, 8192 ammo and the BFG.
  const struct {
    int flags;
    const char* there;
  } cases[] = {{0, "111111111"},    {1, "001111111"},    {2, "110111110"},
               {2048, "111001111"}, {8192, "111110011"}, {1 | 2 | 2048 | 8192, "000000010"}};
  for (const auto& c : cases) {
    sv::RulesRunner r = start_flags(c.flags);
    uint32_t sequence = 0;
    run(r, sequence, 30, 0);
    const std::string got = present(*sv::q2_of(r));
    CHECK_MSG(got == c.there, "dmflags " + std::to_string(c.flags) + ": " + got);
  }
}

TEST(dmflags_16_uses_a_powerup_the_moment_it_is_taken) {
  for (const int flags : {0, 16}) {
    sv::RulesRunner r = start_flags(flags);
    const sv::Q2Server& s = *sv::q2_of(r);
    uint32_t sequence = 0;
    run(r, sequence, 80, 0);    // landed at the start
    run(r, sequence, 40, 300);  // onto the quad
    const sv::Q2Player& me = s.players[0];
    const int quad = sv::sq_item("item_quad");
    CHECK_MSG(named(s, "item_quad")->hidden, "dmflags " + std::to_string(flags));
    if (flags == 16) {
      CHECK(me.body.counts[quad] == 0 && me.quad_until > s.level_time);  // in use, not carried
    } else {
      CHECK(me.body.counts[quad] == 1 && me.quad_until == 0);  // carried for `use`
    }
  }
}

TEST(dmflags_512_spawns_a_player_at_the_spot_farthest_from_the_living) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  for (const char* x : {"-400", "0", "400"})
    w.entities.push_back(entity({{"classname", "info_player_deathmatch"},
                                 {"origin", std::string(x) + " 0 24"},
                                 {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  for (int round = 0; round < 4; round++) {  // never the random pick: always the far end
    sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
    r.console("deathmatch 1");
    r.console("dmflags 512");
    sv::MapSpec spec;
    spec.name = "spots";
    spec.brushes = map;
    CHECK_MSG(r.start(spec, error), error);
    r.connect(0, "Ana", false, false);
    sv::Q2Server& s = *sv::q2_of(r);
    s.players[0].move.origin[0] = static_cast<int16_t>((round % 2 == 0 ? -380 : 380) * 8);
    r.connect(1, "Ben", false, false);
    const int x = s.players[1].move.origin[0] / 8;
    CHECK_MSG(x == (round % 2 == 0 ? 400 : -400), std::to_string(x));
  }
}

TEST(every_class_name_of_the_spawn_table_spawns_on_a_map) {
  // Each class Quake II's maps place, set down far apart in a big floored room in single player:
  // none may be refused as having no spawn function (the line an admin would read).
  static constexpr const char* kClasses[] = {"item_armor_body",
                                             "item_armor_combat",
                                             "item_armor_jacket",
                                             "item_armor_shard",
                                             "item_power_screen",
                                             "item_power_shield",
                                             "weapon_blaster",
                                             "weapon_shotgun",
                                             "weapon_supershotgun",
                                             "weapon_machinegun",
                                             "weapon_chaingun",
                                             "ammo_grenades",
                                             "weapon_grenadelauncher",
                                             "weapon_rocketlauncher",
                                             "weapon_hyperblaster",
                                             "weapon_railgun",
                                             "weapon_bfg",
                                             "ammo_shells",
                                             "ammo_bullets",
                                             "ammo_cells",
                                             "ammo_rockets",
                                             "ammo_slugs",
                                             "item_quad",
                                             "item_invulnerability",
                                             "item_silencer",
                                             "item_breather",
                                             "item_enviro",
                                             "item_ancient_head",
                                             "item_adrenaline",
                                             "item_bandolier",
                                             "item_pack",
                                             "key_data_cd",
                                             "key_power_cube",
                                             "key_pyramid",
                                             "key_data_spinner",
                                             "key_pass",
                                             "key_blue_key",
                                             "key_red_key",
                                             "key_commander_head",
                                             "key_airstrike_target",
                                             "item_health",
                                             "item_health_small",
                                             "item_health_large",
                                             "item_health_mega",
                                             "info_player_deathmatch",
                                             "info_player_coop",
                                             "info_player_intermission",
                                             "func_plat",
                                             "func_button",
                                             "func_door",
                                             "func_door_secret",
                                             "func_door_rotating",
                                             "func_rotating",
                                             "func_train",
                                             "func_water",
                                             "func_conveyor",
                                             "func_areaportal",
                                             "func_clock",
                                             "func_wall",
                                             "func_object",
                                             "func_timer",
                                             "func_explosive",
                                             "func_killbox",
                                             "trigger_always",
                                             "trigger_once",
                                             "trigger_multiple",
                                             "trigger_relay",
                                             "trigger_push",
                                             "trigger_hurt",
                                             "trigger_key",
                                             "trigger_counter",
                                             "trigger_elevator",
                                             "trigger_gravity",
                                             "trigger_monsterjump",
                                             "target_temp_entity",
                                             "target_speaker",
                                             "target_explosion",
                                             "target_changelevel",
                                             "target_secret",
                                             "target_goal",
                                             "target_splash",
                                             "target_spawner",
                                             "target_blaster",
                                             "target_crosslevel_trigger",
                                             "target_crosslevel_target",
                                             "target_laser",
                                             "target_help",
                                             "target_lightramp",
                                             "target_earthquake",
                                             "target_character",
                                             "target_string",
                                             "viewthing",
                                             "light",
                                             "light_mine1",
                                             "light_mine2",
                                             "info_null",
                                             "func_group",
                                             "info_notnull",
                                             "path_corner",
                                             "point_combat",
                                             "misc_explobox",
                                             "misc_banner",
                                             "misc_actor",
                                             "target_actor",
                                             "misc_satellite_dish",
                                             "misc_gib_arm",
                                             "misc_gib_leg",
                                             "misc_gib_head",
                                             "misc_deadsoldier",
                                             "misc_viper",
                                             "misc_viper_bomb",
                                             "misc_bigviper",
                                             "misc_strogg_ship",
                                             "misc_teleporter",
                                             "misc_teleporter_dest",
                                             "misc_blackhole",
                                             "misc_eastertank",
                                             "misc_easterchick",
                                             "misc_easterchick2",
                                             "turret_breach",
                                             "turret_base",
                                             "monster_berserk",
                                             "monster_gladiator",
                                             "monster_gunner",
                                             "monster_infantry",
                                             "monster_soldier_light",
                                             "monster_soldier",
                                             "monster_soldier_ss",
                                             "monster_tank",
                                             "monster_tank_commander",
                                             "monster_medic",
                                             "monster_flipper",
                                             "monster_chick",
                                             "monster_parasite",
                                             "monster_flyer",
                                             "monster_brain",
                                             "monster_floater",
                                             "monster_hover",
                                             "monster_mutant",
                                             "monster_supertank",
                                             "monster_boss2",
                                             "monster_boss3_stand",
                                             "monster_makron",
                                             "monster_jorg",
                                             "monster_commander_body",
                                             "misc_insane",
                                             "turret_driver"};
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-4096, -4096, -16}, hi[3] = {4096, 4096, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-4000 -4000 24"}, {"angle", "0"}}));
  int i = 0;
  for (const char* c : kClasses) {
    const int x = -3600 + (i % 12) * 600, y = -3600 + (i / 12) * 600;
    w.entities.push_back(entity({{"classname", c},
                                 {"origin", std::to_string(x) + " " + std::to_string(y) + " 40"},
                                 {"targetname", std::string("t") + std::to_string(i)}}));
    i++;
  }
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 0");
  sv::MapSpec spec;
  spec.name = "table";
  spec.brushes = map;
  CHECK_MSG(r.start(spec, error), error);
  r.connect(0, "player", false, false);
  uint32_t sequence = 0;
  run(r, sequence, 30, 0);
  for (const std::string& line : sv::q2_of(r)->console)
    CHECK_MSG(line.find("doesn't have a spawn function") == std::string::npos, line);
  for (const char* c : kClasses) CHECK_MSG(named(*sv::q2_of(r), c) != nullptr, c);
}
