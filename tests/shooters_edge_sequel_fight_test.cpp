// Tests: the sequel's fight on its runner (games/shooters_edge/server/sequel_fight.cpp): two
// players face each other down a floored hall. The railgun's line kills through the armour and
// scores the obituary; a rocket flies and hits for its direct damage (its blast leaves the one it
// hit out); a rocket fired at the floor lifts its shooter and hurts him by half (the rocket jump);
// the machinegun's bullets take 8 each; a number key changes the weapon; a taken weapon is raised
// at once over the blaster; `kill` scores −1; the HUD shows the weapon's model, frame and ammo.
// Then the rest of the fight: quad damage ×4, invulnerability, the power shield and the screen
// (only from in front), a fall's damage, lava each frame, and the frag limit ending the level.
//
// ge:summary Tests: the sequel's fight: rail, rocket, rocket jump, bullets, changing, obituaries
// ge:keywords test sequel fight railgun rocket rocket-jump machinegun obituary frags use-weapon
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_fight_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/num.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long floored hall, walled at its ends, a start at each end facing down it.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
  const float lava_lo[3] = {700, -256, 0}, lava_hi[3] = {1000, 256, 40};  // a pool over the floor
  w.models[0].push_back(br::box_brush(lava_lo, lava_hi, sh::kSqLava, "world/lava"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "-260 0 24"}}));
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
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p(int i) const { return s().players[i]; }
};

std::unique_ptr<Arena> arena(const char* deathmatch = "1") {
  auto a = std::make_unique<Arena>();
  a->r = sv::sequel_runner(sv::make_sequel_server({}));
  a->r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->r.connect(1, "Ben", false, false);
  return a;
}

// Host ticks of 13 ms: Ana with `buttons` and `pitch` (degrees down), Ben standing still.
void run(Arena& a, int ticks, uint8_t buttons = 0, float pitch = 0, uint8_t impulse = 0) {
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
        cmd.impulse = i == 0 ? impulse : 0;
      }
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

void place(Arena& a, int i, float x) {
  int16_t* o = a.p(i).move.origin;
  o[0] = static_cast<int16_t>(x * 8);
  o[1] = 0;
  for (int16_t& v : a.p(i).move.velocity) v = 0;
}

// Both landed, Ana at x −300 looking down the hall at Ben at x `ben`.
std::unique_ptr<Arena> facing(float ben, const char* deathmatch = "1") {
  auto a = arena(deathmatch);
  run(*a, 80);
  place(*a, 0, -300);
  place(*a, 1, ben);
  a->p(0).body = sv::sq_new_body();  // the railgun at her feet is the pickup test's
  run(*a, 20);
  return a;
}

void give(Arena& a, sv::SqWeapon w, int ammo) {
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  a.p(0).body.counts[sv::sq_item(r.item)] = 1;
  a.p(0).body.ammo[int(r.ammo)] = ammo;
  CHECK(a.r.client_command(0, std::string("use ") + r.name) == sv::CommandResult::mod);
  run(a, 100);  // the blaster dropped, the new one raised
  CHECK(a.p(0).hand.weapon == int(w) && a.p(0).hand.state == sv::SqHandState::ready);
}

}  // namespace

TEST(the_rail_kills_through_armour_and_the_obituary_scores) {
  auto a = facing(300);
  a->p(1).body.armour = sv::SqArmour::jacket;
  a->p(1).body.armour_count = 50;
  give(*a, sv::SqWeapon::railgun, 10);
  run(*a, 8, sh::kButtonAttack);  // one rules frame: one slug
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 9);
  CHECK(a->p(1).alive);
  // The jacket's 0.3 × 100 is 30.000002 in floats, so it saves 31.
  CHECK_MSG(a->p(1).health == 100 - 69, std::to_string(a->p(1).health));
  CHECK(a->p(1).body.armour_count == 50 - 31);
  run(*a, 16 * 8, sh::kButtonAttack);  // the next slug, a period later
  CHECK(!a->p(1).alive && a->p(0).frags == 1);
  CHECK(!a->s().obituaries.empty() && a->s().obituaries.back() == "Ben was pierced by Ana's rail.");
}

TEST(a_rocket_flies_and_hits_for_its_direct_damage) {
  auto a = facing(300);
  a->p(1).health = 500;  // to read the whole blow
  give(*a, sv::SqWeapon::rocket_launcher, 5);
  run(*a, 8, sh::kButtonAttack);
  int flying = 0;
  for (const sv::SequelMissile& m : a->s().missiles) flying += m.live ? 1 : 0;
  CHECK(flying == 1);
  run(*a, 100);  // 600 units at 650 units/s
  for (const sv::SequelMissile& m : a->s().missiles) CHECK(!m.live);
  const float lost = 500 - a->p(1).health;
  CHECK_MSG(lost >= 100 && lost <= 120, std::to_string(lost));  // the blast leaves him out
}

TEST(a_rocket_at_the_floor_lifts_its_shooter_and_hurts_him_by_half) {
  auto a = facing(-800);
  give(*a, sv::SqWeapon::rocket_launcher, 5);
  run(*a, 8, sh::kButtonAttack, 89);
  run(*a, 2);
  const float up = float(a->p(0).move.velocity[2]) / 8;
  CHECK_MSG(up > 200, std::to_string(up));
  CHECK(a->p(0).health < 100 && a->p(0).health > 100 - 60);
  CHECK(a->p(0).alive);
}

