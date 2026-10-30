// FALL LINE'S SKIER AGAINST THE ARMY, HELD (games/fall_line/sim/army_meet.hpp, sled_ride.hpp): on
// seed 38, an army snowmobile's crew skied into at a run knocked off it and lying, then up and
// after him; a machine skied into slowly holding him out, and hard throwing him down; one left
// standing empty stolen with the machine press, ridden, hopped off and taken again; a soldier on
// foot run down. Each offence raises the heat. Every scene's end held bit for bit by
// tests/golden/fall_line_army_meet.txt: a deliberate change rewrites it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_army_meet_test` and puts the `army-sleds` lab's
// table before and after in the PR.
//
// ge:summary Test: the skier against the army: a crew knocked off, the hull, a theft, run down
// ge:keywords test army meet_army knock-off hull thrown steal stolen run-down hit_soldier golden
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
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/army_sled.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/skier.hpp"
#include "skier/tuning.hpp"
#include "snowmobile/sled.hpp"
#include "snowmobile/spec.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace sm = ge::snowmobile;
namespace lm = ge::core::libm;

constexpr const char* kGolden = "tests/golden/fall_line_army_meet.txt";

double dist(double a, double b) { return ge::core::hypot(a, b); }

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

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

// Open snow some 350 m up the mountain from the village's machines: no trunk within 15 m.
fs::Spot open_spot(const fs::GameState& st) {
  const fs::ArmyState& a = st.army;
  const double fx = lm::sin(a.pad_heading);
  const double fz = lm::cos(a.pad_heading);
  for (int32_t i = 0; i < 40; ++i) {
    const double r = 350 + 10.0 * i;
    for (const double side : {0.0, 60.0, -60.0, 120.0, -120.0}) {
      const double x = a.pad_x + fx * r + fz * side;
      const double z = a.pad_z + fz * r - fx * side;
      bool clear = true;
      for (const mg::TreeDef& t : st.level->trees) {
        if (dist(t.x - x, t.z - z) < 15 + t.radius) clear = false;
      }
      if (clear) return {x, 0, z};
    }
  }
  return {a.pad_x + fx * 350, 0, a.pad_z + fz * 350};
}

fs::GameState ride() {
  fs::GameState st = fs::create_free_ride(seed38(), 0);
  st.army.send_helis = false;
  st.army.send_vehicles = false;
  st.army.fire = false;
  return st;
}

// An army machine stood on the open spot facing `heading`, empty: the army sent but no hunt.
fs::ArmySled& stand_empty(fs::GameState& st, double heading) {
  const fs::Spot at = open_spot(st);
  fs::ArmySled m;
  m.id = st.army.next_sled++;
  m.mode = fs::ArmySledMode::empty;
  m.body = sm::stand_sled(*st.level, at.x, at.z, heading);
  m.home_x = st.army.pad_x;
  m.home_z = st.army.pad_z;
  st.army.sleds.push_back(m);
  st.army.active = true;
  return st.army.sleds.back();
}

// The skier stood `off` m out to the right of a machine's middle, skiing at it at `speed`.
void at_its_side(fs::GameState& st, const sm::SledState& b, double off, double speed) {
  const double rx = lm::cos(b.heading);
  const double rz = -lm::sin(b.heading);
  const double heading = lm::atan2(-rx, -rz);
  st.racers[0].skier = sk::stand_skier(st.world(), b.x + rx * off, b.z + rz * off, heading, speed);
}

void step(fs::GameState& st, const fs::Controls& c, std::vector<fs::GameEvent>* all = nullptr) {
  fs::step(st, std::span<const fs::Controls>(&c, 1));
  if (all != nullptr) all->insert(all->end(), st.events.begin(), st.events.end());
}

void run(fs::GameState& st, double s, std::vector<fs::GameEvent>* all = nullptr) {
  const fs::Controls none{};
  for (int64_t k = 0; k < std::llround(s * fs::kHz); k++) step(st, none, all);
}

bool said(const std::vector<fs::GameEvent>& all, fs::ArmySledChange c) {
  for (const fs::GameEvent& e : all) {
    if (e.kind == fs::EventKind::army_sled && static_cast<fs::ArmySledChange>(e.gate) == c) {
      return true;
    }
  }
  return false;
}

int count(const std::vector<fs::GameEvent>& all, fs::EventKind kind) {
  int n = 0;
  for (const fs::GameEvent& e : all) n += e.kind == kind ? 1 : 0;
  return n;
}

