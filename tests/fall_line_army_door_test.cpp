// FALL LINE'S DOOR GUNNERS, HELD (games/fall_line/sim/army_door.hpp): the roster's gunners spread
// two to a hunting machine and manned only by rules of fire past `hold`; on the flat strip a still
// man draws no door fire at one star (`hold`), warning bursts that never touch him at two and aimed
// ones at three, all from the inward door of the pylon turn; a man running fast enough to be flown
// in echelon still draws the crabbed overwatch's fire; and the door gunners' bursts at each
// rule of fire on seed 38, held bit for bit by tests/golden/fall_line_army_door.txt.
//
// A deliberate change to the door gunners moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_army_door_test`, and put the `gunfire` lab's
// tables before and after in the PR.
//
// ge:summary Test: the door gunners: the gunners spread, manning by the rules, the arcs; golden
// ge:keywords test door-gun door-gunner gunners_of doors_manned echelon crab golden
// ge:skill sim testing
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
#include "core/math.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_door.hpp"
#include "fall_line/sim/army_heli_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/gunfire.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/body.hpp"
#include "skier/skier.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace ah = fall_line::sim::army_heli;
namespace lm = ge::core::libm;
constexpr const char* kGolden = "tests/golden/fall_line_army_door.txt";
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

// The door guns' rounds since `after` (a round's id), and, of those fired from the pylon turn, how
// many came from the door its machine's circle turns toward.
struct Doors {
  int32_t rounds = 0;
  int32_t after = -1;
  // Fired from a machine on its pylon turn (its circle's radius, within a fifth), and of those the
  // ones from the inward door.
  int32_t late = 0;
  int32_t inward = 0;
};

void count_doors(const fs::GameState& st, Doors& d) {
  const fs::Spot q = fs::quarry_of(st);
  for (const fs::Round& r : st.gunfire.rounds) {
    if (r.weapon != fs::WeaponKind::door_gun || r.id <= d.after) continue;
    d.after = r.id;
    d.rounds++;
    const int32_t k = r.gun - ah::kGunBase;
    for (const fs::ArmyHeli& h : st.army.helis) {
      const double off = ge::core::hypot(h.x - q.x, h.z - q.z);
      if (k / 2 != h.id || off < 0.8 * ah::kOrbitM || off > 1.2 * ah::kOrbitM) continue;
      d.late++;
      if ((k % 2 == 0) == (h.dir > 0)) d.inward++;
    }
  }
}

// A HUNT ON THE STRIP at `stars` for `limit` s: he stands at its middle or, `run` m/s, runs up it.
struct Hunt {
  Doors doors;
  int32_t hits = 0;
  bool armed_at_hold = false;  // a door gun armed while the rules were `hold`
};

Hunt strip_hunt(int32_t stars, double limit, double run) {
  fs::GameState st = fs::create_game_on(strip(), 1);
  const double z0 = run > 0 ? kMid - 0.5 * run * limit : kMid;
  st.racers[0].skier = sk::stand_skier(st.world(), kMid, z0, 0, 0);
  fs::set_wanted(st, stars);
  const fs::Controls none{};
  Hunt h;
  for (int64_t i = 0; i < std::llround(limit * fs::kHz); i++) {
    fs::WantedState& w = st.wanted;
    if (w.ended != fs::WantedEnd::running) break;
    if (w.stars > 0) w.heat = stars;
    sk::SkierState& s = st.racers[0].skier;
    s = sk::stand_skier(st.world(), kMid, z0 + run * st.t, 0, 0);
    s.vz = run;
    s.speed = run;
    st.racers[0].body = sk::BodyState{};
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    count_doors(st, h.doors);
    for (const fs::Gun& g : st.gunfire.guns) {
      h.armed_at_hold = h.armed_at_hold || (g.weapon == fs::WeaponKind::door_gun &&
                                            fs::roster_of(w.stars).fire == fs::FireRule::hold);
    }
  }
  h.hits = st.gunfire.hits;
  return h;
}

TEST(the_rosters_gunners_are_two_to_a_machine_and_man_only_past_hold) {
  fs::GameState st = fs::create_game_on(strip(), 1);
  for (int32_t i = 0; i < 3; i++) {
    fs::ArmyHeli h;
    h.id = i;
    h.mode = fs::ArmyHeliMode::hunt;
    st.army.helis.push_back(h);
  }
  const auto gunners = [&st](int32_t stars) {
    fs::set_wanted(st, stars);
    return std::to_string(fs::gunners_of(st, st.army.helis[0])) +
           std::to_string(fs::gunners_of(st, st.army.helis[1])) +
           std::to_string(fs::gunners_of(st, st.army.helis[2]));
  };
  CHECK(gunners(5) == "222");
  CHECK(gunners(4) == "220");
  CHECK(gunners(2) == "200");
  CHECK(gunners(1) == "100");
  CHECK(!fs::doors_manned(st, st.army.helis[0]));  // one star: hold
  fs::set_wanted(st, 2);
  CHECK(fs::doors_manned(st, st.army.helis[0]));
  CHECK(!fs::doors_manned(st, st.army.helis[1]));  // no gunner aboard
  st.army.helis[0].mode = fs::ArmyHeliMode::set;
  CHECK(!fs::doors_manned(st, st.army.helis[0]));    // about a landing
  CHECK(fs::gunners_of(st, st.army.helis[1]) == 2);  // its gunners to the next machine hunting
  st.army.helis[0].mode = fs::ArmyHeliMode::hunt;
  st.army.fire = false;
  CHECK(!fs::doors_manned(st, st.army.helis[0]));
}