TEST(bullets_take_eight_each_and_a_number_key_changes_the_weapon) {
  auto a = facing(200);
  const sv::SqWeaponRow& mg = sv::kSqWeapons[int(sv::SqWeapon::machinegun)];
  a->p(0).body.counts[sv::sq_item(mg.item)] = 1;
  a->p(0).body.ammo[int(sv::SqAmmo::bullets)] = 3;
  run(*a, 100, 0, 0, 4);  // impulse 4: the machinegun
  CHECK(a->p(0).hand.weapon == int(sv::SqWeapon::machinegun));
  a->p(1).health = 500;
  run(*a, 8 * 5, sh::kButtonAttack);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::bullets)] == 0);
  const float lost = 500 - a->p(1).health;
  CHECK_MSG(lost >= 8 && int(lost) % 8 == 0, std::to_string(lost));  // most hit at 200 units
  // Out of bullets: a click, and the blaster again.
  run(*a, 100, sh::kButtonAttack);
  CHECK(a->p(0).hand.weapon == int(sv::SqWeapon::blaster));
}

TEST(a_taken_weapon_is_raised_over_the_blaster_and_the_hud_shows_it) {
  auto a = arena();
  run(*a, 80);
  place(*a, 0, -340);  // the railgun lies at −260
  run(*a, 120, 0, 0);
  sh::Packet p;
  for (int i = 0; i < 60; i++) {
    p.client = 0;
    p.sequence = ++a->sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.forward = 300;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = c;
    a->r.frame(&p, 1);
  }
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 10);
  run(*a, 100);
  CHECK(a->p(0).hand.weapon == int(sv::SqWeapon::railgun));
  const sv::PlayerNumbers& n = a->r->world.clients[0].numbers;
  CHECK(a->r->world.models[size_t(n.weapon_model)] == "models/weapons/v_rail/tris.md2");
  CHECK(n.current_ammo == 10);
  CHECK(a->r->world.models.back() == sv::kSequelPlayerModel);
}

TEST(kill_scores_minus_one_and_the_dead_respawn_by_attack) {
  auto a = facing(300);
  run(*a, 8 * 60);  // past the 5 s `kill` waits
  CHECK(a->r.client_command(0, "kill") == sv::CommandResult::mod);
  CHECK(!a->p(0).alive && a->p(0).frags == -1);
  CHECK(a->s().obituaries.back() == "Ana gave up.");
  run(*a, 8 * 12);
  run(*a, 8, sh::kButtonAttack);
  CHECK(a->p(0).alive && a->p(0).health == 100);
  CHECK(a->p(0).hand.weapon == int(sv::SqWeapon::blaster));
}

TEST(quad_damage_strikes_four_times_as_hard_and_invulnerability_takes_none) {
  auto a = facing(300);
  a->p(1).health = 1000;
  give(*a, sv::SqWeapon::railgun, 10);
  a->p(0).body.counts[sv::sq_item("item_quad")] = 1;
  CHECK(a->r.client_command(0, "use Quad Damage") == sv::CommandResult::mod);
  CHECK(a->p(0).body.counts[sv::sq_item("item_quad")] == 0);
  run(*a, 8, sh::kButtonAttack);
  CHECK_MSG(a->p(1).health == 1000 - 400, std::to_string(a->p(1).health));
  a->p(1).body.counts[sv::sq_item("item_invulnerability")] = 1;
  a->r.client_command(1, "use invulnerability");
  run(*a, 16 * 8, sh::kButtonAttack);
  CHECK(a->p(1).health == 600 && a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 8);
}

TEST(the_power_shield_takes_two_thirds_and_the_screen_only_guards_in_front) {
  auto a = facing(300);
  a->p(1).body.counts[sv::sq_item("item_power_shield")] = 1;
  a->p(1).body.ammo[int(sv::SqAmmo::cells)] = 100;
  a->r.client_command(1, "use Power Shield");
  CHECK(a->p(1).power_on);
  give(*a, sv::SqWeapon::railgun, 10);
  run(*a, 8, sh::kButtonAttack);
  CHECK_MSG(a->p(1).health == 100 - 34, std::to_string(a->p(1).health));  // 66 saved
  CHECK(a->p(1).body.ammo[int(sv::SqAmmo::cells)] == 100 - 33);
  // A screen, Ben facing away from the shot: it guards nothing.
  auto b = facing(300);
  b->p(1).body.counts[sv::sq_item("item_power_screen")] = 1;
  b->p(1).body.ammo[int(sv::SqAmmo::cells)] = 100;
  b->r.client_command(1, "use Power Screen");
  give(*b, sv::SqWeapon::railgun, 10);
  run(*b, 8, sh::kButtonAttack);
  CHECK(!b->p(1).alive);
  CHECK(b->p(1).body.ammo[int(sv::SqAmmo::cells)] == 100);
}

TEST(a_long_fall_hurts_and_lava_burns_each_frame) {
  auto a = facing(300);
  a->p(0).move.origin[2] = static_cast<int16_t>((24 + 600) * 8);
  run(*a, 8 * 20);
  // Down 600 units: about 810 units/s at the last frame before the floor, (65 − 30) / 2 = 17.
  CHECK_MSG(a->p(0).health == 100 - 17, std::to_string(a->p(0).health));
  place(*a, 1, 850);  // into the lava pool
  run(*a, 8 * 3);
  CHECK(a->p(1).health < 100 && a->p(1).water_level > 0);
  run(*a, 8 * 40);
  CHECK(!a->p(1).alive);
  CHECK(!a->s().obituaries.empty() && a->s().obituaries.back() == "Ben sank into the lava.");
  CHECK(a->p(1).frags == -1);
}

TEST(the_frag_limit_ends_the_level) {
  auto a = facing(300);
  a->r.console("fraglimit 1");
  give(*a, sv::SqWeapon::railgun, 10);
  a->p(1).health = 50;
  run(*a, 8, sh::kButtonAttack);
  CHECK(!a->p(1).alive && a->p(0).frags == 1);
  run(*a, 8);
  CHECK(a->r->pending_map == "hall");
}