// A scene's end for the golden: its name and the state digest.
std::string end_of(const char* scene, const fs::GameState& st) {
  return std::string(scene) + " " + hex(fs::state_hash(st, 0xcbf29ce484222325ull));
}

// A HUNT AT THREE STARS until one of its machines has halted side-on to him, settled: the stars
// pinned and a helicopter over him so it keeps sight of him. Returns that machine's id.
int32_t halted(fs::GameState& st) {
  const fs::Spot at = open_spot(st);
  st.racers[0].skier = sk::stand_skier(st.world(), at.x, at.z, 0, 0);
  st.wanted.lethal = true;
  fs::set_wanted(st, 3);
  const fs::Controls none{};
  for (int64_t k = 0; k < std::llround(240 * fs::kHz); k++) {
    fs::WantedState& w = st.wanted;
    w.heat = 3;
    std::erase_if(w.pursuers, [](const fs::Pursuer& p) { return p.id == 1; });
    w.pursuers.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, at.x, 150, at.z});
    st.racers[0].skier = sk::stand_skier(st.world(), at.x, at.z, 0, 0);
    step(st, none);
    for (const fs::ArmySled& m : st.army.sleds) {
      if (m.mode == fs::ArmySledMode::halt && m.t > 2 * fall_line::sim::army_sled::kSettleS &&
          m.body.speed < 0.5) {
        return m.id;
      }
    }
  }
  return -1;
}

const fs::ArmySled* sled_of(const fs::GameState& st, int32_t id) {
  for (const fs::ArmySled& m : st.army.sleds) {
    if (m.id == id) return &m;
  }
  return nullptr;
}

std::string scene_knock() {
  fs::GameState st = ride();
  const int32_t id = halted(st);
  CHECK_MSG(id >= 0, "no machine halted on seed 38 within 240 s at three stars");
  if (id < 0) return "knock none";
  const double heat = st.wanted.heat;
  // At 4.5 m/s (16 km/h): past the knock off, short of being thrown himself.
  at_its_side(st, sled_of(st, id)->body, 1.6, 4.5);
  std::vector<fs::GameEvent> all;
  run(st, 0.8, &all);
  const fs::ArmySled* m = sled_of(st, id);
  CHECK(said(all, fs::ArmySledChange::knocked));
  CHECK(m != nullptr && m->mode == fs::ArmySledMode::empty);
  CHECK(st.wanted.cause == fs::WantedCause::hit_soldier && st.wanted.heat > heat);
  CHECK(!st.racers[0].skier.down);
  int lying = 0;
  for (const fs::Soldier& s : st.army.soldiers) {
    lying += s.from == fs::SoldierFrom::sled && s.mode == fs::SoldierMode::down ? 1 : 0;
  }
  CHECK_MSG(lying == 2, "the crew lying beside it: " + std::to_string(lying));
  run(st, fall_line::sim::army::kDownS + 0.5);
  for (const fs::Soldier& s : st.army.soldiers) CHECK(s.mode != fs::SoldierMode::down);
  return end_of("knock", st);
}

std::string scene_hull() {
  fs::GameState st = ride();
  const fs::ArmySled& m = stand_empty(st, 0.3);
  const double reach = sm::spec::kHalfWidth + sk::tuning::wipeout::kBodyRadius;
  at_its_side(st, m.body, 1.2, 1.5);
  double least = 1e9;
  const fs::Controls none{};
  for (int64_t k = 0; k < std::llround(2 * fs::kHz); k++) {
    step(st, none);
    const sk::SkierState& c = st.racers[0].skier;
    const sm::SledState& b = st.army.sleds.front().body;
    const double u =
        ge::core::clamp((c.x - b.x) * lm::sin(b.heading) + (c.z - b.z) * lm::cos(b.heading),
                        -sm::spec::kLength / 2, sm::spec::kLength / 2);
    least = std::fmin(
        least, dist(c.x - (b.x + lm::sin(b.heading) * u), c.z - (b.z + lm::cos(b.heading) * u)));
  }
  CHECK_MSG(least > reach - 0.02, "he came within " + std::to_string(least) + " m of its spine");
  CHECK(!st.racers[0].skier.down);
  // Into it at 7 m/s (25 km/h): thrown.
  std::vector<fs::GameEvent> all;
  at_its_side(st, st.army.sleds.front().body, 1.2, 7);
  run(st, 0.5, &all);
  CHECK(st.racers[0].skier.down);
  CHECK(count(all, fs::EventKind::wipeout) == 1);
  CHECK(!st.wanted.active);  // an empty machine is nobody hurt
  return end_of("hull", st);
}

