// QUAKE'S HELD WEAPON PREDICTED: the nailgun's frames, nails and sounds a think apart while held
// and at rest one think after release; the shotgun's six frames and refire; the axe's four; a state
// a server rebuilds from the frame alone (`weapon_step_of`) running on exactly as the unbroken
// prediction; a long command split as short ones; a frozen player's clock held; impulses taken only
// once the refire is past, a selection refused without ammunition, the cycle; an empty gun changed
// for the best one; the sound table's bits and samples.
//
// ge:summary Tests: Quake's held weapon predicted (frames, refire, thinks, impulses, empty switch)
// ge:keywords test weapon-prediction weapon-clock think-step nailgun shotgun axe best-weapon
// ge:skill classic testing
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "classic/weapon_predict.hpp"

namespace cl = ge::classic;

namespace {

constexpr int kAll = cl::kItAxe | cl::kItShotgun | cl::kItSuperShotgun | cl::kItNailgun |
                     cl::kItSuperNailgun | cl::kItGrenadeLauncher | cl::kItRocketLauncher |
                     cl::kItLightning;

cl::WeaponState holding(int weapon) {
  cl::WeaponState s;
  s.known = true;
  s.weapon = weapon;
  s.clock = 10;
  s.ammo[0] = 50;
  s.ammo[1] = 100;
  s.ammo[2] = 20;
  s.ammo[3] = 40;
  return s;
}

struct Run {
  std::vector<int> frames;  // after each command
  std::vector<cl::WeaponEvent> events;
};

// `count` commands of `msec`, attack held for the first `held`.
Run run(cl::WeaponState& s, int count, int msec, int held, const cl::WeaponWorld& w) {
  Run r;
  for (int k = 0; k < count; k++) {
    cl::WeaponCmd c;
    c.msec = msec;
    c.attack = k < held;
    cl::WeaponEvent ev[8];
    const int n = cl::weapon_command(s, c, w, ev, 8);
    for (int i = 0; i < n; i++) r.events.push_back(ev[i]);
    r.frames.push_back(s.frame);
  }
  return r;
}

// The frames as they changed.
std::vector<int> changes(const std::vector<int>& frames) {
  std::vector<int> out;
  for (const int f : frames)
    if (out.empty() || out.back() != f) out.push_back(f);
  return out;
}

bool same(const cl::WeaponState& a, const cl::WeaponState& b) {
  return a.weapon == b.weapon && a.frame == b.frame && a.step == b.step &&
         a.attack_finished == b.attack_finished && a.next_think == b.next_think &&
         a.ammo[0] == b.ammo[0] && a.ammo[1] == b.ammo[1] && a.ammo[2] == b.ammo[2] &&
         a.ammo[3] == b.ammo[3] && a.impulse == b.impulse;
}

}  // namespace

TEST(nailgun_fires_a_nail_each_think_while_held) {
  cl::WeaponState s = holding(cl::kItNailgun);
  const cl::WeaponWorld w{kAll, ""};
  // 50 commands of 10 ms (0.5 s) held: nails at 0, .1, .2, .3, .4: five, frames 1..5.
  const Run r = run(s, 50, 10, 50, w);
  CHECK(r.events.size() == 5);
  for (const cl::WeaponEvent& e : r.events) CHECK(e.sound == cl::WeaponSound::nailgun);
  CHECK(changes(r.frames) == std::vector<int>({1, 2, 3, 4, 5}));
  CHECK(s.ammo[1] == 95);
  // Released: the next think rests it.
  const Run after = run(s, 12, 10, 0, w);
  CHECK(after.events.empty());
  CHECK(after.frames.back() == 0);
  CHECK(s.next_think == 0);
}

TEST(super_nailgun_takes_two_nails_and_one_when_one_is_left) {
  cl::WeaponState s = holding(cl::kItSuperNailgun);
  s.ammo[1] = 3;
  const cl::WeaponWorld w{kAll, ""};
  const Run r = run(s, 25, 10, 25, w);
  CHECK(r.events.size() == 2);
  CHECK(r.events[0].sound == cl::WeaponSound::super_nailgun);
  CHECK(r.events[1].sound == cl::WeaponSound::nailgun);  // the last nail, one barrel
  CHECK(s.weapon == cl::kItLightning);                   // then out: the best gun left
  CHECK(s.ammo[1] == 0);
}

TEST(shotgun_shows_six_frames_then_rests_and_refires_at_half_a_second) {
  cl::WeaponState s = holding(cl::kItShotgun);
  const cl::WeaponWorld w{kAll, ""};
  const Run once = run(s, 80, 10, 1, w);
  CHECK(changes(once.frames) == std::vector<int>({1, 2, 3, 4, 5, 6, 0}));
  cl::WeaponState t = holding(cl::kItShotgun);
  const Run r = run(t, 99, 10, 99, w);  // held: shots at 0 and 0.5 s
  CHECK(r.events.size() == 2);
  CHECK(t.ammo[0] == 48);
}

