// Tests: Ground Zero's rules (games/shooters_edge/server/q2_gz.hpp): the chainfist, the ETF rifle,
// the plasma beam, the disruptor, the prox mine, the tesla and the A-M bomb on a hall, double
// damage's multiplier, the obituaries' words and the ammo a dry body turns to.
//
// ge:summary Tests: Ground Zero's weapons, mines and bomb, double damage, obituaries
// ge:keywords test q2 ground-zero rogue chainfist etf-rifle plasma-beam disruptor prox-mine tesla
// ge:keywords am-bomb double-damage obituary
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/content/load_order.hpp"
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
using sv::SqWeapon;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long hall with a floor at 0 and walls at x ±1024.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-900 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Arena {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

// Host ticks of 13 ms: Ana with `buttons` and `pitch` (degrees down), Ben standing still.
void run(Arena& a, int ticks, uint8_t buttons = 0, float pitch = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++a.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      if (c == 0) {
        cmd.buttons = buttons;
        cmd.angles = {pitch, 0, 0};
      }
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

void place(Arena& a, int i, float x) {
  int32_t* o = a.p(i).move.origin;
  o[0] = static_cast<int32_t>(x * 8);
  o[1] = 0;
  for (int32_t& v : a.p(i).move.velocity) v = 0;
}

// Ground Zero's rules on the hall: Ana at x −300 looking down it at Ben at x `ben`.
std::unique_ptr<Arena> facing(float ben, const char* deathmatch = "1") {
  auto a = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  a->r = sv::q2_runner(sv::make_q2_server(o));
  a->r.console(std::string("deathmatch ") + deathmatch);
  a->r.console("sv_fps 10");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->p(0).move.origin[0] = static_cast<int32_t>(a->p(0).move.origin[0] - 64 * 8);
  a->r.connect(1, "Ben", false, false);
  run(*a, 80);
  place(*a, 0, -300);
  place(*a, 1, ben);
  run(*a, 20);
  return a;
}

void give(Arena& a, SqWeapon w, int ammo) {
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  a.p(0).body.counts[sv::sq_item(r.item, sv::Q2Pack::ground_zero)] = 1;
  if (r.ammo != sv::SqAmmo::none) a.p(0).body.ammo[int(r.ammo)] = ammo;
  CHECK(a.r.client_command(0, std::string("use ") + r.name) == sv::CommandResult::mod);
  run(a, 100);  // the blaster dropped, the new one raised
  CHECK_MSG(a.p(0).hand.weapon == int(w) && a.p(0).hand.state == sv::SqHandState::ready, r.name);
}

int events_named(const Arena& a, const char* name) {
  int n = 0;
  for (const sv::NetEvent& e : a.s().out.events) n += e.text == name ? 1 : 0;
  return n;
}

TEST(the_plasma_beam_burns_15_a_frame_and_draws_its_beam) {
  auto a = facing(200);
  give(*a, SqWeapon::plasma_beam, 100);
  const float before = a->p(1).health;
  bool beam = false;
  for (int i = 0; i < 8; i++) {  // a tenth: one frame's 15
    run(*a, 1, 1);
    beam |= events_named(*a, "te_heatbeam") > 0;
  }
  CHECK(beam);
  CHECK(a->p(1).health < before);
  CHECK(int(before - a->p(1).health) % 15 == 0);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::cells)] < 100);
}

TEST(the_etf_rifle_s_flechettes_pass_armour) {
  auto a = facing(0);
  a->p(1).body.armour = sv::SqArmour::body;
  a->p(1).body.armour_count = 100;
  give(*a, SqWeapon::etf_rifle, 50);
  const float before = a->p(1).health;
  run(*a, 60, 1);
  CHECK(a->p(1).health < before || !a->p(1).alive);
  CHECK(a->p(1).body.armour_count == 100);  // flechettes skip the armour
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::flechettes)] < 50);
}

