// Tests: The Reckoning's rules beside Quake II's (games/shooters_edge/server/q2_reckoning.hpp):
// its items in their list's numbers and its ammunition's limits; the ion ripper's and the
// phalanx's frames, ammunition and periods; dual fire stepping the hand twice; the obituaries; on
// a hall played under the pack, the ion ripper's bolt bouncing off the wall until its 3 s end, the
// phalanx's two balls, the trap pulling a player in and killing him past invulnerability, and the
// `use hyperblaster` and `use railgun` toggles; the base game left as it was.
//
// ge:summary Tests: The Reckoning's items, weapons, dual fire, trap, toggles and words
// ge:keywords test q2 reckoning ion-ripper phalanx trap dual-fire mag-slug item-number toggle
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_reckoning_test.cpp
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/item_roles.hpp"
#include "shooters_edge/profiles/remix.hpp"
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_pack.hpp"
#include "shooters_edge/server/q2_powers.hpp"
#include "shooters_edge/server/q2_reckoning.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
using sv::Q2Pack;
using sv::SqAmmo;
using sv::SqWeapon;

constexpr Q2Pack kPack = Q2Pack::reckoning;

int ammo(const sv::SqBody& b, SqAmmo a) { return b.ammo[static_cast<int>(a)]; }

sv::SqBody holding(SqWeapon w, int count) {
  sv::SqBody b = sv::sq_new_body(kPack);
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  b.counts[sv::sq_item(r.item, kPack)] = 1;
  b.ammo[int(r.ammo)] = count;
  return b;
}

// Steps a ready hand of `w` with attack held for `steps`; the shots each step made.
std::vector<int> fire(sv::SqBody& b, SqWeapon w, int steps) {
  sv::SqHand h;
  h.weapon = int(w);
  h.state = sv::SqHandState::ready;
  h.frame = sv::kSqWeapons[int(w)].last_fire + 1;
  std::vector<int> shots;
  shots.reserve(size_t(steps));
  for (int i = 0; i < steps; i++) shots.push_back(sv::sq_hand_step(h, b, true, i * 0.1).shots);
  return shots;
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A floored hall walled at its ends, with `things` placed in it.
std::shared_ptr<const br::Map> hall(const std::vector<br::Entity>& things = {}) {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  w.entities.insert(w.entities.end(), things.begin(), things.end());
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

// Host ticks of 13 ms: Ana with `buttons`, Ben standing still.
void run(Arena& a, int ticks, uint8_t buttons = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++a.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      if (c == 0) cmd.buttons = buttons;
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

void place(Arena& a, int i, float x) {
  int32_t* o = a.p(i).move.origin;
  o[0] = static_cast<int32_t>(x * 8);
  o[1] = 0;
  for (auto& v : a.p(i).move.velocity) v = 0;
}

// Ana at x −300 looking down the hall at Ben at x `ben`, under the pack (deathmatch).
std::unique_ptr<Arena> facing(float ben, const std::vector<br::Entity>& things = {}) {
  auto a = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = kPack;
  a->r = sv::q2_runner(sv::make_q2_server(o));
  a->r.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(things);
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->p(0).move.origin[0] = static_cast<int16_t>(a->p(0).move.origin[0] - 64 * 8);
  a->r.connect(1, "Ben", false, false);
  run(*a, 80);
  place(*a, 0, -300);
  place(*a, 1, ben);
  a->p(0).body = sv::sq_new_body(kPack);
  run(*a, 20);
  return a;
}

void give(Arena& a, SqWeapon w, int count) {
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  a.p(0).body.counts[sv::sq_item(r.item, kPack)] = 1;
  a.p(0).body.ammo[int(r.ammo)] = count;
  CHECK(a.r.client_command(0, std::string("use ") + r.name) == sv::CommandResult::mod);
  run(a, 100);
  CHECK(a.p(0).hand.weapon == int(w) && a.p(0).hand.state == sv::SqHandState::ready);
}

// The map entity of `classname`; -1 none.
int thing(const sv::Q2Server& s, const std::string& classname) {
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].classname == classname) return static_cast<int>(i);
  return -1;
}

// The events of `text` the next `ticks` host ticks made.
int heard(Arena& a, int ticks, const std::string& text) {
  int n = 0;
  for (int i = 0; i < ticks; i++) {
    const int64_t before = a.s().out.events_total;
    run(a, 1);
    const std::vector<sv::NetEvent>& ev = a.s().out.events;
    const int64_t first = a.s().out.events_total - static_cast<int64_t>(ev.size());
    for (size_t k = static_cast<size_t>(std::max<int64_t>(0, before - first)); k < ev.size(); k++)
      n += ev[k].text == text ? 1 : 0;
  }
  return n;
}

int live(const sv::Q2Server& s, sv::SqShot kind) {
  int n = 0;
  for (const sv::Q2Missile& m : s.missiles) n += m.live && m.kind == kind ? 1 : 0;
  return n;
}

}  // namespace