TEST(axe_swings_four_frames) {
  cl::WeaponState s = holding(cl::kItAxe);
  const cl::WeaponWorld w{kAll, ""};
  const Run r = run(s, 45, 10, 1, w);
  CHECK(r.events.size() == 1);
  const int base = r.frames[0];
  CHECK(base == 1 || base == 5);
  CHECK(changes(r.frames) == std::vector<int>({base, base + 1, base + 2, base + 3, 0}));
}

TEST(a_state_rebuilt_from_its_frame_runs_on_as_the_unbroken_one) {
  for (const int weapon : {cl::kItAxe, cl::kItShotgun, cl::kItRocketLauncher, cl::kItNailgun,
                           cl::kItSuperNailgun, cl::kItLightning}) {
    for (int cut = 1; cut < 60; cut += 7) {
      cl::WeaponState whole = holding(weapon);
      const cl::WeaponWorld w{kAll, ""};
      run(whole, cut, 13, 80, w);
      // What a server sends: everything but the step, which it works out from the frame.
      cl::WeaponState sent = whole;
      sent.step = cl::weapon_step_of(sent.weapon, sent.frame);
      CHECK_MSG(sent.step == whole.step || sent.frame == 0, "the step from the frame");
      for (int k = cut; k < 80; k++) {
        cl::WeaponCmd c;
        c.msec = 13;
        c.attack = k < 50;
        cl::WeaponEvent ev[8];
        cl::weapon_command(whole, c, w, ev, 8);
        cl::weapon_command(sent, c, w, ev, 8);
        CHECK(same(whole, sent));
      }
    }
  }
}

TEST(a_long_command_is_split_into_short_ones) {
  cl::WeaponState a = holding(cl::kItNailgun), b = holding(cl::kItNailgun);
  const cl::WeaponWorld w{kAll, ""};
  cl::WeaponEvent ev[16];
  cl::WeaponCmd c;
  c.attack = true;
  c.msec = 120;
  const int n_long = cl::weapon_command(a, c, w, ev, 16);
  c.msec = 30;
  int n_short = 0;
  for (int k = 0; k < 4; k++) n_short += cl::weapon_command(b, c, w, ev, 16);
  CHECK(n_long == n_short);
  CHECK(same(a, b));
  CHECK(a.clock == b.clock);
}

TEST(a_frozen_player_holds_the_clock_and_drops_the_impulse) {
  cl::WeaponState s = holding(cl::kItShotgun);
  s.impulse = 7;
  cl::WeaponCmd c;
  c.msec = 50;
  c.frozen = true;
  c.attack = true;
  cl::WeaponEvent ev[4];
  CHECK(cl::weapon_command(s, c, {kAll, ""}, ev, 4) == 0);
  CHECK(s.clock == 10);
  CHECK(s.attack_finished == 10.05f);
  CHECK(s.impulse == 0);
}

TEST(impulses_wait_for_the_refire_and_need_ammunition) {
  cl::WeaponState s = holding(cl::kItRocketLauncher);
  const cl::WeaponWorld w{kAll, ""};
  run(s, 1, 10, 1, w);  // fired: refire at +0.8 s
  cl::WeaponCmd c;
  c.msec = 10;
  c.impulse = 4;
  cl::WeaponEvent ev[4];
  cl::weapon_command(s, c, w, ev, 4);
  CHECK(s.weapon == cl::kItRocketLauncher);
  CHECK(s.impulse == 4);  // pending
  run(s, 80, 10, 0, w);
  CHECK(s.weapon == cl::kItNailgun);
  CHECK(s.frame == 0);
  // The super shotgun with one shell is refused; the cycle skips it.
  s.ammo[0] = 1;
  c.impulse = 3;
  cl::weapon_command(s, c, w, ev, 4);
  CHECK(s.weapon == cl::kItNailgun);
  c.impulse = 12;  // back from the nailgun: past the super shotgun to the shotgun
  cl::weapon_command(s, c, w, ev, 4);
  CHECK(s.weapon == cl::kItShotgun);
}

TEST(an_empty_gun_changes_for_the_best_one) {
  cl::WeaponState s = holding(cl::kItRocketLauncher);
  s.ammo[2] = 0;
  const cl::WeaponWorld w{kAll, ""};
  run(s, 1, 10, 1, w);
  CHECK(s.weapon == cl::kItLightning);
  CHECK(cl::weapon_best(s, w, 2) == cl::kItSuperNailgun);  // under water: no lightning gun
  CHECK(cl::weapon_best(s, {kAll, "7342"}, 0) == cl::kItSuperShotgun);
}