std::string scene_steal() {
  fs::GameState st = ride();
  const fs::ArmySled& m = stand_empty(st, 0.3);
  const int32_t id = m.id;
  const fs::SledSpot own{st.sled.body.x, st.sled.body.z, st.sled.body.heading};
  at_its_side(st, m.body, 2, 0);
  CHECK(fs::sled_within(st, 0));
  std::vector<fs::GameEvent> all;
  fs::Controls press{};
  press.machine = true;
  step(st, press, &all);
  CHECK(fs::on_sled(st, 0) && st.stolen.present && st.stolen.rider && !st.sled.rider);
  CHECK(said(all, fs::ArmySledChange::stolen));
  CHECK(sled_of(st, id) == nullptr);
  CHECK(st.wanted.cause == fs::WantedCause::stole_snowmobile && st.wanted.stars == 2);
  // Ridden off on the throttle.
  const double x0 = st.stolen.body.x;
  const double z0 = st.stolen.body.z;
  fs::Controls gas{};
  gas.ski.tuck = 0.6;
  for (int64_t k = 0; k < std::llround(3 * fs::kHz); k++) step(st, gas, &all);
  CHECK(dist(st.stolen.body.x - x0, st.stolen.body.z - z0) > 5);
  // Hopped off, stood, and taken again: no second offence.
  step(st, press, &all);
  CHECK(!fs::on_sled(st, 0));
  run(st, 4, &all);
  const sm::SledState& b = st.stolen.body;
  at_its_side(st, b, 2, 0);
  step(st, press, &all);
  CHECK(fs::on_sled(st, 0) && st.stolen.rider && st.stolen.taken == 2);
  int stolen = 0;
  for (const fs::GameEvent& e : all) {
    stolen += e.kind == fs::EventKind::army_sled &&
              static_cast<fs::ArmySledChange>(e.gate) == fs::ArmySledChange::stolen;
  }
  CHECK(stolen == 1);
  // The free ride's own machine never moved.
  CHECK(st.sled.body.x == own.x && st.sled.body.z == own.z);
  return end_of("steal", st);
}

std::string scene_run_down() {
  fs::GameState st = ride();
  const fs::Spot at = open_spot(st);
  st.army.active = true;
  fs::Soldier& s =
      fs::enlist_soldier(st, fs::SoldierFrom::wave, 0, fs::SoldierMode::chase, at.x, at.z, 0);
  const int32_t id = s.id;
  st.racers[0].skier = sk::stand_skier(st.world(), at.x - 1.2, at.z, lm::atan2(1, 0), 4);
  std::vector<fs::GameEvent> all;
  run(st, 0.5, &all);
  const fs::Soldier* him = nullptr;
  for (const fs::Soldier& o : st.army.soldiers) him = o.id == id ? &o : him;
  CHECK(him != nullptr && him->mode == fs::SoldierMode::down);
  bool run_down = false;
  for (const fs::GameEvent& e : all) {
    run_down = run_down ||
               (e.kind == fs::EventKind::army && fs::army_change(e) == fs::ArmyChange::run_down);
  }
  CHECK(run_down);
  CHECK(st.wanted.active && st.wanted.cause == fs::WantedCause::hit_soldier &&
        st.wanted.stars >= 1);
  return end_of("run-down", st);
}

// Each scene played once, by its own test or by the golden's, whichever runs first.
template <std::string (*Scene)()>
const std::string& once() {
  static const std::string e = Scene();
  return e;
}

}  // namespace

TEST(a_halted_crew_skied_into_at_a_run_go_over_and_get_up_after_him) {
  CHECK(!once<scene_knock>().empty());
}

TEST(a_machine_holds_him_out_slowly_and_throws_him_hard) { CHECK(!once<scene_hull>().empty()); }

TEST(an_empty_machine_is_stolen_ridden_and_taken_again) { CHECK(!once<scene_steal>().empty()); }

TEST(a_soldier_on_foot_skied_into_goes_down) { CHECK(!once<scene_run_down>().empty()); }

TEST(every_scene_keeps_its_golden) {
  const std::vector<std::string> got{once<scene_knock>(), once<scene_hull>(), once<scene_steal>(),
                                     once<scene_run_down>()};
  CHECK(got.size() == 4);
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each scene's end: its name and the state digest (fall_line_army_meet_test)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "a scene moved:\n  got  " + got[r] + "\n  want " + line +
                                  "\nIf the move is meant, rewrite " + kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the army-sleds lab's table "
                                  "before and after in the PR (skill: sim).");
  }
}
