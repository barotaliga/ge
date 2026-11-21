// Tests: Quake II's fight on its runner (games/shooters_edge/server/q2_fight.cpp): two
// players face each other down a floored hall. The railgun's line kills through the armour and
// scores the obituary; a rocket flies and hits for its direct damage (its blast leaves the one it
// hit out); a rocket fired at the floor lifts its shooter and hurts him by half (the rocket jump);
// the machinegun's bullets take 8 each; a number key changes the weapon; a taken weapon is raised
// at once over the blaster; `kill` scores −1; the HUD shows the weapon's model, frame and ammo.
// Then the rest of the fight: quad damage ×4, invulnerability, the power shield and the screen
// (only from in front), a fall's damage, lava each frame, and the frag limit ending the level
// after the intermission. Then the hand grenade (thrown faster the longer it is held, exploding
// in the hand at the fuse's end), the weapon and the quad dropped at death and taken back, gibs,
// and `drop`.
//
// ge:summary Tests: Quake II's fight: rail, rocket, rocket jump, bullets, changing, obituaries
// ge:keywords test q2 fight railgun rocket rocket-jump machinegun obituary frags use-weapon
// ge:keywords hand-grenade drop-item gibs intermission sv_dropquad sv_droppent sv_dropsuit
// ge:keywords sv_dropbreather sv_dropsilencer aimfix g_machinegun_norecoil g_footsteps
// ge:keywords sv_maxvelocity machinegun-climb
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "core/num.hpp"
#include "shooters_edge/server/q2_fight.hpp"
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
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

std::unique_ptr<Arena> arena(const char* deathmatch = "1", const char* dmflags = "0",
                             int fps = 10) {
  auto a = std::make_unique<Arena>();
  a->r = sv::q2_runner(sv::make_q2_server({}));
  a->r.console(std::string("deathmatch ") + deathmatch);
  a->r.console(std::string("dmflags ") + dmflags);
  a->r.console("sv_fps " + std::to_string(fps));
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
std::unique_ptr<Arena> facing(float ben, const char* deathmatch = "1", int fps = 10) {
  auto a = arena(deathmatch, "0", fps);
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

// The hand grenade in flight, if any (`found` says).
const sv::Q2Missile* thrown(sv::Q2Server& s, bool& found) {
  found = false;
  for (const sv::Q2Missile& m : s.missiles)
    if (m.live && m.kind == sv::SqShot::hand_grenade) {
      found = true;
      return &m;
    }
  return nullptr;
}

// A sound a tick started: the entity, the channel, the name.
struct Heard {
  int ent = 0, channel = 0;
  std::string name;
};

// `ticks` host ticks as `run`, with the sounds they started.
std::vector<Heard> listen(Arena& a, int ticks, uint8_t buttons = 0, float pitch = 0) {
  std::vector<Heard> out;
  for (int i = 0; i < ticks; i++) {
    const int64_t before = a.s().out.events_total;
    run(a, 1, buttons, pitch);
    const std::vector<sv::NetEvent>& ev = a.s().out.events;
    const int64_t first = a.s().out.events_total - static_cast<int64_t>(ev.size());
    for (size_t k = static_cast<size_t>(std::max<int64_t>(0, before - first)); k < ev.size(); k++)
      if (ev[k].kind == sv::EventKind::sound) out.push_back({ev[k].ent, ev[k].level, ev[k].text});
  }
  return out;
}

int count(const std::vector<Heard>& h, int ent, const std::string& prefix) {
  int n = 0;
  for (const Heard& x : h)
    if (x.ent == ent && x.name.rfind(prefix, 0) == 0) n++;
  return n;
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
  CHECK(!a->s().obituaries.empty() && a->s().obituaries.back() == "Ben was railed by Ana");
}

// Lag compensation (the network server's `lag_place`): a player's rail meets the others where
// his screen showed them, in the box they had. Ben stands in Ana's line; shown 200 units aside,
// or crouched, the slug misses him.
namespace {
bool shown_aside(void* ctx, int shooter, int target, sh::Vec3f& at, sh::Vec3f& mins,
                 sh::Vec3f& maxs) {
  const auto& s = *static_cast<const sv::Q2Server*>(ctx);
  if (shooter != 0 || target != 1) return false;
  const int16_t* o = s.players[1].move.origin;
  at = {float(o[0]) * 0.125f, float(o[1]) * 0.125f + 200, float(o[2]) * 0.125f};
  mins = s.players[1].mins;
  maxs = s.players[1].maxs;
  return true;
}
// Shown where he stands, but crouched (his box then 4 high): the slug at eye height passes over.
bool shown_crouched(void* ctx, int shooter, int target, sh::Vec3f& at, sh::Vec3f& mins,
                    sh::Vec3f& maxs) {
  const auto& s = *static_cast<const sv::Q2Server*>(ctx);
  if (shooter != 0 || target != 1) return false;
  const int16_t* o = s.players[1].move.origin;
  at = {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f};
  mins = s.players[1].mins;
  maxs = s.players[1].maxs;
  maxs.z = 4;
  return true;
}
}  // namespace

TEST(a_rail_meets_the_others_where_the_shooter_saw_them) {
  auto a = facing(300);
  a->s().lag_place = &shown_aside;
  a->s().lag_ctx = &a->s();
  give(*a, sv::SqWeapon::railgun, 10);
  run(*a, 8, sh::kButtonAttack);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 9);
  CHECK_MSG(a->p(1).health == 100, std::to_string(a->p(1).health));
  CHECK(a->s().lag_shooter == -1);  // the rewind ends with the shot
  // Shown where he stands (no rewind): hit.
  auto b = facing(300);
  give(*b, sv::SqWeapon::railgun, 10);
  run(*b, 8, sh::kButtonAttack);
  CHECK(b->p(1).health < 100);
  // Shown crouched where he stands: the rewound box is the one met, and the slug passes over.
  auto c = facing(300);
  c->s().lag_place = &shown_crouched;
  c->s().lag_ctx = &c->s();
  give(*c, sv::SqWeapon::railgun, 10);
  run(*c, 8, sh::kButtonAttack);
  CHECK_MSG(c->p(1).health == 100, std::to_string(c->p(1).health));
}

TEST(the_aim_help_bends_a_granted_players_rail_onto_a_head_it_passes_over) {
  // Ana's slug passes some 20 over Ben's head at his distance: a miss.
  auto a = facing(300);
  give(*a, sv::SqWeapon::railgun, 10);
  run(*a, 8, sh::kButtonAttack, -3.6f);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 9);
  CHECK(a->p(1).health == 100);
  // Granted the aim help: the same shot is bent onto him, and counted.
  auto b = facing(300);
  b->r.user_info(0, "*aimhelp", "1");
  CHECK(b->s().sv.clients[0].aimhelp);
  give(*b, sv::SqWeapon::railgun, 10);
  run(*b, 8, sh::kButtonAttack, -3.6f);
  CHECK_MSG(b->p(1).health < 100, std::to_string(b->p(1).health));
  CHECK(b->s().sv.clients[0].aim_helped == 1);
  // No vertical leeway: missed again.
  auto c = facing(300);
  c->r.user_info(0, "*aimhelp", "1");
  c->r.console("sv_aimhelp_y 0");
  give(*c, sv::SqWeapon::railgun, 10);
  run(*c, 8, sh::kButtonAttack, -3.6f);
  CHECK(c->p(1).health == 100);
}