TEST(the_sound_table) {
  CHECK(cl::weapon_sound_bit_of_sample("weapons/spike2.wav") == 32);
  CHECK(cl::weapon_sound_bit_of_sample("weapons/lhit.wav") == 256);
  CHECK(cl::weapon_sound_bit_of_sample("player/axhit2.wav") == 0);
  CHECK(cl::weapon_sound_channel(cl::WeaponSound::lightning_start) == 0);
  CHECK(cl::weapon_view_model(cl::kItSuperNailgun) == std::string_view("progs/v_nail2.mdl"));
}

TEST(the_nailgun_s_nails_leave_its_barrels_in_turn_the_super_nailgun_s_the_middle) {
  cl::WeaponState s = holding(cl::kItNailgun);
  const cl::WeaponWorld w{kAll, ""};
  const Run r = run(s, 50, 10, 50, w);
  CHECK(r.events.size() == 5);
  for (size_t i = 0; i < r.events.size(); i++) CHECK(r.events[i].side == (i % 2 == 0 ? 1 : -1));
  cl::WeaponState sn = holding(cl::kItSuperNailgun);
  const Run rs = run(sn, 25, 10, 25, w);
  for (const cl::WeaponEvent& e : rs.events) CHECK(e.side == 0);
}

// THE MISSION PACKS' GUNS.
namespace {
constexpr int kScourge = kAll | cl::kItLaserCannon | cl::kItMjolnir | cl::kItProximityGun;
}  // namespace

TEST(scourge_s_laser_cannon_fires_twin_and_single_bolts_a_cell_a_think) {
  cl::WeaponState s = holding(cl::kItLaserCannon);
  const cl::WeaponWorld w{kScourge, "", cl::WeaponRules::scourge};
  // Held 0.5 s in 10 ms moves: a think at the press, then every 0.1 s.
  const Run r = run(s, 80, 10, 50, w);
  CHECK(r.events.size() == 5);
  for (size_t i = 0; i < r.events.size(); i++) {
    CHECK(r.events[i].sound == cl::WeaponSound::laser);
    CHECK(r.events[i].copies == (i % 2 == 0 ? 2 : 1));  // twin bolts, a single bolt, in turn
  }
  CHECK(r.events[1].clock - r.events[0].clock > 0.099f &&
        r.events[1].clock - r.events[0].clock < 0.101f);
  CHECK(s.ammo[3] == 35);
  CHECK(changes(r.frames) == (std::vector<int>{1, 4, 1, 4, 1, 0}));
  CHECK(std::string_view(cl::weapon_sound_sample(cl::WeaponSound::laser)) == "hipweap/laserg.wav");
  CHECK(cl::weapon_sound_bit_of_sample("hipweap/laserg.wav") == 256);
}

TEST(scourge_s_mjolnir_swings_with_no_cells_and_is_never_switched_away) {
  cl::WeaponState s = holding(cl::kItMjolnir);
  s.ammo[3] = 0;
  const cl::WeaponWorld w{kScourge, "", cl::WeaponRules::scourge};
  const Run r = run(s, 70, 10, 1, w);  // one press
  CHECK(r.events.empty());             // its blow is heard by what it hits: the server's
  CHECK(changes(r.frames) == (std::vector<int>{1, 2, 3, 4, 0}));
  CHECK(s.weapon == cl::kItMjolnir);
  // The blow at 0.3 s sets the refire 0.4 s on: 0.7 s from the press (at 10.01).
  CHECK(s.attack_finished > 10.70f && s.attack_finished < 10.72f);
}

TEST(scourge_s_key_6_toggles_the_launchers_and_its_impulses_and_cycle_take_its_guns) {
  const cl::WeaponWorld w{kScourge, "", cl::WeaponRules::scourge};
  cl::WeaponState s = holding(cl::kItGrenadeLauncher);
  cl::WeaponEvent ev[8];
  const auto press = [&](int impulse) {
    cl::WeaponCmd c;
    c.msec = 10;
    c.impulse = impulse;
    cl::weapon_command(s, c, w, ev, 8);
    return s.weapon;
  };
  CHECK(press(6) == cl::kItProximityGun);
  CHECK(press(6) == cl::kItGrenadeLauncher);
  CHECK(press(225) == cl::kItLaserCannon);
  CHECK(press(10) == cl::kItMjolnir);  // the cycle's last
  CHECK(press(10) == cl::kItAxe);
  CHECK(press(12) == cl::kItMjolnir);
  CHECK(press(226) == cl::kItMjolnir);
  // The proximity gun fires as the launcher does, with its own sample.
  s = holding(cl::kItProximityGun);
  const Run r = run(s, 10, 10, 1, w);
  CHECK(r.events.size() == 1 && r.events[0].sound == cl::WeaponSound::proximity);
  CHECK(s.ammo[2] == 19);
  // Out of cells and shells and nails, the best weapon is Mjolnir, not the axe.
  cl::WeaponState e = holding(cl::kItLaserCannon);
  e.ammo[0] = e.ammo[1] = e.ammo[3] = 0;
  CHECK(cl::weapon_best(e, w, 0) == cl::kItMjolnir);
}