TEST(the_pack_s_items_take_their_numbers_and_the_base_list_keeps_its_own) {
  CHECK(sv::sq_numbers(kPack) == 48);
  CHECK(sv::sq_number_of("ammo_trap", kPack) == 13);
  CHECK(sv::sq_number_of("weapon_grenadelauncher", kPack) == 14);
  CHECK(sv::sq_number_of("weapon_boomer", kPack) == 17);
  CHECK(sv::sq_number_of("weapon_phalanx", kPack) == 19);
  CHECK(sv::sq_number_of("ammo_magslug", kPack) == 26);
  CHECK(sv::sq_number_of("item_quadfire", kPack) == 28);
  CHECK(sv::sq_number_of("key_green_key", kPack) == 44);
  CHECK(sv::sq_number_of("item_health", kPack) == 47);
  CHECK(sv::sq_weapon_number(int(SqWeapon::trap), kPack) == 13);
  CHECK(sv::sq_weapon_number(int(SqWeapon::ion_ripper), kPack) == 17);
  // Not in the base game's list, nor found as its rows.
  CHECK(sv::sq_number_of("weapon_boomer") == 0);
  CHECK(sv::sq_item("weapon_boomer") < 0);
  CHECK(sv::sq_item("weapon_boomer", kPack) >= 0);
  CHECK(!sv::sq_weapon_plays(int(SqWeapon::phalanx), Q2Pack::base));
  CHECK(sv::sq_weapon_plays(int(SqWeapon::phalanx), kPack));
}

TEST(mag_slugs_and_traps_are_carried_only_under_the_pack) {
  sv::SqBody base = sv::sq_new_body();
  CHECK(base.max_ammo[int(SqAmmo::magslugs)] == 0 && base.max_ammo[int(SqAmmo::traps)] == 0);
  CHECK(sv::sq_pick_up(sv::sq_item("item_pack"), base, true, 1, 0));
  CHECK(ammo(base, SqAmmo::magslugs) == 0);  // the base game's pack gives none
  sv::SqBody b = sv::sq_new_body(kPack);
  CHECK(b.max_ammo[int(SqAmmo::magslugs)] == 50 && b.max_ammo[int(SqAmmo::traps)] == 5);
  CHECK(sv::sq_pick_up(sv::sq_item("item_bandolier", kPack), b, true, 1, 0));
  CHECK(b.max_ammo[int(SqAmmo::magslugs)] == 75 && ammo(b, SqAmmo::magslugs) == 0);
  CHECK(sv::sq_pick_up(sv::sq_item("item_pack", kPack), b, true, 1, 0));
  CHECK(b.max_ammo[int(SqAmmo::magslugs)] == 100 && ammo(b, SqAmmo::magslugs) == 10);
  CHECK(ammo(b, SqAmmo::traps) == 0 && b.max_ammo[int(SqAmmo::traps)] == 5);
  for (int i = 0; i < 7; i++) sv::sq_pick_up(sv::sq_item("ammo_trap", kPack), b, true, 1, 0);
  CHECK(ammo(b, SqAmmo::traps) == 5);
  const int cells = ammo(b, SqAmmo::cells);
  CHECK(sv::sq_pick_up(sv::sq_item("weapon_boomer", kPack), b, true, 1, 0));
  CHECK(ammo(b, SqAmmo::cells) == cells + 50);
}

TEST(the_ion_ripper_fires_every_third_step_for_two_cells) {
  sv::SqBody b = holding(SqWeapon::ion_ripper, 7);
  const std::vector<int> shots = fire(b, SqWeapon::ion_ripper, 9);
  CHECK((shots == std::vector<int>{1, 0, 0, 1, 0, 0, 1, 0, 0}));
  CHECK(ammo(b, SqAmmo::cells) == 1);  // one cell left: no more
  CHECK(!sv::sq_can_fire(b, int(SqWeapon::ion_ripper)));
}

TEST(the_phalanx_fires_two_balls_a_pull_for_one_mag_slug_every_sixteen_steps) {
  sv::SqBody b = holding(SqWeapon::phalanx, 5);
  const std::vector<int> shots = fire(b, SqWeapon::phalanx, 19);
  CHECK(shots[0] == 0 && shots[1] == 1 && shots[2] == 1);
  int total = 0;
  for (int k : shots) total += k;
  CHECK(total == 4 && shots[17] == 1 && shots[18] == 1);  // the next pull 16 steps on
  CHECK(ammo(b, SqAmmo::magslugs) == 3);
}