TEST(the_aim_help_leads_a_rocket_unless_the_weapon_is_denied) {
  // Ana's rocket passes some 25 over Ben's head: it hits the far wall, and the blast is too far.
  auto a = facing(300);
  give(*a, sv::SqWeapon::rocket_launcher, 10);
  run(*a, 8, sh::kButtonAttack, -4.0f);
  run(*a, 120);
  CHECK(a->p(1).health == 100);
  auto b = facing(300);
  b->r.user_info(0, "*aimhelp", "1");
  give(*b, sv::SqWeapon::rocket_launcher, 10);
  run(*b, 8, sh::kButtonAttack, -4.0f);
  run(*b, 120);
  CHECK_MSG(b->p(1).health < 100, std::to_string(b->p(1).health));
  CHECK(b->s().sv.clients[0].aim_helped == 1);
  // The server denies the help to the rocket launcher: missed again.
  auto c = facing(300);
  c->r.user_info(0, "*aimhelp", "1");
  c->r.console("sv_aimhelp_deny \"rl lg\"");
  give(*c, sv::SqWeapon::rocket_launcher, 10);
  run(*c, 8, sh::kButtonAttack, -4.0f);
  run(*c, 120);
  CHECK(c->p(1).health == 100);
  CHECK(c->s().sv.clients[0].aim_helped == 0);
}

TEST(a_rocket_flies_and_hits_for_its_direct_damage) {
  auto a = facing(300);
  a->p(1).health = 500;  // to read the whole blow
  give(*a, sv::SqWeapon::rocket_launcher, 5);
  run(*a, 8, sh::kButtonAttack);
  int flying = 0;
  for (const sv::Q2Missile& m : a->s().missiles) flying += m.live ? 1 : 0;
  CHECK(flying == 1);
  // The world names it a rocket in its row, as every runner names its shots.
  int named = 0;
  for (const sv::WorldEntity& e : a->s().world.entities)
    named += !e.free && e.shot == sv::WorldShot::rocket ? 1 : 0;
  CHECK(named == 1);
  run(*a, 100);  // 600 units at 650 units/s
  for (const sv::Q2Missile& m : a->s().missiles) CHECK(!m.live);
  const float lost = 500 - a->p(1).health;
  CHECK_MSG(lost >= 100 && lost <= 120, std::to_string(lost));  // the blast leaves him out
}

// At `sv_fps` 40 a rocket flies in four steps a tenth: it is where the 10 Hz one is at each tenth,
// and in between it is seen part of the way; it hits as hard, and the dead respawn as before.
TEST(at_sv_fps_40_a_rocket_steps_four_times_a_tenth_and_hits_as_at_10) {
  auto slow = facing(300), fast = facing(300, "1", 40);
  CHECK(fast->s().fps == 40);
  for (Arena* a : {slow.get(), fast.get()}) {
    a->p(1).health = 500;
    give(*a, sv::SqWeapon::rocket_launcher, 5);
    run(*a, 8, sh::kButtonAttack);
  }
  const auto rocket_x = [](Arena& a) {
    for (const sv::Q2Missile& m : a.s().missiles)
      if (m.live) return m.origin.x;
    return -9999.0f;
  };
  CHECK(slow->s().frame_number == fast->s().frame_number);
  int moved_between = 0;
  for (int t = 0; t < 40; t++) {
    const float before = rocket_x(*fast);
    run(*slow, 1);
    run(*fast, 1);
    if (fast->s().sub != 0 && rocket_x(*fast) != before) moved_between++;
    if (slow->s().frame_number != fast->s().frame_number || fast->s().sub != 0) continue;
    const float d = rocket_x(*slow) - rocket_x(*fast);
    CHECK_MSG(d > -0.5f && d < 0.5f,
              std::to_string(rocket_x(*slow)) + " " + std::to_string(rocket_x(*fast)));
  }
  CHECK_MSG(moved_between > 0, "the rocket never moved between tenths");
  run(*slow, 100);
  run(*fast, 100);
  CHECK(slow->p(1).health == fast->p(1).health);
  run(*fast, 8 * 60);
  CHECK(fast->r.client_command(0, "kill") == sv::CommandResult::mod);
  run(*fast, 8 * 12);
  run(*fast, 8, sh::kButtonAttack);
  CHECK(fast->p(0).alive && fast->p(0).health == 100);
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
  CHECK(a->r->world.models.back() == sv::kQ2PlayerModel);
}

