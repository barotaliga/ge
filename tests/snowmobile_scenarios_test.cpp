// THE SNOWMOBILE, HELD: every scenario (plugins/snowmobile/scenarios.cpp) run twice to the same
// bits, its trajectory's digest and its key numbers held by tests/golden/snowmobile_scenarios.txt,
// and each scenario that has one right answer held to it: the class's top speed on the groomer,
// the belt sat 20-30 cm into powder at rest and planing up as it gathers speed, the climb up a 24
// deg face and the high-mark that bogs on a 35 deg one, full lock's circle, the carve's roll to
// either side, the brake, the parking brake on a slope, the trunk and the cliff that throw the
// rider and the drop he rides out, and the bot up the slope and round a trunk. The drive's curve
// is held on its own.
//
// A deliberate change to the model moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/snowmobile_scenarios_test`, run the `sled` lab before
// and after (`--tag=before`, `--compare=before`) and put both in the PR.
//
// ge:summary Test: the snowmobile scenarios' digests and expectations, and the drive's curve
// ge:keywords test snowmobile sled scenarios golden digest top-speed sink climb high-mark carve
// ge:keywords turn brake park tree cliff drop pilot drive power-curve
// ge:skill snowmobile testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "snowmobile/drive.hpp"
#include "snowmobile/scenarios.hpp"
#include "snowmobile/spec.hpp"

namespace {

namespace sm = ge::snowmobile;
constexpr const char* kGolden = "tests/golden/snowmobile_scenarios.txt";

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

std::string row_of(const sm::SledScenario& sc, const sm::SledRun& r) {
  return std::string(sc.id) + " " + hex(r.digest) + " " + hex(ge::test::bits(r.top_speed)) + " " +
         hex(ge::test::bits(r.distance)) + " " + std::to_string(r.flights) + " " +
         sm::thrown_name(r.thrown);
}

sm::SledRun ride(const char* id) {
  const sm::SledScenario* sc = sm::find_sled_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  if (sc == nullptr) return {};
  return sm::run_sled_scenario(*sc);
}

// The way the machine had `t` s into a run, off its trace.
double way_at(const sm::SledRun& r, double t) {
  for (const sm::SledSample& s : r.trace) {
    if (s.t >= t) return s.way;
  }
  return r.end.way;
}

std::string look(const char* id) {
  return std::string("; run `make lab LAB=sled ARGS=--scenario=") + id +
         "` and read plugins/snowmobile/body.cpp (skill: snowmobile)";
}

}  // namespace

TEST(every_scenario_rides_the_same_twice_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::string> got;
  for (const sm::SledScenario& sc : sm::sled_scenarios()) {
    const sm::SledRun a = sm::run_sled_scenario(sc);
    const sm::SledRun b = sm::run_sled_scenario(sc);
    CHECK_MSG(a.digest == b.digest, std::string(sc.id) +
                                        ": the same scenario ridden twice differs; something in "
                                        "plugins/snowmobile keeps state between runs (skill: "
                                        "determinism)");
    got.push_back(row_of(sc, a));
    const size_t i = got.size() - 1;
    if (!write) {
      CHECK_MSG(i < want.size() && got[i] == want[i],
                "the ride moved:\n  got  " + got[i] + "\n  want " +
                    (i < want.size() ? want[i] : std::string("(no row)")) +
                    "\nIf the move is meant, rewrite " + kGolden +
                    " with GE_GOLDEN_WRITE=1, and put the sled lab's --compare in the PR.");
    }
  }
  if (!write)
    CHECK_MSG(got.size() == want.size(), "a scenario was added or removed; rewrite the golden");
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario digest top-speed-bits distance-bits flights thrown "
           "(snowmobile_scenarios_test, the reference rider, 120 Hz)\n";
    for (const auto& r : got) out << r << "\n";
  }
}

TEST(the_drive_makes_a_two_strokes_power_and_none_below_the_clutch) {
  CHECK(sm::power_share(sm::spec::kIdleRpm) < 0.2);
  CHECK(std::abs(sm::power_share(sm::spec::kPeakRpm) - 1) < 1e-12);
  CHECK(sm::power_share(sm::spec::kEngageRpm) < 0.75);
  CHECK(sm::power_share(sm::spec::kMaxRpm) < 1);
  CHECK(sm::drive_force(sm::spec::kIdleRpm, 1, 0) == 0);
  const double launch = sm::drive_force(sm::spec::kPeakRpm, 1, 0);
  CHECK(launch > 0 && launch <= sm::max_drive_force() + 1e-6);
  CHECK(sm::drive_force(sm::spec::kPeakRpm, 1, 30) < launch / 2);
}