TEST(dual_fire_is_a_timed_power_with_the_quad_s_shell) {
  const sv::SqPower* pw = sv::sq_power("item_quadfire");
  CHECK(pw != nullptr && pw->until == &sv::Q2Player::dual_fire_until);
  if (pw == nullptr) return;
  CHECK(pw->drop_dmflag == 65536);
  sv::Q2Player p;
  p.dual_fire_until = 20;
  CHECK(sv::sq_first_power(p, 10) == pw);
  p.quad_until = 20;  // the quad first: its timer and tint win
  CHECK(std::string(sv::sq_first_power(p, 10)->item) == "item_quad");
}

TEST(the_pack_s_weapons_have_their_words) {
  bool by = false;
  CHECK(sv::sq_obituary("Ben", "Ana", false, int(SqWeapon::ion_ripper), 0, "male", &by) ==
        "Ben ripped to shreds by Ana's ripper gun");
  CHECK(by);
  CHECK(sv::sq_obituary("Ben", "Ana", false, int(SqWeapon::phalanx), 0, "male", &by) ==
        "Ben was evaporated by Ana");
  CHECK(sv::sq_obituary("Ben", "Ana", false, int(SqWeapon::trap), 0, "female", &by) ==
        "Ben caught in trap by Ana");
  CHECK(sv::sq_obituary("Ana", "Ana", true, int(SqWeapon::trap), 0, "female", &by) ==
        "Ana sucked into his own trap.");
}

TEST(the_ion_ripper_bolt_bounces_off_the_wall_until_it_sparks_out) {
  auto a = facing(900);
  a->p(1).move.origin[1] = static_cast<int16_t>(200 * 8);  // out of the line
  give(*a, SqWeapon::ion_ripper, 10);
  run(*a, 8, sh::kButtonAttack);
  CHECK(live(a->s(), sv::SqShot::ripper) == 1);
  CHECK(ammo(a->p(0).body, SqAmmo::cells) == 8);
  run(*a, 8 * 27);  // 2.8 s: 1400 units, the wall 1310 away: on its way back
  float vx = 0;
  for (const sv::Q2Missile& m : a->s().missiles)
    if (m.live && m.kind == sv::SqShot::ripper) vx = m.velocity.x;
  CHECK_MSG(vx < -499 && vx > -501, std::to_string(vx));
  run(*a, 8 * 4);  // past its 3 s
  CHECK(live(a->s(), sv::SqShot::ripper) == 0);
}

TEST(the_ion_ripper_hits_for_thirty_in_deathmatch) {
  auto a = facing(100);
  a->p(1).health = 500;
  give(*a, SqWeapon::ion_ripper, 2);
  run(*a, 8, sh::kButtonAttack);
  run(*a, 80);
  CHECK_MSG(a->p(1).health == 470, std::to_string(a->p(1).health));
}

TEST(the_phalanx_balls_burst_against_the_far_wall) {
  auto a = facing(900);
  a->p(1).health = 500;
  give(*a, SqWeapon::phalanx, 1);
  run(*a, 24, sh::kButtonAttack);
  CHECK(live(a->s(), sv::SqShot::plasma) == 2);
  CHECK(ammo(a->p(0).body, SqAmmo::magslugs) == 0);
  run(*a, 8 * 30);
  CHECK(live(a->s(), sv::SqShot::plasma) == 0);
  CHECK(a->p(1).health < 500);  // a ball, its splash or both
}

TEST(a_trap_pulls_the_nearest_in_and_kills_him_past_invulnerability) {
  auto a = facing(-100);
  give(*a, SqWeapon::trap, 2);
  run(*a, 8 * 12, sh::kButtonAttack);  // lit at frame 11
  run(*a, 8 * 2);                      // let go: thrown
  CHECK(live(a->s(), sv::SqShot::trap) == 1);
  CHECK(ammo(a->p(0).body, SqAmmo::traps) == 1);
  place(*a, 0, -900);                // Ana far off, Ben by the trap
  a->p(1).invulnerable_until = 1e6;  // no help
  run(*a, 8 * 60);
  CHECK(!a->p(1).alive);
  CHECK(!a->s().obituaries.empty() && a->s().obituaries.back() == "Ben caught in trap by Ana");
}