TEST(kill_scores_minus_one_and_the_dead_respawn_by_attack) {
  auto a = facing(300);
  run(*a, 8 * 60);  // past the 5 s `kill` waits
  CHECK(a->r.client_command(0, "kill") == sv::CommandResult::mod);
  CHECK(!a->p(0).alive && a->p(0).frags == -1);
  CHECK(a->s().obituaries.back() == "Ana suicides.");
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

TEST(power_armour_that_takes_a_blow_shows_its_plate_or_shell_for_a_fifth_of_a_second) {
  // A shield's hit: a green shell (the colour shell, the green render bit), its pale blue specks
  // and the hit's sound, for 0.2 s.
  auto a = facing(300);
  a->p(1).body.counts[sv::sq_item("item_power_shield")] = 1;
  a->p(1).body.ammo[int(sv::SqAmmo::cells)] = 100;
  a->r.client_command(1, "use Power Shield");
  give(*a, sv::SqWeapon::railgun, 10);
  const auto ben = [&] { return a->s().world.entities[2]; };
  CHECK((ben().effects & 256) == 0);
  bool sparks = false, heard = false;
  for (int i = 0; i < 16 && a->p(1).health == 100; i++) {
    const int64_t before = a->s().out.events_total;
    run(*a, 1, sh::kButtonAttack);
    const std::vector<sv::NetEvent>& ev = a->s().out.events;
    const int64_t first = a->s().out.events_total - static_cast<int64_t>(ev.size());
    for (size_t k = static_cast<size_t>(std::max<int64_t>(0, before - first)); k < ev.size(); k++) {
      sparks = sparks || ev[k].text == "te_shield_sparks";
      heard = heard || ev[k].text == "weapons/lashit.wav";
    }
  }
  CHECK(a->p(1).health < 100 && sparks && heard);
  CHECK_MSG(a->p(1).power_flash > a->s().level_time, std::to_string(a->p(1).power_flash));
  run(*a, 1);
  CHECK((ben().effects & 256) != 0 && (ben().render_fx & 2048) != 0 && (ben().effects & 512) == 0);
  run(*a, 3 * 8);
  CHECK((ben().effects & 256) == 0 && ben().render_fx == 0);
  // A screen's hit from in front: its plate (the screen bit), green specks.
  auto b = facing(300);
  b->p(1).body.counts[sv::sq_item("item_power_screen")] = 1;
  b->p(1).body.ammo[int(sv::SqAmmo::cells)] = 100;
  b->r.client_command(1, "use Power Screen");
  b->p(1).view_angles.y = 180;  // facing the blow along +x
  CHECK(sv::fight_power_save(b->s(), 1, {1, 0, 0}, 30) == 10);
  CHECK(b->p(1).body.ammo[int(sv::SqAmmo::cells)] == 90);
  run(*b, 1);
  CHECK((b->s().world.entities[2].effects & 512) != 0);
  CHECK((b->s().world.entities[2].effects & 256) == 0);
  run(*b, 3 * 8);
  CHECK((b->s().world.entities[2].effects & 512) == 0);
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
  CHECK(!a->s().obituaries.empty() &&
        a->s().obituaries.back() == "Ben does a back flip into the lava.");
  CHECK(a->p(1).frags == -1);
}

TEST(the_players_own_sounds_are_listed_and_a_fall_and_a_jump_cry_out) {
  auto a = facing(300);
  for (const char* name : {"*pain25_1.wav", "*pain100_2.wav", "*death4.wav", "*fall1.wav",
                           "*jump1.wav", "*gurp2.wav", "player/watr_in.wav", "player/land1.wav"})
    CHECK_MSG(std::find(a->s().sounds.begin(), a->s().sounds.end(), name) != a->s().sounds.end(),
              name);
  // Down 600 units: a landing past 55, the far cry on the auto channel, and no pain cry over it.
  a->p(0).move.origin[2] = static_cast<int16_t>((24 + 600) * 8);
  std::vector<Heard> h = listen(*a, 8 * 20);
  CHECK(count(h, 1, "*fall1.wav") == 1 && count(h, 1, "*pain") == 0);
  for (const Heard& x : h)
    if (x.name == "*fall1.wav") CHECK(x.channel == 0);
  // A jump from the floor: the jump cry once, in the voice.
  h = listen(*a, 8 * 4, sh::kButtonJump);
  CHECK(count(h, 1, "*jump1.wav") == 1);
  for (const Heard& x : h)
    if (x.name == "*jump1.wav") CHECK(x.channel == 2);
}

TEST(running_steps_once_a_stride_and_walking_is_quiet) {
  // Ana runs down the hall: past 225 units/s on the floor a step each time the stride's clock comes
  // round (0.25 a 0.1 s frame: one each 0.4 s), on the body channel; walking slower, none.
  const auto stride = [](int16_t forward, const char* footsteps = "1") {
    auto a = facing(900);
    a->r.console(std::string("g_footsteps ") + footsteps);
    std::vector<Heard> h;
    for (int i = 0; i < 154; i++) {  // 2 s
      const int64_t before = a->s().out.events_total;
      sh::Packet p[2];
      for (int c = 0; c < 2; c++) {
        p[c].client = c;
        p[c].sequence = ++a->sequence;
        sh::MoveCmd cmd;
        cmd.msec = 13;
        if (c == 0) cmd.forward = forward;
        p[c].cmds[0] = p[c].cmds[1] = p[c].cmds[2] = cmd;
      }
      a->r.frame(p, 2);
      const std::vector<sv::NetEvent>& ev = a->s().out.events;
      const int64_t first = a->s().out.events_total - static_cast<int64_t>(ev.size());
      for (size_t k = static_cast<size_t>(std::max<int64_t>(0, before - first)); k < ev.size(); k++)
        if (ev[k].kind == sv::EventKind::sound) h.push_back({ev[k].ent, ev[k].level, ev[k].text});
    }
    for (const Heard& x : h)
      if (x.name.rfind("player/step", 0) == 0) CHECK(x.ent == 1 && x.channel == 4);
    return count(h, 1, "player/step");
  };
  const int run_steps = stride(400);
  CHECK_MSG(run_steps >= 3 && run_steps <= 6, std::to_string(run_steps));
  CHECK_MSG(stride(150) == 0, "walking");
  // g_footsteps: 0 none at all; 2 a step each stride at any pace on the ground.
  CHECK_MSG(stride(400, "0") == 0, "g_footsteps 0");
  const int walk_steps = stride(150, "2");
  CHECK_MSG(walk_steps >= 1, std::to_string(walk_steps));
}

TEST(lava_says_so_burns_aloud_and_its_death_cries) {
  auto a = facing(300);
  place(*a, 1, 850);  // into the lava pool
  std::vector<Heard> h = listen(*a, 8 * 3);
  CHECK(count(h, 2, "player/lava_in.wav") == 1);
  CHECK(count(h, 2, "player/burn") == 1);  // then not again for a second
  h = listen(*a, 8 * 40);
  CHECK(!a->p(1).alive);
  CHECK(count(h, 2, "player/burn") >= 1 && count(h, 2, "*death") == 1);  // a second later
}

TEST(a_blow_cries_out_once_by_the_health_left) {
  auto a = facing(-200);
  a->p(1).health = 60;
  std::vector<Heard> h = listen(*a, 8 * 10, sh::kButtonAttack);  // blaster bolts at Ben
  CHECK_MSG(a->p(1).health < 60, std::to_string(a->p(1).health));
  CHECK(count(h, 2, "*pain") >= 1 && count(h, 2, "*pain100") == 0);
  CHECK(count(h, 2, "*pain") <= 2);  // at most one cry each 0.75 s (the bolts come faster)
}

TEST(the_frag_limit_ends_the_level_after_the_intermission) {
  auto a = facing(300);
  a->r.console("fraglimit 1");
  give(*a, sv::SqWeapon::railgun, 10);
  a->p(1).health = 50;
  run(*a, 8, sh::kButtonAttack);
  CHECK(a->p(0).frags == 1);
  // The intermission: Ben stands up again, both frozen at the start with no weapon shown.
  CHECK(a->s().intermission_at > 0 && a->s().next_map == "hall");
  CHECK(a->p(1).alive && a->r->pending_map.empty());
  CHECK(a->r->world.clients[0].numbers.weapon_model == 0);
  CHECK(a->p(0).move.origin[0] == -300 * 8 && a->p(1).move.origin[0] == -300 * 8);
  run(*a, 8 * 40);  // 4 s more, no button: nothing
  CHECK(a->r->pending_map.empty());
  run(*a, 8 * 10, sh::kButtonAttack);  // past 5 s, a button leaves
  CHECK(a->r->pending_map == "hall");
}

TEST(a_hand_grenade_held_longer_flies_faster_and_held_to_the_end_explodes_in_the_hand) {
  auto a = facing(900);
  give(*a, sv::SqWeapon::hand_grenade, 5);
  bool found = false;
  run(*a, 8, sh::kButtonAttack);  // a tap: thrown when the fuse is lit, at the least speed
  run(*a, 8 * 11);
  const sv::Q2Missile* m = thrown(a->s(), found);
  CHECK(found && a->p(0).body.ammo[int(sv::SqAmmo::grenades)] == 4);
  const float slow = found ? m->velocity.x : 0;
  CHECK_MSG(slow > 395 && slow < 405, std::to_string(slow));
  run(*a, 8 * 40);  // its fuse ends, the hand is ready again
  CHECK(a->p(0).hand.state == sv::SqHandState::ready);
  run(*a, 8 * 11 + 8 * 20, sh::kButtonAttack);  // lit, then held 2 s more
  run(*a, 16);  // let go: the frame after the last one held throws it
  m = thrown(a->s(), found);
  const float fast = found ? m->velocity.x : 0;
  CHECK_MSG(fast > 640 && fast < 700, std::to_string(fast));
  run(*a, 8 * 40);
  // Held to the end of the fuse: it goes off in her hand, half its blow to her.
  a->p(0).health = 30;
  run(*a, 8 * 45, sh::kButtonAttack);  // 1.1 s to light it, 3.2 s to its end
  CHECK(!a->p(0).alive && a->p(0).frags == -1);
  CHECK(!a->s().obituaries.empty() &&
        a->s().obituaries.back() == "Ana tried to put the pin back in.");
}

// Quake's `sv_dropquad` drops the quad too, without the dmflags bit; neither: kept.
TEST(sv_dropquad_drops_the_quad_as_dmflags_16384_does) {
  for (const char* line : {"sv_dropquad 1", "sv_dropquad 0"}) {
    auto a = facing(300);
    a->r.console(line);
    give(*a, sv::SqWeapon::railgun, 10);
    sv::Q2Player& ben = a->p(1);
    run(*a, 100);
    ben.quad_until = a->s().level_time + 20;
    ben.health = 10;
    run(*a, 8, sh::kButtonAttack);
    CHECK(!ben.alive);
    bool quad = false;
    for (int k = 0; k < sv::kQ2Drops; k++)
      quad = quad || (a->s().drops[k].live && a->s().drops[k].item == sv::sq_item("item_quad"));
    CHECK_MSG(quad == (std::string(line) == "sv_dropquad 1"), line);
  }
}

// The other timed powerups' drop settings: each left with its time (the silencer its shots), and
// the one who takes it has that much at once; off, nothing is left.
TEST(the_powerup_drop_settings_leave_each_with_its_time) {
  for (const bool on : {true, false}) {
    auto a = facing(300);
    for (const char* name : {"sv_droppent", "sv_dropsuit", "sv_dropbreather", "sv_dropsilencer"})
      a->r.console(std::string(name) + (on ? " 1" : " 0"));
    give(*a, sv::SqWeapon::railgun, 10);
    sv::Q2Player& ben = a->p(1);
    run(*a, 100);
    const double now = a->s().level_time;
    ben.suit_until = now + 12;
    ben.breather_until = now + 8;
    ben.silencer_shots = 17;
    ben.health = 10;
    run(*a, 8, sh::kButtonAttack);
    CHECK(!ben.alive);
    int dropped = 0;
    for (int k = 0; k < sv::kQ2Drops; k++) {
      const sv::Q2Drop& d = a->s().drops[k];
      if (!d.live) continue;
      if (d.item == sv::sq_item("item_enviro")) dropped++, CHECK(d.power_left > 11);
      if (d.item == sv::sq_item("item_breather")) dropped++, CHECK(d.power_left > 7);
      if (d.item == sv::sq_item("item_silencer")) dropped++, CHECK(d.shots == 17);
    }
    CHECK_MSG(dropped == (on ? 3 : 0), std::to_string(dropped));
    if (!on) continue;
    run(*a, 8 * 20);  // they fall to rest
    sv::Q2Player& ana = a->p(0);
    for (int k = 0; k < sv::kQ2Drops; k++) {
      const sv::Q2Drop& d = a->s().drops[k];
      if (!d.live) continue;
      int16_t* o = ana.move.origin;
      o[0] = static_cast<int16_t>(d.origin.x * 8);
      o[1] = static_cast<int16_t>(d.origin.y * 8);
      run(*a, 2);
    }
    CHECK_MSG(ana.suit_until > a->s().level_time + 9, std::to_string(ana.suit_until));
    CHECK(ana.breather_until > a->s().level_time + 4);
    CHECK(ana.silencer_shots == 17);
  }
}

TEST(a_death_drops_the_weapon_and_the_quad_and_a_big_blow_gibs) {
  auto a = facing(300);
  a->r.console("dmflags 16384");  // the quad is dropped at death
  give(*a, sv::SqWeapon::railgun, 10);
  sv::Q2Player& ben = a->p(1);
  const int rl = sv::sq_item("weapon_rocketlauncher");
  ben.body.counts[rl] = 1;
  ben.body.ammo[int(sv::SqAmmo::rockets)] = 5;
  CHECK(a->r.client_command(1, "use Rocket Launcher") == sv::CommandResult::mod);
  run(*a, 100);
  ben.quad_until = a->s().level_time + 20;
  ben.health = 10;
  run(*a, 8, sh::kButtonAttack);  // the rail: 100 on 10 health leaves −90
  CHECK(!ben.alive && ben.gibbed);
  // His body is gone: his row shows his head (q2_gibs.hpp).
  CHECK(a->r->world.entities[2].modelindex == a->s().gib_models[ben.skull ? 6 : 5]);
  int weapon = -1, quad = -1;
  for (int k = 0; k < sv::kQ2Drops; k++) {
    const sv::Q2Drop& d = a->s().drops[k];
    if (d.live && d.item == rl) weapon = k;
    if (d.live && d.item == sv::sq_item("item_quad")) quad = k;
  }
  CHECK(weapon >= 0 && quad >= 0);
  if (weapon < 0 || quad < 0) return;
  CHECK(a->s().drops[quad].power_left > 19);
  run(*a, 8 * 20);  // they fall to rest on the floor
  const sv::Q2Drop& d = a->s().drops[quad];
  CHECK_MSG(d.resting && d.origin.z > 14 && d.origin.z < 16, std::to_string(d.origin.z));
  // Ana walks onto them: the quad is used at once with the time it had left.
  for (int k : {weapon, quad}) {
    int16_t* o = a->p(0).move.origin;
    o[0] = static_cast<int16_t>(a->s().drops[k].origin.x * 8);
    o[1] = static_cast<int16_t>(a->s().drops[k].origin.y * 8);
    run(*a, 2);
  }
  CHECK(!a->s().drops[weapon].live && !a->s().drops[quad].live);
  CHECK(a->p(0).body.counts[rl] == 1);
  const double left = a->p(0).quad_until - a->s().level_time;
  CHECK_MSG(left > 19 && left <= 20, std::to_string(left));
}

TEST(drop_throws_a_weapon_but_not_the_one_in_hand_nor_while_weapons_stay) {
  auto a = facing(300);
  give(*a, sv::SqWeapon::railgun, 10);
  const int sg = sv::sq_item("weapon_shotgun");
  a->p(0).body.counts[sg] = 1;
  a->r.client_command(0, "drop Railgun");
  CHECK(a->p(0).body.counts[sv::sq_item("weapon_railgun")] == 1);  // in hand: refused
  a->r.console("dmflags 4");
  a->r.client_command(0, "drop Shotgun");
  CHECK(a->p(0).body.counts[sg] == 1);  // weapons stay: refused
  a->r.console("dmflags 0");
  a->r.client_command(0, "drop Shotgun");
  CHECK(a->p(0).body.counts[sg] == 0);
  a->r.client_command(0, "drop Slugs");
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 0);
  int live = 0;
  for (const sv::Q2Drop& d : a->s().drops) live += d.live ? 1 : 0;
  CHECK(live == 2);
  run(*a, 8 * 3);  // her own drop is not hers again within the second
  for (const sv::Q2Drop& d : a->s().drops) CHECK(!d.live || d.owner == 0);
}

