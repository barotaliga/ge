// Tests: the sequel's weapons and damage as data (games/shooters_edge/server/sequel_combat.hpp):
// a weapon fires at its period from ready, in the same step as the press; the machinegun fires
// every step while held; the chaingun winds up from 1 to 3 bullets and loops; with no ammo a
// click and the best weapon is chosen; a new weapon is dropped to and raised; armour saves its
// share rounded up; knockback pushes harder on the shooter himself; the hand grenade is lit at
// frame 11 and thrown faster the longer it is held, or goes off in the hand at the fuse's end.
//
// ge:summary Tests: the sequel's weapons: periods, held fire, no ammo, changing, armour, knockback
// ge:keywords test sequel weapons hand-step period machinegun chaingun best-weapon armour-save
// ge:keywords knockback rocket-jump hand-grenade
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_combat_test.cpp
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/server/sequel_combat.hpp"

namespace {

namespace sv = shooters_edge::server;

int item(const char* name) { return sv::sq_item(name); }

// A body holding weapon `w` with `ammo` of its kind.
sv::SqBody armed(sv::SqWeapon w, int ammo) {
  sv::SqBody b = sv::sq_new_body();
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  if (r.item != nullptr) b.counts[item(r.item)] = 1;
  if (r.ammo != sv::SqAmmo::none) b.ammo[int(r.ammo)] = ammo;
  return b;
}

// A hand raised to ready with weapon `w`.
sv::SqHand ready(sv::SqWeapon w, sv::SqBody& b) {
  sv::SqHand h;
  h.weapon = int(w);
  for (int i = 0; i < 100 && h.state != sv::SqHandState::ready; i++)
    sv::sq_hand_step(h, b, false, 0);
  CHECK(h.state == sv::SqHandState::ready);
  return h;
}

// The steps that fired over `steps` steps of attack held, and the shots of each.
std::vector<int> held(sv::SqHand& h, sv::SqBody& b, int steps) {
  std::vector<int> shots;
  shots.reserve(size_t(steps));
  for (int i = 0; i < steps; i++) shots.push_back(sv::sq_hand_step(h, b, true, i * 0.1).shots);
  return shots;
}

}  // namespace

TEST(each_weapon_fires_at_its_period_from_the_step_that_presses) {
  struct Want {
    sv::SqWeapon w;
    int period;
  };
  for (const Want& want : {Want{sv::SqWeapon::blaster, 5}, Want{sv::SqWeapon::shotgun, 12},
                           Want{sv::SqWeapon::rocket_launcher, 9}, Want{sv::SqWeapon::railgun, 16},
                           Want{sv::SqWeapon::grenade_launcher, 12}}) {
    sv::SqBody b = armed(want.w, 50);
    sv::SqHand h = ready(want.w, b);
    const std::vector<int> shots = held(h, b, 3 * want.period);
    CHECK_MSG(shots[0] == 1, sv::kSqWeapons[int(want.w)].name);  // fires in the pressing step
    for (size_t i = 1; i < shots.size(); i++)
      CHECK(shots[i] == (i % size_t(want.period) == 0 ? 1 : 0));
  }
}

TEST(the_bfg_charges_before_it_fires_and_takes_fifty_cells) {
  sv::SqBody b = armed(sv::SqWeapon::bfg, 100);
  sv::SqHand h = ready(sv::SqWeapon::bfg, b);
  const std::vector<int> shots = held(h, b, 25);
  CHECK(shots[8] == 1);  // frame 9 to frame 17
  for (int i = 0; i < 8; i++) CHECK(shots[size_t(i)] == 0);
  for (size_t i = 9; i < shots.size(); i++) CHECK(shots[i] == 0);  // a period of 25 steps
  CHECK(b.ammo[int(sv::SqAmmo::cells)] == 50);
}

TEST(the_machinegun_fires_every_step_while_held_and_stops_on_release) {
  sv::SqBody b = armed(sv::SqWeapon::machinegun, 5);
  sv::SqHand h = ready(sv::SqWeapon::machinegun, b);
  const std::vector<int> shots = held(h, b, 7);
  for (int i = 0; i < 5; i++) CHECK(shots[size_t(i)] == 1);
  CHECK(shots[5] == 0 && b.ammo[int(sv::SqAmmo::bullets)] == 0);
  sv::SqBody more = armed(sv::SqWeapon::machinegun, 50);
  sv::SqHand h2 = ready(sv::SqWeapon::machinegun, more);
  held(h2, more, 3);
  CHECK(sv::sq_hand_step(h2, more, false, 1).shots == 0);
  for (int i = 0; i < 3; i++) sv::sq_hand_step(h2, more, false, 1);
  CHECK(h2.state == sv::SqHandState::ready);
}

