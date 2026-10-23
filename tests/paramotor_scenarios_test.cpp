// THE PARAMOTOR, HELD: every scenario (plugins/paramotor/scenarios.cpp) run twice to the same
// bits, its trajectory's digest and its key numbers held by tests/golden/paramotor_scenarios.txt,
// and each scenario that has one right answer held to it: the launch off the face, the glide near
// 55 km/h and a glide near 4, the climb on full throttle, the steady banked turn, the stall, the
// bot down the fall line at its height and onto its spot, ridge lift, the lee's folds, a crown and
// the drop. The air and the polar are held on their own.
//
// A deliberate change to the model moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/paramotor_scenarios_test`, run the `para` lab before
// and after (`--tag=before`, `--compare=before`) and put both in the PR.
//
// ge:summary Test: the paramotor scenarios' digests and expectations, the air and its eddies
// ge:keywords test paramotor para scenarios golden digest launch glide climb turn stall land
// ge:keywords ridge lee fold crown drop air eddies
// ge:skill paramotor testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "core/math.hpp"
#include "golden.hpp"
#include "paramotor/air.hpp"
#include "paramotor/scenarios.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace pm = ge::paramotor;
constexpr const char* kGolden = "tests/golden/paramotor_scenarios.txt";
constexpr double kDeg = ge::core::kPi / 180;

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

std::string row_of(const pm::ParaScenario& sc, const pm::ParaRun& r) {
  return std::string(sc.id) + " " + hex(r.digest) + " " + hex(ge::test::bits(r.top_speed)) + " " +
         hex(ge::test::bits(r.distance)) + " " + std::to_string(r.folds) + " " +
         pm::para_cut_name(r.cut);
}

pm::ParaRun fly(const char* id) {
  const pm::ParaScenario* sc = pm::find_para_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  if (sc == nullptr) return {};
  return pm::run_para_scenario(*sc);
}

std::string look(const char* id) {
  return std::string("; run `make lab LAB=para ARGS=--scenario=") + id +
         "` and read plugins/paramotor/para.cpp (skill: paramotor)";
}

}  // namespace

TEST(every_scenario_flies_the_same_twice_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::string got;
  for (const pm::ParaScenario& sc : pm::para_scenarios()) {
    const pm::ParaRun a = pm::run_para_scenario(sc);
    const pm::ParaRun b = pm::run_para_scenario(sc);
    CHECK_MSG(a.digest == b.digest, std::string(sc.id) + " is not a pure function of its staging");
    got += row_of(sc, a) + "\n";
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario digest top-speed-bits distance-bits folds cut "
           "(paramotor_scenarios_test, 120 Hz)\n"
        << got;
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want += line + "\n";
  }
  CHECK_MSG(got == want, "the paramotor's flights moved:\n" + got + "want\n" + want +
                             "If the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1, and put the para lab's table before and "
                             "after in the PR (skill: paramotor).");
}

TEST(full_throttle_off_the_face_lets_the_wing_fly_and_lifts_him_off) {
  const pm::ParaRun r = fly("launch");
  CHECK_MSG(r.launch > 0 && r.launch < 3 && r.take_off > r.launch && r.take_off < 5,
            "the wing was let fly at " + std::to_string(r.launch) + " s and lifted him at " +
                std::to_string(r.take_off) + " s" + look("launch"));
  CHECK(r.climbed > 5 && r.cut == pm::ParaCut::none);
}

// The step rate is the integrator's, not the flight's: the launch at 60 and at 240 steps a second
// tops out within a km/h and climbs within a metre of the 120 Hz run.
TEST(the_launch_flies_the_same_at_any_step_rate) {
  const pm::ParaScenario& sc = *pm::find_para_scenario("launch");
  const pm::ParaRun mid = pm::run_para_scenario(sc, 120);
  for (double hz : {60.0, 240.0}) {
    const pm::ParaRun r = pm::run_para_scenario(sc, hz);
    CHECK_MSG(
        std::fabs(r.top_speed - mid.top_speed) * 3.6 < 1 && std::fabs(r.climbed - mid.climbed) < 1,
        "at " + std::to_string(hz) + " Hz the launch topped " + std::to_string(r.top_speed * 3.6) +
            " km/h and climbed " + std::to_string(r.climbed) + " m; at 120 Hz " +
            std::to_string(mid.top_speed * 3.6) + " km/h and " + std::to_string(mid.climbed) +
            " m" + look("launch"));
  }
}