TEST(a_weapon_stays_when_taken_and_infinite_ammo_fills_the_stock) {
  auto a = arena("1", "8196");  // weapons stay (4) and infinite ammo (8192)
  run(*a, 80);
  place(*a, 0, -260);  // onto the railgun
  run(*a, 8);
  const int rail = sv::sq_item("weapon_railgun");
  CHECK(a->p(0).body.counts[rail] == 1);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 50);  // to the carry limit
  const sv::Q2Entity& e = a->s().entities[1];
  CHECK(e.item == rail && !e.hidden && !e.gone);  // it stays for the next player
  run(*a, 8 * 20, sh::kButtonAttack);             // and her shots take none
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::slugs)] == 50);
}

TEST(teammates_by_skin_take_no_damage_without_friendly_fire_and_a_team_kill_scores_minus_one) {
  auto a = facing(300);
  a->r.user_info(0, "skin", "male/red");
  a->r.user_info(1, "skin", "female/red");
  a->r.console("dmflags 320");  // skin teams, no friendly fire
  CHECK(sv::fight_teammates(a->s(), 0, 1));
  give(*a, sv::SqWeapon::railgun, 10);
  run(*a, 8, sh::kButtonAttack);
  CHECK(a->p(1).alive && a->p(1).health == 100);
  a->r.console("dmflags 128");  // model teams: male and female are not one team
  CHECK(!sv::fight_teammates(a->s(), 0, 1));
  a->r.console("dmflags 64");  // skin teams with friendly fire
  a->p(1).health = 10;
  run(*a, 16 * 8, sh::kButtonAttack);
  CHECK(!a->p(1).alive && a->p(0).frags == -1);
}