TEST(the_chaingun_winds_up_from_one_to_three_bullets_and_loops_while_held) {
  sv::SqBody b = armed(sv::SqWeapon::chaingun, 200);
  sv::SqHand h = ready(sv::SqWeapon::chaingun, b);
  const std::vector<int> shots = held(h, b, 24);
  const std::vector<int> want = {1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3,
                                 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
  CHECK(shots == want);
  int used = 0;
  for (int n : shots) used += n;
  CHECK(b.ammo[int(sv::SqAmmo::bullets)] == 200 - used);
  sv::SqBody c = armed(sv::SqWeapon::chaingun, 200);
  sv::SqHand let_go = ready(sv::SqWeapon::chaingun, c);
  held(let_go, c, 9);  // to frame 14
  CHECK(let_go.frame == 14);
  CHECK(sv::sq_hand_step(let_go, c, false, 1).shots == 0 && let_go.frame == 32);
}

TEST(with_no_ammo_a_click_and_the_best_weapon_with_ammo_is_chosen) {
  sv::SqBody b = armed(sv::SqWeapon::rocket_launcher, 0);
  b.counts[item("weapon_shotgun")] = 1;
  b.ammo[int(sv::SqAmmo::shells)] = 10;
  b.counts[item("weapon_supershotgun")] = 1;  // only 10 shells: the super shotgun is better
  sv::SqHand h = ready(sv::SqWeapon::rocket_launcher, b);
  const sv::SqStep first = sv::sq_hand_step(h, b, true, 5);
  CHECK(first.click && first.shots == 0);
  CHECK(h.pending == int(sv::SqWeapon::super_shotgun));
  CHECK(!sv::sq_hand_step(h, b, true, 5.5).click);  // at most once a second
  // Dropped, then raised: the view model changes once.
  int changed = 0;
  for (int i = 0; i < 30; i++) changed += sv::sq_hand_step(h, b, false, 6).changed ? 1 : 0;
  CHECK(changed == 1 && h.weapon == int(sv::SqWeapon::super_shotgun));
  CHECK(h.state == sv::SqHandState::ready);
  CHECK(sv::sq_best_weapon(sv::sq_new_body()) == int(sv::SqWeapon::blaster));
}

TEST(only_a_held_weapon_is_chosen_and_names_find_weapons) {
  sv::SqBody b = sv::sq_new_body();
  sv::SqHand h;
  CHECK(!sv::sq_choose(h, b, int(sv::SqWeapon::railgun)));
  b.counts[item("weapon_railgun")] = 1;
  CHECK(sv::sq_choose(h, b, int(sv::SqWeapon::railgun)) && h.pending == int(sv::SqWeapon::railgun));
  CHECK(sv::sq_weapon_named("rocket launcher") == int(sv::SqWeapon::rocket_launcher));
  CHECK(sv::sq_weapon_of_item(item("weapon_bfg")) == int(sv::SqWeapon::bfg));
  CHECK(sv::sq_weapon_of_item(item("ammo_rockets")) == -1);
}

TEST(armour_saves_its_share_rounded_up_and_knockback_pushes_the_shooter_harder) {
  sv::SqBody b = sv::sq_new_body();
  b.armour = sv::SqArmour::jacket;
  b.armour_count = 25;
  CHECK(sv::sq_armour_save(b, 10, false) == 3 && b.armour_count == 22);
  CHECK(sv::sq_armour_save(b, 7, false) == 3);  // 2.1 rounded up
  CHECK(sv::sq_armour_save(b, 50, true) == 0);  // a jacket takes no energy
  b.armour = sv::SqArmour::combat;
  b.armour_count = 4;
  CHECK(sv::sq_armour_save(b, 100, false) == 4 && b.armour == sv::SqArmour::none);
  CHECK(sv::sq_knockback(100, 200, false) == 250);
  CHECK(sv::sq_knockback(100, 200, true) == 800);
  CHECK(sv::sq_knockback(10, 10, false) == 100);  // mass held to 50
}

TEST(the_hand_grenade_is_lit_at_eleven_and_thrown_faster_the_longer_it_is_held) {
  sv::SqBody b = armed(sv::SqWeapon::hand_grenade, 3);
  CHECK(sv::sq_holds(b, int(sv::SqWeapon::hand_grenade)));  // held while there are grenades
  sv::SqHand h = ready(sv::SqWeapon::hand_grenade, b);
  // A tap: frames 1 to 11 a step each; lit at 11 and, let go, thrown at the least speed.
  double now = 0;
  sv::SqStep st = sv::sq_hand_step(h, b, true, now);
  int steps = 1;
  while (!st.lit && steps < 20) st = sv::sq_hand_step(h, b, false, now += 0.1), steps++;
  CHECK(st.lit && h.frame == 12 && steps == 11);
  CHECK(st.shots == 1 && st.throw_speed == 400 && b.ammo[int(sv::SqAmmo::grenades)] == 2);
  // Frame 15 waits for the fuse's end before the hand is ready.
  for (int i = 0; i < 31; i++) sv::sq_hand_step(h, b, false, now += 0.1);
  CHECK(h.state == sv::SqHandState::firing && h.frame == 15);
  sv::sq_hand_step(h, b, false, now += 0.2);
  CHECK(h.state == sv::SqHandState::ready && h.fuse_at == 0);
  // Held 2 s past the lighting, let go a step later: 1.1 s of fuse left, 400 + 1.9 × 400 / 3.
  st = sv::sq_hand_step(h, b, true, now += 0.1);
  for (int i = 0; i < 10 + 20; i++) st = sv::sq_hand_step(h, b, true, now += 0.1);
  CHECK(st.shots == 0 && h.frame == 11);
  st = sv::sq_hand_step(h, b, false, now += 0.1);
  CHECK(st.shots == 1);
  CHECK_MSG(st.throw_speed > 650 && st.throw_speed < 657, std::to_string(st.throw_speed));
  CHECK(st.fuse_left > 1.05 && st.fuse_left < 1.15);
  for (int i = 0; i < 20; i++) sv::sq_hand_step(h, b, false, now += 0.1);
  CHECK(h.state == sv::SqHandState::ready);
  // Held to the end of the fuse: one shot with no speed, the grenade gone off in the hand.
  int fired = 0;
  float speed = -1;
  for (int i = 0; i < 50; i++) {
    st = sv::sq_hand_step(h, b, true, now += 0.1);
    if (st.shots > 0) fired++, speed = st.throw_speed;
  }
  CHECK(fired == 1 && speed == 0 && b.ammo[int(sv::SqAmmo::grenades)] == 0);
  CHECK(!sv::sq_holds(b, int(sv::SqWeapon::hand_grenade)));
}