TEST(dissolution_s_guns_fire_from_the_counts_shown_and_a_change_is_left_to_the_server) {
  const int all = cl::kItAxeEternity | cl::kItNailgun | cl::kItLavaNailgun |
                  cl::kItLavaSuperNailgun | cl::kItMultiRocket | cl::kItPlasmaGun;
  const cl::WeaponWorld w{all, "", cl::WeaponRules::dissolution};
  // The lava super nailgun: two of the nails shown a think, the super nailgun's sound.
  cl::WeaponState s = holding(cl::kItLavaSuperNailgun);
  Run r = run(s, 30, 10, 25, w);
  CHECK(r.events.size() == 3 && r.events[0].sound == cl::WeaponSound::super_nailgun);
  CHECK(s.ammo[1] == 94);
  // The multi rocket launcher: the rocket launcher's sound and refire, one of the rockets shown.
  s = holding(cl::kItMultiRocket);
  r = run(s, 100, 10, 100, w);
  CHECK(r.events.size() == 2 && r.events[0].sound == cl::WeaponSound::rocket);
  CHECK(r.events[1].clock - r.events[0].clock > 0.79f);
  // The plasma gun: one bolt a second, at half volume, its firing frame never shown.
  s = holding(cl::kItPlasmaGun);
  r = run(s, 150, 10, 150, w);
  CHECK(r.events.size() == 2 && r.events[0].sound == cl::WeaponSound::plasma);
  CHECK(cl::weapon_sound_volume(cl::WeaponSound::plasma) == 0.5f);
  CHECK(changes(r.frames) == (std::vector<int>{0}));
  CHECK(s.ammo[3] == 38);
  // Its axe swings at 2048.
  s = holding(cl::kItAxeEternity);
  r = run(s, 5, 10, 1, w);
  CHECK(r.events.size() == 1 && r.events[0].sound == cl::WeaponSound::axe);
  // A key is the server's to follow: the weapon is unknown, nothing fires, until it says.
  s = holding(cl::kItLavaNailgun);
  cl::WeaponEvent ev[8];
  cl::WeaponCmd c;
  c.msec = 10;
  c.impulse = 4;
  c.attack = true;
  CHECK(cl::weapon_command(s, c, w, ev, 8) == 0);
  CHECK(s.weapon == cl::kWeaponUnknown);
  // Run dry: left to the server too.
  s = holding(cl::kItLavaNailgun);
  s.ammo[1] = 0;
  r = run(s, 5, 10, 5, w);
  CHECK(r.events.empty() && s.weapon == cl::kWeaponUnknown);
}

TEST(a_pack_s_weapon_past_the_short_travels_as_its_bit_and_the_level_names_the_rules) {
  CHECK(cl::weapon_off_wire(cl::weapon_on_wire(cl::kItLaserCannon)) == cl::kItLaserCannon);
  CHECK(cl::weapon_off_wire(cl::weapon_on_wire(cl::kItProximityGun)) == cl::kItProximityGun);
  CHECK(cl::weapon_on_wire(cl::kItProximityGun) < 0);
  CHECK(cl::weapon_off_wire(cl::weapon_on_wire(cl::kItAxe)) == cl::kItAxe);
  CHECK(cl::weapon_rules_of({"", "progs/v_axe.mdl", "progs/v_laserg.mdl"}) ==
        cl::WeaponRules::scourge);
  CHECK(cl::weapon_rules_of({"", "progs/v_lava.mdl"}) == cl::WeaponRules::dissolution);
  CHECK(cl::weapon_rules_of({"", "progs/v_axe.mdl"}) == cl::WeaponRules::quake);
  // A server rebuilds the laser cannon's and Mjolnir's thinks from the frame.
  CHECK(cl::weapon_step_of(cl::kItLaserCannon, 4, cl::WeaponRules::scourge) != 0);
  CHECK(cl::weapon_step_of(cl::kItMjolnir, 2, cl::WeaponRules::scourge) == 3);
  CHECK(cl::weapon_step_of(cl::kItLavaNailgun, 3, cl::WeaponRules::dissolution) == 2);
}
