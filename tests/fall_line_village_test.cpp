// FALL LINE'S VILLAGE ON THE FREE RIDE, HELD (games/fall_line/sim/village.hpp): on seed 38 the
// free ride lays the village's streets clear of the snowmobile's spot and the helicopter's pad,
// fells its trees out of the run's tree index and plans its traffic; a skier into a parked car's
// side at a run goes down and a slow one is held out of it; a moving vehicle driving into him
// stood in its way puts him down. The plan held by tests/golden/fall_line_village.txt: a
// deliberate change rewrites it with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_village_test`
// and puts the `traffic` lab's table before and after in the PR.
//
// ge:summary Test: the village on the free ride: pads kept clear, trees felled, the skier struck
// ge:keywords test village plan_village meet_traffic felled pads parked vehicle struck golden
// ge:skill sim testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/village.hpp"
#include "golden.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/traffic.hpp"
#include "skier/skier.hpp"
#include "skier/trees.hpp"
#include "skier/tuning.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;
namespace sk = ge::skier;
namespace lm = ge::core::libm;

constexpr const char* kGolden = "tests/golden/fall_line_village.txt";
constexpr double kRadius = sk::tuning::wipeout::kBodyRadius;

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

const fs::GameState& seed38() {
  static const fs::GameState st =
      fs::create_free_ride(std::make_shared<const mg::Level>(fs::free_map(38, 10)), 0);
  return st;
}

// The run's traffic plan, or null where its village has none.
const vg::TrafficPlan* plan_of(const fs::GameState& st) {
  if (!st.village) return nullptr;
  const std::optional<vg::TrafficPlan>& traffic = st.village->traffic;
  return traffic ? &*traffic : nullptr;
}

int count(const fs::GameState& st, fs::EventKind kind) {
  int n = 0;
  for (const fs::GameEvent& e : st.events) n += e.kind == kind ? 1 : 0;
  return n;
}

// The skier stood `off` m out to the right of a body's middle facing `heading`, skiing at it.
void at_side(fs::GameState& st, double x, double z, double heading, double off, double speed) {
  const double rx = lm::cos(heading);
  const double rz = -lm::sin(heading);
  st.racers[0].skier =
      sk::stand_skier(st.world(), x + rx * off, z + rz * off, lm::atan2(-rx, -rz), speed);
}

}  // namespace

TEST(seed_38_plans_a_village_with_traffic_and_fells_its_trees) {
  const fs::GameState& st = seed38();
  CHECK(st.village != nullptr);
  if (!st.village) return;
  const fs::Village& v = *st.village;
  CHECK(v.traffic.has_value());
  CHECK(v.felled_count > 0);
  CHECK(v.felled.size() == st.level->trees.size());
  // No felled trunk is in the run's index: nothing meets it.
  int met = 0;
  for (size_t i = 0; i < v.felled.size(); i++) {
    if (v.felled[i] == 0) continue;
    const mg::TreeDef& t = st.level->trees[i];
    met += sk::tree_overlap(st.trees, *st.level, t.x, t.z, 0.05) == static_cast<int32_t>(i);
  }
  CHECK(met == 0);
}

TEST(the_machines_stand_off_the_streets) {
  const fs::GameState& st = seed38();
  if (!st.village) return;
  const vg::GroundMask& g = st.village->ground;
  CHECK(!vg::on_street(g, st.army.pad_x, st.army.pad_z));
  CHECK(!vg::on_street(g, st.heli.pad_x, st.heli.pad_z));
}

TEST(a_skier_into_a_parked_car_at_a_run_goes_down) {
  fs::GameState st = seed38();
  const vg::TrafficPlan* plan = plan_of(st);
  if (plan == nullptr || plan->parked.empty()) {
    CHECK(false);
    return;
  }
  const vg::Parked& p = plan->parked.front();
  const double hw = vg::vehicle_size(p.kind).width / 2;
  at_side(st, p.x, p.z, p.heading, hw + kRadius - 0.05, 8);
  st.events.clear();
  fs::meet_traffic(st);
  CHECK(count(st, fs::EventKind::traffic) == 1);
  CHECK(count(st, fs::EventKind::wipeout) == 1);
  CHECK(st.racers[0].skier.down);
}

TEST(a_slow_skier_is_held_out_of_a_parked_car) {
  fs::GameState st = seed38();
  const vg::TrafficPlan* plan = plan_of(st);
  if (plan == nullptr || plan->parked.empty()) {
    CHECK(false);
    return;
  }
  const vg::Parked& p = plan->parked.front();
  const double hw = vg::vehicle_size(p.kind).width / 2;
  at_side(st, p.x, p.z, p.heading, hw + kRadius - 0.1, 1);
  st.events.clear();
  fs::meet_traffic(st);
  const sk::SkierState& c = st.racers[0].skier;
  CHECK(!c.down);
  CHECK(count(st, fs::EventKind::wipeout) == 0);
  // Put back clear of its side, and turned away from it.
  const double rx = lm::cos(p.heading);
  const double rz = -lm::sin(p.heading);
  CHECK((c.x - p.x) * rx + (c.z - p.z) * rz >= hw + kRadius - 1e-9);
  CHECK(c.vx * rx + c.vz * rz > 0);
}