TEST(the_wing_trims_near_55_km_h_and_glides_near_4) {
  const pm::ParaRun r = fly("glide");
  CHECK_MSG(
      r.airspeed * 3.6 > 50 && r.airspeed * 3.6 < 62,
      "the glide's airspeed is " + std::to_string(r.airspeed * 3.6) + " km/h" + look("glide"));
  const double glide = r.airspeed / r.sink;
  CHECK_MSG(glide > 3 && glide < 5, "the glide is " + std::to_string(glide) + look("glide"));
  CHECK(r.folds == 0 && !r.stalled);
}

TEST(full_throttle_climbs) {
  const pm::ParaRun r = fly("climb");
  CHECK_MSG(r.climbed > 20 && r.sink < -0.8,
            "full throttle climbed " + std::to_string(r.climbed) + " m" + look("climb"));
}

TEST(a_toggle_held_is_a_steady_banked_turn) {
  const pm::ParaRun r = fly("turn");
  CHECK_MSG(r.turned > 360 * kDeg && r.max_bank > 15 * kDeg && r.max_bank < 45 * kDeg,
            "the right toggle held turned " + std::to_string(r.turned / kDeg) +
                " deg at a bank of " + std::to_string(r.max_bank / kDeg) + look("turn"));
  CHECK(r.cut == pm::ParaCut::none && !r.wiped_out);
}

TEST(the_brakes_held_down_stall_the_wing) {
  const pm::ParaRun r = fly("stall");
  CHECK_MSG(r.stalled, "both brakes held down never stalled the wing" + look("stall"));
}

TEST(the_bot_flies_down_the_fall_line_at_its_height) {
  const pm::ParaRun r = fly("fly-down");
  const double agl = r.skier.y - ge::skier::flat_strip(0.2, 1).ground_at(r.skier.x, r.skier.z);
  CHECK_MSG(r.take_off > 0 && std::fabs(agl - 60) < 10 && r.cut == pm::ParaCut::none,
            "the bot ended " + std::to_string(agl) + " m over the snow" + look("fly-down"));
  CHECK(std::fabs(r.turned) < 10 * kDeg);
}

TEST(the_bot_glides_onto_its_spot_and_lands_on_his_skis) {
  const pm::ParaRun r = fly("land");
  CHECK_MSG(r.touches == 1 && !r.wiped_out && r.landing_g < 4 && r.closest < 20,
            "the landing touched " + std::to_string(r.touches) + " times at " +
                std::to_string(r.landing_g) + " g, " + std::to_string(r.closest) +
                " m from the spot" + look("land"));
  CHECK(r.cut == pm::ParaCut::ground);
}

TEST(a_wind_up_the_face_lifts_the_wing_and_one_down_it_folds_it) {
  const pm::ParaRun glide = fly("glide");
  const pm::ParaRun ridge = fly("ridge");
  CHECK_MSG(ridge.sink < glide.sink * 0.7,
            "up the face in the wind the sink was " + std::to_string(ridge.sink) + " m/s against " +
                std::to_string(glide.sink) + " in still air" + look("ridge"));
  const pm::ParaRun lee = fly("lee");
  CHECK_MSG(lee.folds > 0 && lee.end.fold < 0.5,
            "in the lee the wing folded " + std::to_string(lee.folds) + " times" + look("lee"));
}

TEST(a_crown_brings_the_wing_down) {
  const pm::ParaRun r = fly("crown");
  CHECK_MSG(r.cut == pm::ParaCut::crown, std::string("the wing flown into the tree ended ") +
                                             pm::para_cut_name(r.cut) + look("crown"));
}