TEST(the_field_of_view_is_held_to_its_range_and_fixed_by_the_flag) {
  auto a = arena();
  run(*a, 8);
  CHECK(a->r->world.clients[0].numbers.fov == 90);
  a->r.user_info(0, "fov", "200");
  CHECK(a->r->world.clients[0].numbers.fov == 160);
  a->r.user_info(0, "fov", "110");
  CHECK(a->r->world.clients[0].numbers.fov == 110);
  a->r.console("dmflags 32768");
  run(*a, 8);
  CHECK(a->r->world.clients[0].numbers.fov == 90);
}

TEST(a_respawn_leaves_the_body_which_a_blast_throws_apart) {
  auto a = facing(300);
  a->r.console("dmflags 1024");  // the dead respawn at once
  give(*a, sv::SqWeapon::railgun, 10);
  a->p(1).health = 80;
  run(*a, 8, sh::kButtonAttack);  // 100 on 80: −20, a body knocked back, not a gib
  CHECK(!a->p(1).alive && !a->p(1).gibbed);
  run(*a, 8 * 12);  // a second on: he stands up elsewhere
  CHECK(a->p(1).alive);
  const sv::Q2Body& b = a->s().bodies[0];
  CHECK_MSG(b.live && b.origin.x > 300 && b.health == -20,
            std::to_string(b.origin.x) + " " + std::to_string(b.health));
  const size_t row = sv::fight_first_row(a->s()) + sv::kQ2Missiles + sv::kQ2Drops;
  CHECK(!a->r->world.entities[row].free && a->r->world.entities[row].modelindex > 0);
  sv::fight_blast(a->s(), b.origin, 0, 100, 120, int(sv::SqWeapon::rocket_launcher));
  CHECK(!b.live);
  run(*a, 1);
  CHECK(a->r->world.entities[row].free);
}