TEST(a_still_man_draws_no_fire_at_hold_warning_bursts_at_warn_and_aimed_ones_at_aim) {
  const Hunt hold = strip_hunt(1, 90, 0);
  CHECK_MSG(hold.doors.rounds == 0 && !hold.armed_at_hold,
            "at one star the doors fired " + std::to_string(hold.doors.rounds));
  const Hunt warn = strip_hunt(2, 90, 0);
  CHECK_MSG(warn.doors.rounds > 0, "at two stars the doors fired nothing");
  CHECK_MSG(warn.hits == 0, "warning fire struck him " + std::to_string(warn.hits) + " times");
  const Hunt aim = strip_hunt(3, 150, 0);
  CHECK_MSG(aim.doors.rounds > 0, "at three stars the doors fired nothing");
  // The pylon turn has him square out of the inward door: the outer one has no line on him (on
  // the way in, or climbing off a landing, either door may).
  CHECK_MSG(aim.doors.late > 0 && aim.doors.inward == aim.doors.late,
            std::to_string(aim.doors.inward) + " of " + std::to_string(aim.doors.late) +
                " door rounds fired on the pylon turn came from the inward door");
}

TEST(a_running_man_flown_in_echelon_still_draws_the_overwatchs_doors_fire) {
  // At four stars the first machine stays up as the overwatch (the second puts the men down), so
  // it flies echelon off a man running 26 m/s the whole hunt, crabbed side-on for its doors.
  const Hunt run = strip_hunt(4, 60, 26);
  CHECK_MSG(run.doors.rounds > 0, "a man running 26 m/s drew no door fire in 60 s");
}

TEST(the_door_gunners_at_each_rule_of_fire_on_seed_38_keep_their_golden) {
  // He stands on open snow up the mountain from the village at one, two and three stars (hold,
  // warn, aim) for 90 s each, the army's every arm sent. Each 15 s: the digest, the door guns'
  // rounds, every round into him, his injury severity score and how the hunt stands.
  std::vector<std::string> got;
  for (int32_t stars = 1; stars <= 3; stars++) {
    fs::GameState st = fs::create_free_ride(seed38(), 0);
    const double fx = lm::sin(st.army.pad_heading);
    const double fz = lm::cos(st.army.pad_heading);
    const double x = st.army.pad_x + fx * 400;
    const double z = st.army.pad_z + fz * 400;
    st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
    fs::set_wanted(st, stars);
    const fs::Controls none{};
    Doors doors;
    for (int64_t i = 0; i < std::llround(90 * fs::kHz); i++) {
      fs::WantedState& w = st.wanted;
      if (w.ended == fs::WantedEnd::running && w.stars > 0) w.heat = stars;
      if (!st.racers[0].skier.down) st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
      fs::step(st, std::span<const fs::Controls>(&none, 1));
      count_doors(st, doors);
      if (st.steps % static_cast<int64_t>(15 * fs::kHz) == 0) {
        got.push_back(std::to_string(stars) + " " + hex(fs::state_hash(st, 0xcbf29ce484222325ull)) +
                      " " + std::to_string(doors.rounds) + " " + std::to_string(st.gunfire.hits) +
                      " " + std::to_string(sk::severity_of(st.racers[0].body)) + " " +
                      fs::wanted_end_name(st.wanted.ended));
      }
    }
    if (stars == 1) CHECK_MSG(doors.rounds == 0, "the doors fired at hold on seed 38");
    if (stars == 3) CHECK_MSG(doors.rounds > 0, "the doors never fired at aim on seed 38");
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each 15 s: stars, state digest, door rounds, rounds into him, his injury severity "
           "score, the hunt (fall_line_army_door_test, seed 38 at one, two and three stars)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "the door gunners on seed 38 moved at row " + std::to_string(r) +
                                  ":\n  got  " + got[r] + "\n  want " + line +
                                  "\nIf the move is meant, rewrite " + kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the gunfire lab's tables "
                                  "before and after in the PR.");
  }
}

}  // namespace