TEST(the_chainfist_hits_within_64_and_pulls_its_wielder) {
  auto a = facing(-250);  // 50 apart
  give(*a, SqWeapon::chainfist, 0);
  const float before = a->p(1).health;
  run(*a, 120, 1);  // a swing or two
  CHECK(a->p(1).health < before);
  auto far = facing(0);
  give(*far, SqWeapon::chainfist, 0);
  const float full = far->p(1).health;
  run(*far, 120, 1);
  CHECK(far->p(1).health == full);  // 300 away: nothing
}

TEST(the_disruptor_follows_and_hurts_over_half_a_second) {
  auto a = facing(300);
  give(*a, SqWeapon::disruptor, 15);
  const float before = a->p(1).health;
  run(*a, 120, 1);
  CHECK(a->p(1).health <= before - 30 || !a->p(1).alive);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::rounds)] < 15);
}

TEST(a_prox_mine_sticks_arms_and_goes_off_when_a_body_comes_near) {
  auto a = facing(900);
  give(*a, SqWeapon::prox_launcher, 5);
  run(*a, 2, 1);  // one mine, down the hall onto the floor
  run(*a, 200);   // landed, opened, armed
  const sv::Q2Missile* mine = nullptr;
  for (const sv::Q2Missile& m : a->s().missiles)
    if (m.live && m.kind == sv::SqShot::prox) mine = &m;
  CHECK(mine != nullptr);
  if (mine == nullptr) return;
  CHECK(mine->stage == 2);  // stuck and armed
  const float before = a->p(1).health;
  place(*a, 1, mine->origin.x + 60);  // into its field: 0.5 s later it goes off
  run(*a, 80);
  bool live = false;
  for (const sv::Q2Missile& m : a->s().missiles) live |= m.live && m.kind == sv::SqShot::prox;
  CHECK(!live);
  CHECK(a->p(1).health < before || !a->p(1).alive);
}

TEST(double_damage_doubles_and_stacks_with_the_quad_in_deathmatch) {
  auto a = facing(200);
  sv::Q2Player& p = a->p(0);
  const double now = a->s().level_time;
  CHECK(sv::fight_multiplier(a->s(), p) == 1);
  p.double_until = now + 30;
  CHECK(sv::fight_multiplier(a->s(), p) == 2);
  p.quad_until = now + 30;
  CHECK(sv::fight_multiplier(a->s(), p) == 8);
  a->r.console("dmflags 262144");  // no stacking
  CHECK(sv::fight_multiplier(a->s(), p) == 4);
  CHECK(std::string(sv::fight_multiplier_sound(2)) == "misc/ddamage3.wav");
  CHECK(std::string(sv::fight_multiplier_sound(8)) == "items/damage3.wav");
  CHECK(sv::fight_multiplier_sound(1) == nullptr);
}

TEST(the_am_bomb_kills_within_its_radius) {
  auto a = facing(200);
  sv::Q2Player& p = a->p(0);
  const int bomb = sv::sq_item("ammo_nuke", sv::Q2Pack::ground_zero);
  p.body.counts[bomb] = 1;
  CHECK(sv::fight_use(a->s(), 0, bomb));
  run(*a, 800);  // 10 s on (and some)
  CHECK(!a->p(1).alive);
  CHECK(a->p(1).frags == 0);
}

TEST(ground_zero_s_obituaries) {
  const auto line = [](SqWeapon w) {
    return sv::sq_obituary("Ben", "Ana", false, int(w), sv::kSqHowDirect, "male");
  };
  CHECK(line(SqWeapon::chainfist) == "Ben was shredded by Ana's ripsaw");
  CHECK(line(SqWeapon::etf_rifle) == "Ben was perforated by Ana");
  CHECK(line(SqWeapon::plasma_beam) == "Ben was scorched by Ana's plasma beam");
  CHECK(line(SqWeapon::tesla) == "Ben was enlightened by Ana's tesla mine");
  CHECK(line(SqWeapon::prox_launcher) == "Ben got too close to Ana's proximity mine");
  CHECK(line(SqWeapon::disruptor) == "Ben was annihilated by Ana's disruptor");
  CHECK(sv::sq_obituary("Ben", "Ana", false, sv::kSqByAmBomb, sv::kSqHowBlast, "male") ==
        "Ben was nuked by Ana's antimatter bomb");
}