TEST(the_village_stands_buildings_and_a_skier_into_a_wall_goes_down) {
  fs::GameState st = seed38();
  if (!st.village || st.village->buildings.empty()) {
    CHECK(false);
    return;
  }
  const vg::Building& b = st.village->buildings.front();
  // Every building's trees felled: none left on a footprint.
  for (size_t i = 0; i < st.level->trees.size(); i++) {
    const mg::TreeDef& t = st.level->trees[i];
    for (const vg::Building& o : st.village->buildings) {
      if (vg::in_building(o, t.x, t.z)) CHECK(st.village->felled[i] == 1);
    }
  }
  at_side(st, b.x, b.z, b.heading, b.width / 2 + kRadius - 0.05, 8);
  st.events.clear();
  fs::meet_traffic(st);
  CHECK(count(st, fs::EventKind::building) == 1);
  CHECK(count(st, fs::EventKind::wipeout) == 1);
  CHECK(st.racers[0].skier.down);
}

TEST(a_slow_skier_is_held_out_of_a_wall) {
  fs::GameState st = seed38();
  if (!st.village || st.village->buildings.empty()) {
    CHECK(false);
    return;
  }
  const vg::Building& b = st.village->buildings.front();
  at_side(st, b.x, b.z, b.heading, b.width / 2 + kRadius - 0.1, 1);
  st.events.clear();
  fs::meet_traffic(st);
  const sk::SkierState& c = st.racers[0].skier;
  CHECK(!c.down);
  const double rx = lm::cos(b.heading);
  const double rz = -lm::sin(b.heading);
  CHECK((c.x - b.x) * rx + (c.z - b.z) * rz >= b.width / 2 + kRadius - 1e-9);
}

TEST(a_vehicle_driving_into_a_skier_stood_in_its_way_puts_him_down) {
  fs::GameState st = seed38();
  const vg::TrafficPlan* found_plan = plan_of(st);
  if (found_plan == nullptr) {
    CHECK(false);
    return;
  }
  const vg::TrafficPlan& plan = *found_plan;
  // The first moment a car is under way at a village's pace.
  bool found = false;
  for (int64_t step = 0; step < int64_t{2400} && !found; step++) {
    const double t = static_cast<double>(step) * 0.25;
    for (int32_t k = 0; k < static_cast<int32_t>(plan.vehicles.size()) && !found; k++) {
      if (plan.vehicles[static_cast<size_t>(k)].kind == vg::VehicleKind::bike) continue;
      const vg::VehiclePose p = vg::vehicle_at(*st.level, plan, k, t);
      if (!p.shown || p.speed < 4) continue;
      found = true;
      st.steps = static_cast<int64_t>(std::llround(t * fs::kHz));
      st.t = t;
      const double hl = vg::vehicle_size(p.kind).length / 2;
      const double fx = lm::sin(p.heading);
      const double fz = lm::cos(p.heading);
      const double ahead = hl + kRadius - 0.05;
      st.racers[0].skier =
          sk::stand_skier(st.world(), p.x + fx * ahead, p.z + fz * ahead, p.heading + 1.5, 0);
      st.events.clear();
      fs::meet_traffic(st);
      CHECK(count(st, fs::EventKind::traffic) >= 1);
      CHECK(st.racers[0].skier.down);
    }
  }
  CHECK(found);
}

TEST(the_plan_keeps_its_golden) {
  const fs::GameState& st = seed38();
  CHECK(st.village != nullptr);
  const vg::TrafficPlan* found_plan = plan_of(st);
  if (found_plan == nullptr) return;
  const fs::Village& v = *st.village;
  const vg::TrafficPlan& plan = *found_plan;
  const std::string got =
      "38 " + std::to_string(v.felled_count) + " " + std::to_string(plan.vehicles.size()) + " " +
      std::to_string(plan.parked.size()) + " " + hex(vg::traffic_hash(plan, 0xcbf29ce484222325ull));
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed, trees felled, vehicles, cars parked, the traffic's digest "
           "(fall_line_village_test)\n";
    out << got << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == 1);
  std::string line;
  if (!want.empty()) {
    for (const auto& w : want[0]) line += (line.empty() ? "" : " ") + w;
  }
  CHECK_MSG(line == got, "the village moved:\n  got  " + got + "\n  want " + line +
                             "\nIf the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1 and put the traffic lab's table before and "
                             "after in the PR (skill: sim).");
}