TEST(it_runs_to_the_classs_top_speed_on_the_groomer) {
  const auto r = ride("top-speed");
  const double kmh = r.top_speed * 3.6;
  CHECK_MSG(kmh > sm::spec::kTopSpeedKmh - 15 && kmh < sm::spec::kTopSpeedKmh + 15,
            "top-speed: " + std::to_string(kmh) + " km/h" + look("top-speed"));
  CHECK(r.thrown == sm::Thrown::none);
}

TEST(it_sits_into_powder_at_rest_and_planes_up_as_it_gathers_speed) {
  const auto rest = ride("rest-powder");
  CHECK_MSG(rest.rest_sink > 0.2 && rest.rest_sink < 0.35, "rest-powder: the belt sits " +
                                                               std::to_string(rest.rest_sink) +
                                                               " m in" + look("rest-powder"));
  CHECK_MSG(rest.distance < 0.1, "rest-powder: it moved at an idle");
  const auto launch = ride("launch-powder");
  CHECK(launch.top_speed * 3.6 > 50);
  CHECK(launch.rest_sink < rest.rest_sink / 3);
  // Pinned from a crawl the belt spins far faster than the machine goes: the roost's slip.
  CHECK(launch.top_slip > 5);
}

TEST(it_climbs_a_24_degree_powder_face_and_bogs_on_a_35_degree_one) {
  const auto climb = ride("climb-24");
  CHECK_MSG(climb.climbed > 30 && climb.end.way > 4 && climb.thrown == sm::Thrown::none,
            "climb-24: climbed " + std::to_string(climb.climbed) + " m, way " +
                std::to_string(climb.end.way) + " m/s" + look("climb-24"));
  const auto mark = ride("high-mark-35");
  CHECK_MSG(way_at(mark, 5) < way_at(mark, 1) && mark.top_slip > 5,
            "high-mark-35: its way did not fall off on the face" + look("high-mark-35"));
  CHECK(mark.climbed < climb.climbed);
}

TEST(full_lock_turns_a_tight_circle_and_a_powder_turn_rolls_it_onto_its_inside_edge) {
  const auto lock = ride("turn-lock");
  CHECK_MSG(lock.radius > 3 && lock.radius < 9 && lock.thrown == sm::Thrown::none,
            "turn-lock: a circle of " + std::to_string(lock.radius) + " m" + look("turn-lock"));
  const auto right = ride("powder-carve");
  const auto left = ride("powder-carve-left");
  // Steered right, the heading grows and the machine rolls right side down; the left mirrors it.
  CHECK(right.end.roll > 0.15 && left.end.roll < -0.15);
  CHECK(right.thrown == sm::Thrown::none && left.thrown == sm::Thrown::none);
  CHECK(std::abs(right.end.roll + left.end.roll) < 1e-6);
  CHECK(ride("weave").thrown == sm::Thrown::none);
}

TEST(the_brake_stops_it_and_the_parking_brake_holds_it_on_a_slope) {
  const auto stop = ride("brake");
  CHECK_MSG(stop.stall_time > 0 && stop.distance < 45,
            "brake: " + std::to_string(stop.distance) + " m to stop" + look("brake"));
  CHECK_MSG(ride("park-slope").distance < 0.5, "park-slope: it slid off its parking brake");
}

TEST(a_trunk_and_a_cliff_throw_the_rider_and_a_drop_is_ridden_out) {
  CHECK(ride("tree").thrown == sm::Thrown::tree);
  const auto drop = ride("drop");
  CHECK(drop.flights == 1 && drop.thrown == sm::Thrown::none);
  CHECK(ride("cliff").thrown == sm::Thrown::landing);
}

TEST(the_bot_rides_up_the_slope_and_round_a_trunk_in_its_way) {
  const auto climb = ride("pilot-climb");
  CHECK_MSG(climb.climbed > 100 && climb.closest < 10 && climb.thrown == sm::Thrown::none,
            "pilot-climb: climbed " + std::to_string(climb.climbed) + " m, came within " +
                std::to_string(climb.closest) + " m" + look("pilot-climb"));
  const auto trees = ride("pilot-trees");
  CHECK_MSG(trees.hits == 0 && trees.closest < 10 && trees.thrown == sm::Thrown::none,
            "pilot-trees: hit the trunk or missed its aim; read plugins/snowmobile/pilot.cpp");
}