TEST(a_dry_body_turns_to_ground_zero_s_order) {
  sv::SqBody b = sv::sq_new_body(sv::Q2Pack::ground_zero);
  b.counts[sv::sq_item("weapon_hyperblaster")] = 1;
  b.ammo[int(sv::SqAmmo::cells)] = 50;
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::blaster));  // the hyperblaster is never chosen
  b.counts[sv::sq_item("weapon_etf_rifle", sv::Q2Pack::ground_zero)] = 1;
  b.ammo[int(sv::SqAmmo::flechettes)] = 10;
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::etf_rifle));
  b.counts[sv::sq_item("weapon_plasmabeam", sv::Q2Pack::ground_zero)] = 1;
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::plasma_beam));
  b.ammo[int(sv::SqAmmo::cells)] = 1;  // not enough for a frame
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::etf_rifle));
}

// A unit's start config (runit2.cfg: `give etf rifle`, `give prox 15`, ...) takes the pack's
// item names in single player.
TEST(a_unit_s_start_config_gives_the_pack_s_weapons_by_name) {
  auto a = facing(200, "0");
  a->r.console("cheats 1");
  for (const char* line : {"give etf rifle", "give prox launcher", "give plasma beam",
                           "give chainfist", "give flechettes 125", "give prox 15"})
    a->r.client_command(0, line);
  const sv::SqBody& b = a->p(0).body;
  for (const char* item :
       {"weapon_etf_rifle", "weapon_proxlauncher", "weapon_plasmabeam", "weapon_chainfist"})
    CHECK_MSG(b.counts[sv::sq_item(item, sv::Q2Pack::ground_zero)] == 1, item);
  CHECK(b.ammo[int(sv::SqAmmo::flechettes)] == 125);
  CHECK(b.ammo[int(sv::SqAmmo::prox)] == 15);
}

// The pack's own default.cfg names its first map, as the base game's does.
TEST(the_new_game_starts_on_the_map_the_game_s_default_cfg_names) {
  namespace cc = ge::classic::content;
  CHECK(cc::new_game_map("// newgame command\nalias   newgame \" killserver ; maxclients 1 ; "
                         "deathmatch 0 ; map *rintro.cin+rmine1\"\n") == "rmine1");
  CHECK(cc::new_game_map("alias newgame \"map *logo.cin+xin.cin+xswamp\"") == "xswamp");
  CHECK(cc::new_game_map("alias newgame \"map *ntro.cin+base1$start\"\r\n") == "base1");
  CHECK(cc::new_game_map("alias newgame \"map victory.pcx\"").empty());
  CHECK(cc::new_game_map("bind 1 \"use Blaster\"\nmap base2\n").empty());
}

// A game of the pack saved with mines in flight and stuck, a tesla thrown and double damage
// running, loaded into a fresh server of the pack, plays on as the game never saved.
TEST(a_ground_zero_game_saved_and_loaded_plays_on_as_the_game_never_saved) {
  auto a = facing(900, "0");
  give(*a, SqWeapon::prox_launcher, 5);
  run(*a, 2, 1);  // a mine down the hall
  run(*a, 120);   // landed and opening
  run(*a, 2, 1);  // another, in flight at the save
  a->p(0).double_until = a->s().level_time + 20;
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(sv::q2_save(a->s(), file, error), error);
  auto b = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  b->r = sv::q2_runner(sv::make_q2_server(o));
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  CHECK_MSG(sv::q2_load(b->r, map, file, "test", error), error);
  b->sequence = a->sequence;
  CHECK_MSG(sv::q2_digest(b->s()) == sv::q2_digest(a->s()), "the loaded game differs");
  for (int k = 0; k < 400; k++) {
    const uint8_t buttons = k % 60 < 2 ? 1 : 0;  // a mine a little under a second apart
    run(*a, 1, buttons);
    run(*b, 1, buttons);
    if (sv::q2_digest(b->s()) != sv::q2_digest(a->s())) {
      CHECK_MSG(false, "the loaded game went its own way at tick " + std::to_string(k));
      break;
    }
  }
  CHECK(b->p(0).double_until > 0);
}