TEST(the_scores_list_the_best_first) {
  auto a = arena();
  run(*a, 8);
  a->p(1).frags = 3;
  a->p(0).frags = -1;
  const std::vector<std::string> lines = sv::fight_scores(a->s());
  CHECK(lines.size() == 2 && lines[0] == "   3  Ben" && lines[1] == "  -1  Ana");
  CHECK(a->r.client_command(0, "score") == sv::CommandResult::mod);
}

// Outside deathmatch the machine gun's view climbs 1.5 degrees a bullet after the first, to 13.5,
// and its bullets climb with it; deathmatch and `g_machinegun_norecoil 1` keep it level.
TEST(the_machine_gun_climbs_outside_deathmatch_unless_g_machinegun_norecoil) {
  const auto climbed = [](const char* deathmatch, const char* norecoil) {
    auto a = facing(200, deathmatch);
    a->r.console(std::string("g_machinegun_norecoil ") + norecoil);
    const sv::SqWeaponRow& mg = sv::kSqWeapons[int(sv::SqWeapon::machinegun)];
    a->p(0).body.counts[sv::sq_item(mg.item)] = 1;
    a->p(0).body.ammo[int(sv::SqAmmo::bullets)] = 50;
    run(*a, 100, 0, 0, 4);  // impulse 4: the machinegun
    CHECK(a->p(0).hand.weapon == int(sv::SqWeapon::machinegun));
    float most = 0;
    for (int i = 0; i < 160; i++) {  // 2 s of fire
      run(*a, 1, sh::kButtonAttack);
      most = std::min(most, a->p(0).seen.kick.x);
    }
    return most;
  };
  const float single = climbed("0", "0");
  CHECK_MSG(single < -13.4f && single > -13.6f, std::to_string(single));
  CHECK_MSG(climbed("1", "0") == 0, "deathmatch: level");
  CHECK_MSG(climbed("0", "1") == 0, "g_machinegun_norecoil 1: level");
}