TEST(use_hyperblaster_and_use_railgun_toggle_to_the_pack_s_weapons) {
  auto a = facing(900);
  sv::SqBody& b = a->p(0).body;
  b.counts[sv::sq_item("weapon_hyperblaster", kPack)] = 1;
  b.counts[sv::sq_item("weapon_boomer", kPack)] = 1;
  b.ammo[int(SqAmmo::cells)] = 50;
  a->r.client_command(0, "use HyperBlaster");
  run(*a, 100);
  CHECK(a->p(0).hand.weapon == int(SqWeapon::hyperblaster));
  a->r.client_command(0, "use hyperblaster");
  run(*a, 100);
  CHECK(a->p(0).hand.weapon == int(SqWeapon::ion_ripper));
  b.counts[sv::sq_item("weapon_railgun", kPack)] = 1;
  b.counts[sv::sq_item("weapon_phalanx", kPack)] = 1;
  b.ammo[int(SqAmmo::magslugs)] = 5;
  a->r.client_command(0, "use railgun");  // no slugs: the phalanx
  run(*a, 100);
  CHECK(a->p(0).hand.weapon == int(SqWeapon::phalanx));
  // weapnext from the phalanx: the next in the pack's order that can fire (the BFG cannot): the
  // blaster, round the end.
  a->r.client_command(0, "weapnext");
  run(*a, 100);
  CHECK(a->p(0).hand.weapon == int(SqWeapon::blaster));
}

TEST(the_rotating_light_alarms_while_on_and_breaks_into_sparks) {
  auto a = facing(900, {entity({{"classname", "rotating_light"},
                                {"origin", "0 0 200"},
                                {"spawnflags", "3"},
                                {"targetname", "lamp"}})});
  const int i = thing(a->s(), "rotating_light");
  CHECK(i >= 0 && !a->s().entities[size_t(i)].on);  // starts off
  CHECK(heard(*a, 80, "misc/alarm.wav") == 0);
  CHECK(sv::reckoning_use(a->s(), i, 0));
  const int alarms = heard(*a, 8 * 31, "misc/alarm.wav");  // 3.2 s: at 0.1, 1.1, 2.1, 3.1
  CHECK_MSG(alarms == 4, std::to_string(alarms));
  CHECK(sv::reckoning_damage(a->s(), i, 10));
  CHECK(heard(*a, 16, "te_welding_sparks") == 0);  // sent with the blow itself
  CHECK(a->s().entities[size_t(i)].gone);
}

TEST(the_hum_is_heard_everywhere_every_2_7_seconds) {
  auto a = facing(900, {entity({{"classname", "misc_amb4"}, {"origin", "0 0 64"}})});
  const int hums = heard(*a, 8 * 55, "world/amb4.wav");  // 5.7 s
  CHECK_MSG(hums == 2, std::to_string(hums));
}

TEST(the_nuke_kills_every_player_once_whatever_protects_him) {
  auto a = facing(900, {entity({{"classname", "misc_nuke"}, {"targetname", "boom"}})});
  a->p(1).invulnerable_until = 1e6;
  const int i = thing(a->s(), "misc_nuke");
  CHECK(sv::reckoning_use(a->s(), i, 0));
  CHECK(!a->p(0).alive && !a->p(1).alive);
  CHECK(!a->s().entities[size_t(i)].on);  // spent
}

TEST(the_repair_object_sparks_until_welded_through_then_regrows) {
  auto a = facing(900, {entity({{"classname", "func_object_repair"}, {"origin", "0 0 64"}})});
  const int i = thing(a->s(), "object_repair");  // renamed for the repair bot to find
  CHECK(i >= 0 && a->s().entities[size_t(i)].health == 100);
  CHECK(heard(*a, 8 * 21, "te_welding_sparks") >= 1);
  CHECK(sv::reckoning_damage(a->s(), i, 105));
  run(*a, 8 * 30);
  CHECK(a->s().entities[size_t(i)].state == 2);  // its targets fired once, regrowing
  CHECK(a->s().entities[size_t(i)].health > -5);
}

TEST(the_pulsing_laser_burns_a_player_in_its_beam) {
  auto a = facing(200, {entity({{"classname", "target_mal_laser"},
                                {"origin", "100 0 40"},
                                {"angle", "0"},
                                {"spawnflags", "1"}})});
  a->p(1).health = 500;
  run(*a, 8 * 20);
  CHECK(a->p(1).health < 500);
}

TEST(a_new_game_of_the_pack_starts_on_its_own_first_map) {
  CHECK(sv::q2_pack_start_map(sv::q2_pack_of_folder("xatrix")) == "xswamp");
  CHECK(sv::q2_pack_start_map(sv::q2_pack_of_folder("rogue")) == "rmine1");
  CHECK(sv::q2_pack_start_map(sv::Q2Pack::base).empty());  // the base game's own start map
}