// The pack's last level hands every player the A-M bomb, selected, outside deathmatch.
TEST(the_last_level_hands_out_the_am_bomb_selected) {
  for (const char* dm : {"0", "1"}) {
    auto a = std::make_unique<Arena>();
    sv::Q2ServerOptions o;
    o.pack = sv::Q2Pack::ground_zero;
    a->r = sv::q2_runner(sv::make_q2_server(o));
    a->r.console(std::string("deathmatch ") + dm);
    sv::MapSpec map;
    map.name = "rboss";
    map.brushes = hall();
    std::string error;
    CHECK_MSG(a->r.start(map, error), error);
    a->r.connect(0, "Ana", false, false);
    run(*a, 20);
    const int bomb = sv::sq_item("ammo_nuke", sv::Q2Pack::ground_zero);
    const sv::SqBody& b = a->p(0).body;
    const bool single = dm[0] == '0';
    CHECK_MSG((b.counts[bomb] == 1) == single, dm);
    CHECK((b.selected == sv::sq_item_number(bomb, sv::Q2Pack::ground_zero)) == single);
  }
}

}  // namespace

// A single player's room under Ground Zero: an exit trigger with TOGGLE (8) and two relays that
// use it, a box of bullets spawned by a trigger (1) above the floor, a health with a flag it lacks.
std::unique_ptr<Arena> room() {
  br::BrushWorld w;
  w.models.resize(4);
  const float floor_lo[3] = {-512, -512, -16}, floor_hi[3] = {512, 512, 0};
  const float exit_lo[3] = {384, -64, 0}, exit_hi[3] = {448, 64, 96};
  const float door_lo[3] = {0, 256, 0}, door_hi[3] = {64, 264, 128};
  const float crate_lo[3] = {-256, 256, 0}, crate_hi[3] = {-192, 320, 64};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[1].push_back(br::box_brush(exit_lo, exit_hi, sh::kSqSolid, "common/trigger"));
  w.models[2].push_back(br::box_brush(door_lo, door_hi, sh::kSqSolid, "world/door"));
  w.models[3].push_back(br::box_brush(crate_lo, crate_hi, sh::kSqSolid, "world/crate"));
  const auto put = [&](std::vector<std::pair<std::string, std::string>> pairs) {
    w.entities.push_back(entity(std::move(pairs)));
  };
  put({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}});
  put({{"classname", "trigger_multiple"},
       {"model", "*1"},
       {"spawnflags", "8"},
       {"targetname", "exiter"},
       {"target", "out"}});
  put({{"classname", "target_changelevel"}, {"targetname", "out"}, {"map", "next"}});
  put({{"classname", "trigger_relay"}, {"targetname", "r"}, {"target", "exiter"}});
  put({{"classname", "ammo_bullets"},
       {"origin", "0 0 96"},
       {"spawnflags", "2049"},
       {"targetname", "t1"}});
  put({{"classname", "item_health"}, {"origin", "-128 0 24"}, {"spawnflags", "2"}});
  put({{"classname", "func_door_rotating"},
       {"model", "*2"},
       {"origin", "0 256 0"},
       {"spawnflags", "8192"},
       {"targetname", "d"},
       {"distance", "90"}});
  put({{"classname", "func_explosive"}, {"model", "*3"}, {"spawnflags", "8"}, {"targetname", "c"}});
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error),
            error);
  auto a = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  a->r = sv::q2_runner(sv::make_q2_server(o));
  a->r.console("deathmatch 0");
  sv::MapSpec spec;
  spec.name = "room";
  spec.brushes = map;
  CHECK_MSG(a->r.start(spec, error), error);
  a->r.connect(0, "Ana", false, false);
  a->r.connect(1, "Ben", false, false);
  run(*a, 40);
  return a;
}

int named(const sv::Q2Server& s, std::string_view classname) {
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].classname == classname) return static_cast<int>(i);
  return -1;
}