// `aimfix 1`: a rocket flies from the gun onto the point the eye looks at (Ben's body), not
// straight ahead from the gun, which sits 8 to the right and 8 below the eye.
TEST(aimfix_turns_a_shot_from_the_gun_onto_the_point_the_eye_looks_at) {
  const auto rocket = [](const char* fix) {
    auto a = facing(200);
    a->r.console(std::string("aimfix ") + fix);
    give(*a, sv::SqWeapon::rocket_launcher, 5);
    run(*a, 1, sh::kButtonAttack);
    for (const sv::Q2Missile& m : a->s().missiles)
      if (m.live) return m.velocity;
    CHECK_MSG(false, "no rocket in flight");
    return sh::Vec3f{};
  };
  const sh::Vec3f straight = rocket("0"), aimed = rocket("1");
  CHECK(straight.y == 0 && straight.z == 0 && straight.x > 0);
  // The gun is on the right (its y below the eye's), so the aim turns left (+y) and up a little.
  CHECK_MSG(aimed.x > 0 && aimed.y > 0 && aimed.z > 0,
            std::to_string(aimed.y) + " " + std::to_string(aimed.z));
}

// `sv_maxvelocity` holds a thrown thing's speed along each axis.
TEST(sv_maxvelocity_holds_each_axis_of_a_thrown_things_speed) {
  auto a = arena();
  a->r.console("sv_maxvelocity 100");
  sh::Vec3f v{300, -300, 50};
  sv::q2_hold_speed(a->s(), v);
  CHECK(v.x == 100 && v.y == -100 && v.z == 50);
}

// Others see a strafing player's figure lean into the strafe: four times `sv_rollangle` at
// `sv_rollspeed` sideways; its pitch a third of the view's.
TEST(a_strafing_figure_leans_by_sv_rollangle_and_pitches_a_third_of_the_view) {
  const auto lean = [](const char* angle) {
    auto a = facing(900);
    a->r.console(std::string("sv_rollangle ") + angle);
    for (int i = 0; i < 40; i++) {
      sh::Packet p[2];
      for (int c = 0; c < 2; c++) {
        p[c].client = c;
        p[c].sequence = ++a->sequence;
        sh::MoveCmd cmd;
        cmd.msec = 13;
        if (c == 0) {
          cmd.side = 400;  // to the right
          cmd.angles = {30, 0, 0};
        }
        p[c].cmds[0] = p[c].cmds[1] = p[c].cmds[2] = cmd;
      }
      a->r.frame(p, 2);
    }
    const sh::WorldEntity& e = a->s().world.entities[1];
    CHECK_MSG(e.angles.x > 9.9f && e.angles.x < 10.1f, std::to_string(e.angles.x));
    return e.angles.z;
  };
  const float two = lean("2");
  CHECK_MSG(two > 7.9f && two < 8.1f, std::to_string(two));  // past 200 sideways: the full 8
  CHECK(lean("0") == 0);
}

TEST(the_screen_takes_the_quad_tint_and_a_blow_flashes_red_then_fades) {
  auto a = arena();
  run(*a, 10);
  sv::Q2Player& ana = a->p(0);
  CHECK(ana.blend[3] == 0);
  ana.quad_until = a->s().level_time + 20;
  run(*a, 8);
  CHECK(ana.blend[2] == 1 && ana.blend[3] > 0.079f && ana.blend[3] < 0.081f);
  ana.quad_until = 0;
  sv::fight_hurt(a->s(), 0, 1, {1, 0, 0}, 30, 0, 0);  // all of it to his health: red
  run(*a, 8);
  CHECK_MSG(ana.blend[0] > 0.99f && ana.blend[1] < 0.01f && ana.blend[3] > 0.29f,
            std::to_string(ana.blend[3]));
  const float first = ana.blend[3];
  run(*a, 8 * 3);
  CHECK(ana.blend[3] < first);
  run(*a, 8 * 20);
  CHECK(ana.blend[3] == 0);  // faded out
  const auto& n = a->s().world.clients[0].numbers;
  CHECK(n.blend[3] == 0);
}

