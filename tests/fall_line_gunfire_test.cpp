// FALL LINE'S GUNFIRE, HELD (games/fall_line/sim/gunfire.hpp, gunfire_body.hpp): a round's flight
// (its speed, energy and time out to a range), where a round meets the body (the chest from in
// front, the back from behind, the head, over him nothing), aimed rifle fire at a still man that
// wounds what it strikes, warning fire and fire where the army does not shoot to kill that never
// touch him, the `hold` rules that fire nothing, a door gun that kills and ends the hunt `killed`,
// and a scripted hunt on seed 38 held bit for bit by tests/golden/fall_line_gunfire.txt.
//
// A deliberate change to the weapons, the aim or the wounds moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_gunfire_test`, and put the `gunfire` lab's
// tables before and after in the PR.
//
// ge:summary Test: the gunfire: flight, the body's capsules, aimed and warning fire, death; golden
// ge:keywords test gunfire rifle lmg door-gun flight_time speed_at body_hit wound killed golden
// ge:skill sim testing
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/gunfire.hpp"
#include "fall_line/sim/gunfire_body.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/body.hpp"
#include "skier/skier.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace lm = ge::core::libm;
constexpr const char* kGolden = "tests/golden/fall_line_gunfire.txt";
constexpr double kMid = sk::kStripSize / 2;

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