int use_named(sv::Q2Server& s, const char* target) {
  sv::Q2Entity relay;
  relay.classname = "trigger_relay";
  relay.target = target;
  s.entities.push_back(relay);
  sv::map_use_targets(s, static_cast<int>(s.entities.size() - 1), 0);
  return static_cast<int>(s.entities.size() - 1);
}

TEST(a_toggled_trigger_starts_off_and_each_use_switches_it_never_firing_it) {
  auto a = room();
  sv::Q2Server& s = a->s();
  const int exit = named(s, "trigger_multiple"), relay = named(s, "trigger_relay");
  CHECK(!s.entities[size_t(exit)].trigger);
  sv::map_use_targets(s, relay, 0);
  CHECK(s.entities[size_t(exit)].trigger && s.pending_map.empty());  // on, and nothing left for
  sv::map_use_targets(s, relay, 0);
  CHECK(!s.entities[size_t(exit)].trigger && s.pending_map.empty());  // off again
}

TEST(a_trigger_spawned_item_waits_unseen_where_placed_then_drops_when_used) {
  auto a = room();
  sv::Q2Server& s = a->s();
  const int box = named(s, "ammo_bullets");
  CHECK(s.entities[size_t(box)].hidden && s.entities[size_t(box)].origin.z == 96);
  bool said_box = false, said_health = false;
  for (const std::string& l : s.console) {
    said_box = said_box || l.find("ammo_bullets at") == 0;
    said_health = said_health || l == "item_health at (-128 0 24) has invalid spawnflags set";
  }
  CHECK(!said_box && said_health);  // 1 is the pack's own; 2 on a health is not
  CHECK(!s.entities[size_t(named(s, "item_health"))].hidden);
  use_named(s, "t1");
  CHECK_MSG(
      !s.entities[size_t(box)].hidden && std::abs(s.entities[size_t(box)].origin.z - 15) < 0.2f,
      std::to_string(s.entities[size_t(box)].origin.z));
  CHECK((s.entities[size_t(box)].spawnflags & 1) == 0);  // once
}

TEST(an_inactive_rotating_door_wakes_on_its_first_use_and_opens_by_touch_after) {
  auto a = room();
  sv::Q2Server& s = a->s();
  const int door = named(s, "func_door_rotating"), field = named(s, "door_field");
  CHECK(field >= 0 && !s.entities[size_t(field)].trigger);  // asleep: no touch opens it
  use_named(s, "d");
  CHECK(s.entities[size_t(field)].trigger);
  CHECK(s.entities[size_t(door)].state == 0);  // woken, not opened
  use_named(s, "d");
  CHECK(s.entities[size_t(door)].state == 0);  // no trigger uses it again
}

TEST(an_inactive_explosive_takes_no_damage_until_woken_then_a_use_blows_it) {
  auto a = room();
  sv::Q2Server& s = a->s();
  const int crate = named(s, "func_explosive");
  const int health = s.entities[size_t(crate)].health;
  sv::map_damage(s, crate, 0, 10, {1, 0, 0});
  CHECK(s.entities[size_t(crate)].health == health);
  use_named(s, "c");
  CHECK(!s.entities[size_t(crate)].gone);
  sv::map_damage(s, crate, 0, 10, {1, 0, 0});
  CHECK(s.entities[size_t(crate)].health == health - 10);
  use_named(s, "c");
  CHECK(s.entities[size_t(crate)].gone);
}

TEST(the_laser_passes_on_through_a_thing_it_hurts) {
  auto a = room();
  sv::Q2Server& s = a->s();
  const sh::Vec3f from{-300, 288, 32}, to{-100, 288, 32};
  CHECK(std::abs(sv::fight_laser(s, from, to, 1, -1, -1).at.x + 256) < 0.1f);  // asleep: stops
  use_named(s, "c");
  const int crate = named(s, "func_explosive");
  const int health = s.entities[size_t(crate)].health;
  CHECK(sv::fight_laser(s, from, to, 1, -1, -1).at.x == -100);  // hurt once, and on past it
  CHECK(s.entities[size_t(crate)].health == health - 1);
}
