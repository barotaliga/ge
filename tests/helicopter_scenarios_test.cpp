// THE HELICOPTER, HELD: every scenario (plugins/helicopter/scenarios.cpp) run twice to the same
// bits, its trajectory's digest and its key numbers held by tests/golden/helicopter_scenarios.txt,
// and each scenario that has one right answer held to it: the rotor spooled up on the skids, the
// lift-off and the torque's turn to the left, the climb at full lever, the bot's hover, cruise,
// landing and terrain-following, the crash on a dropped lever, a steep face and a crown, the disc
// over the top, the fuselage hung toward a rider. The rotor's numbers and its wash are held on
// their own.
//
// A deliberate change to the model moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/helicopter_scenarios_test`, run the `heli` lab before
// and after (`--tag=before`, `--compare=before`) and put both in the PR.
//
// ge:summary Test: the helicopter scenarios' digests and expectations, the rotor and the wash
// ge:keywords test helicopter heli scenarios golden digest lift-off torque climb hover cruise
// ge:keywords land crash tree loop terrain rider thrust_most wash
// ge:skill helicopter testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "ai/why.hpp"
#include "check.hpp"
#include "core/math.hpp"
#include "golden.hpp"
#include "helicopter/scenarios.hpp"
#include "helicopter/spec.hpp"
#include "helicopter/wash.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace hc = ge::helicopter;
constexpr const char* kGolden = "tests/golden/helicopter_scenarios.txt";
constexpr double kDeg = ge::core::kPi / 180;
constexpr double kWeight = hc::spec::kMass * 9.81;

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

std::string row_of(const hc::HeliScenario& sc, const hc::HeliRun& r) {
  return std::string(sc.id) + " " + hex(r.digest) + " " + hex(ge::test::bits(r.top_speed)) + " " +
         hex(ge::test::bits(r.distance)) + " " + std::to_string(r.landings) + " " +
         hc::heli_crash_name(r.crash);
}

hc::HeliRun fly(const char* id) {
  const hc::HeliScenario* sc = hc::find_heli_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  if (sc == nullptr) return {};
  return hc::run_heli_scenario(*sc);
}

std::string look(const char* id) {
  return std::string("; run `make lab LAB=heli ARGS=--scenario=") + id +
         "` and read plugins/helicopter/heli.cpp (skill: helicopter)";
}

double off_aim(const char* id, const hc::HeliRun& r) {
  const hc::HeliScenario* sc = hc::find_heli_scenario(id);
  return ge::core::hypot(r.end.x - sc->aim.x, r.end.z - sc->aim.z);
}

}  // namespace

TEST(every_scenario_flies_the_same_twice_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::string got;
  for (const hc::HeliScenario& sc : hc::heli_scenarios()) {
    const hc::HeliRun a = hc::run_heli_scenario(sc);
    const hc::HeliRun b = hc::run_heli_scenario(sc);
    CHECK_MSG(a.digest == b.digest, std::string(sc.id) + " is not a pure function of its staging");
    got += row_of(sc, a) + "\n";
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario digest top-speed-bits distance-bits landings crash "
           "(helicopter_scenarios_test, 120 Hz)\n"
        << got;
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want += line + "\n";
  }
  CHECK_MSG(got == want, "the helicopter's flights moved:\n" + got + "want\n" + want +
                             "If the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1, and put the heli lab's table before and "
                             "after in the PR (skill: helicopter).");
}

TEST(the_rotor_lifts_nearly_twice_its_weight_at_the_hover_and_more_by_the_snow) {
  const double free_air = hc::thrust_most(0, 0, 100, 0);
  CHECK_MSG(free_air > 1.6 * kWeight && free_air < 2.1 * kWeight,
            "the hover's most thrust is " + std::to_string(free_air / kWeight) +
                " of its weight; read plugins/helicopter/heli.cpp's thrust_most");
  CHECK(hc::thrust_most(0, 0, 3.15, 0) > free_air * 1.2);
  // Climbing fast and in fast flight the power buys less; a disc meeting 50 m/s along its thrust
  // lifts nothing.
  CHECK(hc::thrust_most(0, 15, 100, 15) < free_air);
  CHECK(hc::thrust_most(0, 50, 100, 50) == 0);
}

TEST(the_rotor_spools_up_on_its_skids_with_the_lever_down) {
  const hc::HeliRun r = fly("spool-up");
  CHECK_MSG(r.lift_off < 0 && r.end.grounded && r.crash == hc::HeliCrash::none,
            "a machine with its lever down left the snow" + look("spool-up"));
  CHECK(r.end.spool == 1);
}

TEST(the_lever_lifts_it_off_and_the_torque_turns_the_nose_left) {
  const hc::HeliRun r = fly("lift-off");
  CHECK_MSG(r.lift_off > 0 && r.lift_off < 3 && r.climbed > 20,
            "the lever at 0.7 lifted off at " + std::to_string(r.lift_off) + " s and climbed " +
                std::to_string(r.climbed) + " m" + look("lift-off"));
  CHECK_MSG(r.turned < -10 * kDeg, "the torque turned the nose " + std::to_string(r.turned / kDeg) +
                                       " deg" + look("lift-off"));
}

TEST(the_full_lever_climbs_at_some_15_to_20_m_a_second) {
  const hc::HeliRun r = fly("full-climb");
  CHECK_MSG(
      r.top_climb > 13 && r.top_climb < 20,
      "the full lever climbed at " + std::to_string(r.top_climb) + " m/s" + look("full-climb"));
}