TEST(the_pack_s_items_play_their_parts_in_another_game) {
  namespace pf = shooters_edge::profiles;
  const ge::classic::GameItems& q2 = *pf::game_items("q2");
  const ge::classic::GameItems& q1 = *pf::game_items("q1");
  const auto as_q1 = [&](const char* c) { return ge::classic::swap_item(q2, q1, c, 0); };
  const auto like = [&](const char* pack, const char* base) {
    const ge::classic::ItemSwap a = as_q1(pack), b = as_q1(base);
    return a.known && a.becomes == b.becomes && a.bits == b.bits;
  };
  CHECK(like("weapon_boomer", "weapon_hyperblaster"));
  CHECK(like("weapon_phalanx", "weapon_rocketlauncher"));
  CHECK(like("ammo_magslug", "ammo_rockets"));
  CHECK(like("ammo_trap", "ammo_grenades"));
  CHECK(like("item_quadfire", "item_quad"));
  // Laid down in Quake II, the base game's own (its spawns are the base game's).
  CHECK(ge::classic::swap_item(q2, q2, "weapon_boomer", 0).becomes == "weapon_hyperblaster");
}

TEST(a_game_of_the_pack_saved_and_loaded_plays_on_as_the_game_never_saved) {
  const auto pack_game = [] {
    auto a = std::make_unique<Arena>();
    sv::Q2ServerOptions o;
    o.pack = kPack;
    a->r = sv::q2_runner(sv::make_q2_server(o));
    a->r.console("deathmatch 0");
    return a;
  };
  sv::MapSpec map;
  map.name = "xswamp";
  map.brushes =
      hall({entity({{"classname", "rotating_light"},
                    {"origin", "0 0 200"},
                    {"spawnflags", "2"},
                    {"targetname", "lamp"}}),
            entity({{"classname", "func_object_repair"}, {"origin", "200 100 24"}}),
            entity({{"classname", "ammo_magslug"}, {"origin", "400 0 24"}}),
            entity({{"classname", "monster_soldier"}, {"origin", "700 0 24"}, {"angle", "180"}})});
  auto g = pack_game();
  std::string error;
  CHECK_MSG(g->r.start(map, error), error);
  g->r.connect(0, "Ana", false, false);
  sh::Packet idle[1];
  const auto tick = [&idle](Arena& a, uint8_t buttons) {
    idle[0].client = 0;
    idle[0].sequence = ++a.sequence;
    sh::MoveCmd cmd;
    cmd.msec = 13;
    cmd.buttons = buttons;
    idle[0].cmds[0] = idle[0].cmds[1] = idle[0].cmds[2] = cmd;
    a.r.frame(idle, 1);
  };
  for (int i = 0; i < 80; i++) tick(*g, 0);
  sv::SqBody& b = g->p(0).body;
  b.counts[sv::sq_item("ammo_trap", kPack)] = 2;
  b.ammo[int(SqAmmo::traps)] = 2;
  b.counts[sv::sq_item("weapon_phalanx", kPack)] = 1;
  b.ammo[int(SqAmmo::magslugs)] = 7;
  g->p(0).dual_fire_until = g->s().level_time + 20;
  g->r.client_command(0, "use trap");
  for (int i = 0; i < 100; i++) tick(*g, 0);
  for (int i = 0; i < 8 * 12; i++) tick(*g, sh::kButtonAttack);
  for (int i = 0; i < 8 * 15; i++) tick(*g, 0);  // thrown, landed, opening
  CHECK(live(g->s(), sv::SqShot::trap) == 1);
  std::vector<uint8_t> file;
  CHECK_MSG(sv::q2_save(g->s(), file, error), error);
  auto h = pack_game();
  CHECK_MSG(sv::q2_load(h->r, map, file, "test", error), error);
  h->sequence = g->sequence;
  CHECK_MSG(sv::q2_digest(h->s()) == sv::q2_digest(g->s()), "the loaded game differs");
  CHECK(live(h->s(), sv::SqShot::trap) == 1);
  CHECK(h->p(0).body.ammo[int(SqAmmo::magslugs)] == 7 && h->p(0).dual_fire_until > 0);
  for (int k = 0; k < 8 * 60; k++) {
    tick(*g, 0);
    tick(*h, 0);
    if (sv::q2_digest(h->s()) != sv::q2_digest(g->s())) {
      CHECK_MSG(false, "the loaded game went its own way at tick " + std::to_string(k));
      break;
    }
  }
}