TEST(the_drop_lets_the_rig_go_and_he_skis_on) {
  const pm::ParaRun r = fly("drop");
  CHECK_MSG(r.cut == pm::ParaCut::drop && !r.wiped_out && r.skier.on_snow,
            std::string("the drop ended ") + pm::para_cut_name(r.cut) + look("drop"));
  CHECK(r.end.canopy.down && r.end.motor.down && r.end.rpm == 0);
}

TEST(the_bar_forward_dives_steep_and_fast_and_the_wing_stays_open) {
  const pm::ParaRun r = fly("dive");
  CHECK_MSG(r.top_sink > 18 && r.top_airspeed * 3.6 > 85,
            "the dive fell at most " + std::to_string(r.top_sink) + " m/s at " +
                std::to_string(r.top_airspeed * 3.6) + " km/h" + look("dive"));
  CHECK(r.folds == 0 && r.tangle < 0 && r.end.mode == pm::ParaMode::flown);
}

TEST(the_bar_back_from_a_cruise_only_climbs_and_never_tangles) {
  const pm::ParaRun r = fly("swing-up");
  CHECK_MSG(r.loops == 0 && r.tangle < 0 && r.end.mode == pm::ParaMode::flown,
            std::to_string(r.loops) + " loops, a tangle at " + std::to_string(r.tangle) + " s" +
                look("swing-up"));
}

TEST(a_big_dive_swung_up_goes_round_in_a_loop_and_flies_on) {
  const pm::ParaRun r = fly("loop");
  CHECK_MSG(r.loops == 1 && r.tangle < 0 && r.end.mode == pm::ParaMode::flown,
            std::to_string(r.loops) + " loops, a tangle at " + std::to_string(r.tangle) + " s" +
                look("loop"));
  // A tumble's load: hard, not a fighter's.
  CHECK_MSG(r.top_g > 5 && r.top_g < 12,
            "the loop pulled " + std::to_string(r.top_g) + " g" + look("loop"));
}

TEST(swung_up_without_the_speed_to_go_over_he_falls_into_the_wing_until_the_snow) {
  const pm::ParaRun r = fly("tangle");
  CHECK_MSG(r.tangle > 0 && r.loops == 0, "the swing-up out of a short dive tangled at " +
                                              std::to_string(r.tangle) + " s" + look("tangle"));
  // He falls fast in the bundle, and the snow stops him: wiped out, the rig cut away.
  CHECK(r.top_sink > 12);
  CHECK_MSG(r.wiped_out && r.cut == pm::ParaCut::rider,
            std::string("the tangle ended ") + pm::para_cut_name(r.cut) + look("tangle"));
}

TEST(the_release_cuts_the_tangled_wing_away) {
  const pm::ParaRun r = fly("cut-tangle");
  CHECK(r.tangle > 0 && r.tangle < 7);
  CHECK_MSG(r.cut == pm::ParaCut::drop && r.end.mode == pm::ParaMode::dropped,
            std::string("the release on the tangle ended ") + pm::para_cut_name(r.cut) +
                look("cut-tangle"));
}

TEST(still_air_has_no_lift_and_no_eddies_and_a_wind_up_a_face_rises) {
  const auto level = ge::skier::flat_strip(ge::skier::slope::kPitch, 1);
  const pm::ParaAirField field = pm::make_air_field(level.seed);
  const double x = 2000, z = 2000;
  const double y = level.ground_at(x, z) + 30;
  const pm::ParaAir still = pm::para_air_at(level, field, {0, 0}, 3, x, y, z);
  CHECK(still.x == 0 && still.y == 0 && still.z == 0 && still.rough == 0);
  // The face falls down +z: a wind toward -z blows up it and rises; one toward +z sinks in its
  // lee.
  const pm::ParaAir up = pm::para_air_at(level, field, {0, -10}, 3, x, y, z);
  CHECK_MSG(up.lift > 1 && up.lee == 0, "a 10 m/s wind up a 17 deg face rose at " +
                                            std::to_string(up.lift) +
                                            " m/s; read plugins/paramotor/air.cpp");
  const pm::ParaAir lee = pm::para_air_at(level, field, {0, 10}, 3, x, y, z);
  CHECK(lee.lift < 0 && lee.lee > 0.5 && lee.rough > up.rough);
}