TEST(the_bot_holds_a_hover_over_its_spot) {
  const hc::HeliRun r = fly("hover");
  CHECK_MSG(off_aim("hover", r) < 2 && std::fabs(r.end.y - r.trace.front().y) < 1,
            "the hover drifted " + std::to_string(off_aim("hover", r)) + " m" + look("hover"));
}

TEST(the_bot_cruises_at_some_40_m_a_second_and_stops_over_its_aim) {
  const hc::HeliRun r = fly("cruise");
  CHECK_MSG(r.top_speed > 35 && r.top_speed < 45,
            "the cruise topped out at " + std::to_string(r.top_speed) + " m/s" + look("cruise"));
  CHECK(off_aim("cruise", r) < 5 && r.crash == hc::HeliCrash::none);
}

TEST(the_bot_sets_down_softly_on_its_spot_facing_up_the_strip) {
  const hc::HeliRun r = fly("land");
  CHECK_MSG(r.crash == hc::HeliCrash::none && r.landings == 1 && r.end.grounded,
            std::string("the landing ended ") + hc::heli_crash_name(r.crash) + look("land"));
  CHECK(r.hardest_landing < 1.5);
  CHECK(off_aim("land", r) < 5);
  CHECK(std::fabs(ge::core::angle_diff(r.end.heading, ge::core::kPi)) < 0.2);
}

TEST(a_dropped_lever_a_steep_face_and_a_crown_crash_it) {
  CHECK_MSG(fly("drop-lever").crash == hc::HeliCrash::landing,
            "a lever dropped at 30 m did not crash it on its skids" + look("drop-lever"));
  const hc::HeliCrash slope = fly("slope-land").crash;
  CHECK_MSG(slope == hc::HeliCrash::landing || slope == hc::HeliCrash::airframe,
            std::string("setting down on the 17 deg face ended ") + hc::heli_crash_name(slope) +
                look("slope-land"));
  CHECK_MSG(fly("tree").crash == hc::HeliCrash::tree,
            "the disc flown under the tree's top did not meet its crown" + look("tree"));
}

TEST(nothing_stops_the_disc_past_the_vertical) {
  const hc::HeliRun r = fly("loop");
  CHECK_MSG(r.max_disc > 90 * kDeg && r.crash == hc::HeliCrash::none,
            "full aft cyclic took the disc to " + std::to_string(r.max_disc / kDeg) + " deg" +
                look("loop"));
}

TEST(the_bot_keeps_clear_of_the_face_it_climbs) {
  const hc::HeliRun r = fly("terrain");
  CHECK_MSG(r.crash == hc::HeliCrash::none && r.lowest > 30 && r.closest < 5,
            "up the face its lowest was " + std::to_string(r.lowest) + " m" + look("terrain"));
}

TEST(a_rider_on_the_right_skid_hangs_the_fuselage_toward_him) {
  const hc::HeliRun r = fly("rider");
  CHECK_MSG(r.end.roll > 1 * kDeg && r.end.roll < 6 * kDeg,
            "with a rider on the right skid the fuselage hung " +
                std::to_string(r.end.roll / kDeg) + " deg" + look("rider"));
}

TEST(the_wash_blows_out_along_the_snow_under_a_low_hover_and_is_gone_high_up) {
  const auto level = ge::skier::flat_strip(0, 1);
  hc::HeliState h = hc::stand_heli(level, 2000, 2000, 0);
  h.y += 3;
  h.grounded = false;
  h.thrust = kWeight;
  const ge::core::Vec3 out = hc::wash_at(level, h, 2012, level.ground_at(2012, 2000) + 0.2, 2000);
  CHECK_MSG(out.x > 5 && std::fabs(out.z) < 1, "12 m out from a low hover the wash was " +
                                                   std::to_string(out.x) +
                                                   " m/s; read plugins/helicopter/wash.cpp");
  const ge::core::Vec3 down = hc::wash_at(level, h, 2000, h.y + 1, 2001);
  CHECK(down.y < -5);
  h.y += 100;
  const ge::core::Vec3 high = hc::wash_at(level, h, 2012, level.ground_at(2012, 2000) + 0.2, 2000);
  CHECK(high.x == 0 && high.z == 0);
}

TEST(the_bot_flies_the_same_traced_and_says_why_it_slowed_and_settled) {
  // Recording decides nothing: every bot-flown scenario keeps its digest with a trace on.
  for (const hc::HeliScenario& sc : hc::heli_scenarios()) {
    if (sc.hand) continue;
    ge::ai::Why why;
    const hc::HeliRun traced = hc::run_heli_scenario(sc, 120, &why);
    CHECK_MSG(traced.digest == hc::run_heli_scenario(sc).digest,
              std::string(sc.id) +
                  " flew otherwise with its decisions recorded; read "
                  "plugins/helicopter/pilot.cpp (skill: bots)");
  }
  // The landing: the way slows to stop at the aim, and the height settles onto it.
  ge::ai::Why why;
  hc::run_heli_scenario(*hc::find_heli_scenario("land"), 120, &why);
  bool stopped = false, settled = false;
  for (const ge::ai::Decision& d : why.decisions()) {
    stopped = stopped || (std::string(d.name) == "way" && std::string(d.chosen) == "stop_at_aim");
    settled = settled || (std::string(d.name) == "height" && std::string(d.chosen) == "settle");
  }
  CHECK_MSG(stopped && settled,
            "the landing's record never slowed to stop at the aim or settled "
            "onto it; read plugins/helicopter/pilot.cpp (skill: bots)");
}