TEST(using_quad_says_so_and_power_armour_with_no_cells_says_why) {
  auto a = arena("0");
  run(*a, 10);
  const auto heard = [&](const char* sample, int64_t from) {
    const auto& ev = a->s().out.events;
    for (size_t k = 0; k < ev.size(); k++)
      if (a->s().out.events_total - int64_t(ev.size() - k) >= from && ev[k].text == sample)
        return true;
    return false;
  };
  int64_t from = a->s().out.events_total;
  a->p(0).body.counts[sv::sq_item("item_quad")] = 1;
  CHECK(sv::fight_use(a->s(), 0, sv::sq_item("item_quad")));
  CHECK(heard("items/damage.wav", from));
  from = a->s().out.events_total;
  const int screen = sv::sq_item("item_power_screen");
  a->p(0).body.counts[screen] = 1;
  a->p(0).body.ammo[static_cast<int>(sv::SqAmmo::cells)] = 0;
  CHECK(sv::fight_use(a->s(), 0, screen) && !a->p(0).power_on);
  CHECK(heard("No cells for power armor.\n", from));
  a->p(0).body.ammo[static_cast<int>(sv::SqAmmo::cells)] = 50;
  from = a->s().out.events_total;
  CHECK(sv::fight_use(a->s(), 0, screen) && a->p(0).power_on);
  CHECK(heard("misc/power1.wav", from));
  // Run dry, it switches itself off with its sound, and stays off when cells come back.
  from = a->s().out.events_total;
  a->p(0).body.ammo[static_cast<int>(sv::SqAmmo::cells)] = 0;
  run(*a, 8);
  CHECK(!a->p(0).power_on);
  CHECK(heard("misc/power2.wav", from));
  a->p(0).body.ammo[static_cast<int>(sv::SqAmmo::cells)] = 50;
  run(*a, 8);
  CHECK(!a->p(0).power_on);
}

// The obituaries' words: a direct hit and a blast apart, the BFG's three, the victim's gender in
// his own deaths, the world's causes whoever struck, and a killer's line with no full stop.
TEST(the_obituaries_word_each_way_to_die) {
  const int rl = int(sv::SqWeapon::rocket_launcher), bfg = int(sv::SqWeapon::bfg);
  bool by = false;
  CHECK(sv::sq_obituary("Ben", "Ana", false, rl, sv::kSqHowDirect, "", &by) ==
            "Ben ate Ana's rocket" &&
        by);
  CHECK(sv::sq_obituary("Ben", "Ana", false, rl, sv::kSqHowBlast, "") ==
        "Ben almost dodged Ana's rocket");
  CHECK(sv::sq_obituary("Ben", "Ana", false, bfg, sv::kSqHowLaser, "") ==
        "Ben saw the pretty lights from Ana's BFG");
  CHECK(sv::sq_obituary("Ben", "Ana", false, bfg, sv::kSqHowBurst, "") ==
        "Ben couldn't hide from Ana's BFG");
  CHECK(sv::sq_obituary("Ben", "Ana", false, sv::kSqByTelefrag, 0, "") ==
        "Ben tried to invade Ana's personal space");
  CHECK(sv::sq_obituary("Ana", "", true, rl, sv::kSqHowBlast, "female") == "Ana blew herself up.");
  CHECK(sv::sq_obituary("Ana", "", true, int(sv::SqWeapon::grenade_launcher), sv::kSqHowBlast,
                        "male") == "Ana tripped on his own grenade.");
  CHECK(sv::sq_obituary("Ana", "", true, int(sv::SqWeapon::railgun), 0, "cyborg") ==
        "Ana killed itself.");
  CHECK(sv::sq_obituary("Ben", "Ana", false, sv::kSqByFall, 0, "", &by) == "Ben cratered." && !by);
  CHECK(sv::sq_obituary("Ben", "", false, sv::kSqByMonster + 3, 0, "") == "Ben died.");
}

// Single player says only that he died.
TEST(in_single_player_a_death_says_only_died) {
  auto a = facing(300, "0");
  run(*a, 8 * 60);  // past the 5 s `kill` waits
  a->r.client_command(0, "kill");
  CHECK(!a->p(0).alive);
  CHECK_MSG(a->s().obituaries.back() == "Ana died.", a->s().obituaries.back());
}

namespace {

// Whether a print of `text` to everyone at the high level is in the events not yet sent.
bool said_to_all(const sv::Q2Server& s, const std::string& text) {
  for (const sv::NetEvent& e : s.out.events)
    if (e.kind == sv::EventKind::print && e.text == text && e.level == 2 && e.to == 0xffffffffu)
      return true;
  return false;
}

}  // namespace

// In deathmatch each player placed (on joining and on every new map) is said to everyone, and so is
// one leaving; outside deathmatch only a server with room for more than one says it.
TEST(entering_and_leaving_are_said_to_everyone) {
  auto a = arena("1");
  CHECK(said_to_all(a->s(), "Ana entered the game\n"));
  CHECK(said_to_all(a->s(), "Ben entered the game\n"));
  a->r.drop(1);
  CHECK(said_to_all(a->s(), "Ben disconnected\n"));
  for (const int most : {1, 4}) {
    auto g = std::make_unique<Arena>();
    g->r = sv::q2_runner(sv::make_q2_server({}));
    g->r.console("deathmatch 0");
    g->s().sv.set.maxclients = most;
    sv::MapSpec map;
    map.name = "hall";
    map.brushes = hall();
    std::string error;
    CHECK_MSG(g->r.start(map, error), error);
    g->r.connect(0, "Ana", false, false);
    CHECK(said_to_all(g->s(), "Ana entered the game\n") == (most > 1));
  }
}