const std::shared_ptr<const mg::Level>& strip() {
  static const auto level = std::make_shared<const mg::Level>(sk::flat_strip(0, 1));
  return level;
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

// A RANGE: on the strip, a gun `range` m in front of him (he faces it) fires `bursts` bursts at him
// under `rules`, the gunfire stepped on its own; he is stood again and healed every step, so every
// round meets the same man.
struct Range {
  int32_t fired = 0;
  int32_t hits = 0;
  int32_t snow = 0;
  int32_t injuries = 0;  // injuries the hits did, counted before each heal
  int32_t iss = 0;       // the worst score a step's hits reached
};

Range range_fire(fs::WeaponKind weapon, double range, fs::FireRule rules, int32_t bursts,
                 bool lethal = true) {
  fs::GameState st = fs::create_game_on(strip(), 1);
  st.wanted.lethal = lethal;
  const auto stand = [&st] { st.racers[0].skier = sk::stand_skier(st.world(), kMid, kMid, 0, 0); };
  stand();
  const double gy = st.level->ground_at(kMid, kMid + range) + 1.45;
  fs::Gun* gun = fs::arm_gun(st, 1, weapon, kMid, gy, kMid + range);
  int32_t shot = 0;
  Range r;
  for (int64_t i = 0; i < std::llround(120 * fs::kHz) &&
                      (shot < bursts || gun->left > 0 || !st.gunfire.rounds.empty());
       i++) {
    if (shot < bursts && fs::gun_ready(*gun) && fs::fire_at(st, *gun, fs::AimTarget{}, rules)) {
      shot++;
    }
    st.events.clear();
    fs::step_gunfire(st);
    st.t += fs::kDt;
    st.steps++;
    sk::BodyState& b = st.racers[0].body;
    r.injuries += b.count;
    r.iss = std::max(r.iss, sk::severity_of(b));
    b = sk::BodyState{};
    stand();
  }
  r.fired = st.gunfire.fired;
  r.hits = st.gunfire.hits;
  r.snow = st.gunfire.snow;
  return r;
}

TEST(a_round_slows_with_the_range_and_carries_its_energy_there) {
  const fs::GunDef& rifle = fs::weapons::gun_def(fs::WeaponKind::rifle);
  const fs::GunDef& door = fs::weapons::gun_def(fs::WeaponKind::door_gun);
  CHECK(fs::speed_at(rifle, 300) > 620 && fs::speed_at(rifle, 300) < 670);
  CHECK(fs::energy_at(rifle, 0) > 1700 && fs::energy_at(rifle, 0) < 1760);
  CHECK(fs::energy_at(door, 0) > 3300 && fs::energy_at(door, 0) < 3400);
  CHECK(fs::speed_at(door, 500) > 580 && fs::speed_at(door, 500) < 620);
  // Slower than the muzzle's speed all the way: longer than at that speed.
  const double t = fs::flight_time(rifle, 300);
  CHECK(t > 300 / rifle.muzzle && t < 300 / fs::speed_at(rifle, 300));
}

TEST(a_round_meets_the_chest_from_in_front_the_back_from_behind_and_the_head_up_top) {
  const fs::GameState st = fs::create_game_on(strip(), 1);
  const sk::SkierState s = sk::stand_skier(st.world(), kMid, kMid, 0, 0);
  const fs::BodyPoints b = fs::body_points(s);
  const auto through = [&](double up, double from) {
    return fs::body_hit(b, fs::Vec3{kMid, s.y + up, kMid + from},
                        fs::Vec3{kMid, s.y + up, kMid - from});
  };
  const auto chest = through(1.25, 5);
  CHECK(chest && chest->part == sk::Part::chest);
  const auto back = through(1.25, -5);
  CHECK(back && back->part == sk::Part::back);
  const auto head = through(1.68, 5);
  CHECK(head && head->part == sk::Part::head);
  const auto belly = through(1.05, 5);
  CHECK(belly && belly->part == sk::Part::abdomen);
  CHECK(!through(2.2, 5));
  // Across him at his knees: his left knee first, from his left.
  const auto knee = fs::body_hit(b, fs::Vec3{kMid - 5, s.y + 0.5, kMid + 0.12},
                                 fs::Vec3{kMid + 5, s.y + 0.5, kMid + 0.12});
  CHECK(knee && (knee->part == sk::Part::knee_l || knee->part == sk::Part::thigh_l ||
                 knee->part == sk::Part::shin_l));
  CHECK(fs::pass_distance(fs::Vec3{0, 0, 0}, fs::Vec3{10, 0, 0}, fs::Vec3{5, 3, 0}) == 3);
}

TEST(aimed_rifle_fire_at_a_still_man_wounds_what_it_strikes) {
  const Range r = range_fire(fs::WeaponKind::rifle, 100, fs::FireRule::aim, 20);
  CHECK(r.fired >= 20 && r.fired <= 60);
  CHECK_MSG(r.hits > 0, "no round of " + std::to_string(r.fired) + " struck him at 100 m");
  CHECK(r.injuries > 0);
  CHECK(r.iss >= 9);  // any service round through the trunk is serious at once
}

TEST(warning_fire_and_fire_where_they_do_not_shoot_to_kill_never_touch_him) {
  const Range warn = range_fire(fs::WeaponKind::lmg, 120, fs::FireRule::warn, 12);
  CHECK(warn.fired > 0 && warn.hits == 0 && warn.injuries == 0);
  CHECK(warn.snow > warn.fired / 2);
  const Range spared = range_fire(fs::WeaponKind::rifle, 60, fs::FireRule::aim, 12, false);
  CHECK(spared.fired > 0 && spared.hits == 0 && spared.injuries == 0);
}

TEST(the_hold_rules_fire_nothing) {
  const Range r = range_fire(fs::WeaponKind::door_gun, 100, fs::FireRule::hold, 5);
  CHECK(r.fired == 0);
}

TEST(a_door_gun_on_a_man_at_five_stars_kills_him_and_the_hunt_ends_killed) {
  fs::GameState st = fs::create_game_on(strip(), 1);
  // Staged: one gun alone, the army's own men and machines kept home.
  st.army.fire = false;
  st.army.send_helis = false;
  st.army.send_vehicles = false;
  st.racers[0].skier = sk::stand_skier(st.world(), kMid, kMid, 0, 0);
  fs::set_wanted(st, 5);
  const double gy = st.level->ground_at(kMid + 80, kMid + 120) + 25;
  const fs::Controls none{};
  bool killed = false;
  for (int64_t i = 0; i < std::llround(90 * fs::kHz) && !killed; i++) {
    if (st.wanted.ended == fs::WantedEnd::running) st.wanted.heat = 5;
    fs::Gun* gun = fs::arm_gun(st, 1, fs::WeaponKind::door_gun, kMid + 80, gy, kMid + 120);
    if (gun != nullptr && fs::gun_ready(*gun))
      fs::fire_at(st, *gun, fs::AimTarget{}, fs::FireRule::aim);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    killed = st.wanted.ended == fs::WantedEnd::killed;
  }
  CHECK_MSG(killed, "not killed in 90 s: " + std::to_string(st.gunfire.hits) + " hits of " +
                        std::to_string(st.gunfire.fired) + ", ISS " +
                        std::to_string(sk::severity_of(st.racers[0].body)));
  CHECK(st.racers[0].body.dead >= 0);
  CHECK(st.racers[0].skier.down);
}

TEST(a_scripted_hunt_on_seed_38_keeps_its_golden) {
  // He stands on open snow up the mountain from the village at four stars for 150 s, the army's
  // every arm sent and firing. Each 15 s: the digest, rounds fired, rounds into him, his injury
  // severity score and how the hunt stands.
  fs::GameState st = fs::create_free_ride(seed38(), 0);
  const double fx = lm::sin(st.army.pad_heading);
  const double fz = lm::cos(st.army.pad_heading);
  const double x = st.army.pad_x + fx * 400;
  const double z = st.army.pad_z + fz * 400;
  st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
  fs::set_wanted(st, 4);
  const fs::Controls none{};
  std::vector<std::string> got;
  int32_t rifle = 0;
  int32_t lmg = 0;
  for (int64_t i = 0; i < std::llround(150 * fs::kHz); i++) {
    fs::WantedState& w = st.wanted;
    if (w.ended == fs::WantedEnd::running && w.stars > 0) w.heat = 4;
    if (!st.racers[0].skier.down) st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind != fs::EventKind::gunfire || fs::gunfire_phase(e) != fs::GunfirePhase::shot) {
        continue;
      }
      rifle += e.value == static_cast<double>(fs::WeaponKind::rifle) ? 1 : 0;
      lmg += e.value == static_cast<double>(fs::WeaponKind::lmg) ? 1 : 0;
    }
    if (st.steps % static_cast<int64_t>(15 * fs::kHz) == 0) {
      got.push_back(hex(fs::state_hash(st, 0xcbf29ce484222325ull)) + " " +
                    std::to_string(st.gunfire.fired) + " " + std::to_string(st.gunfire.hits) + " " +
                    std::to_string(sk::severity_of(st.racers[0].body)) + " " +
                    fs::wanted_end_name(st.wanted.ended));
    }
  }
  CHECK_MSG(rifle > 0 && lmg > 0, "the soldiers fired " + std::to_string(rifle) +
                                      " rounds and the snowmobiles' gunners " +
                                      std::to_string(lmg));
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each 15 s: state digest, rounds fired, rounds into him, his injury severity score, "
           "the hunt (fall_line_gunfire_test, the seed 38 hunt at four stars for 150 s)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "the seed 38 hunt moved at row " + std::to_string(r) + ":\n  got  " +
                                  got[r] + "\n  want " + line + "\nIf the move is meant, rewrite " +
                                  kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the gunfire lab's tables "
                                  "before and after in the PR.");
  }
}

}  // namespace
