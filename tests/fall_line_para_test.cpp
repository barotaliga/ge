// FALL LINE'S PARAMOTOR ON THE FREE RIDE, HELD (games/fall_line/sim/para_ride.hpp and
// para_outing.hpp): begun on the highest head of the runs with the rig ready over him; his skis
// his own on the snow under it and nothing while he hangs in the harness; the machine press
// letting the rig go; his reset under it beginning the ride again on the summit, and his run's
// own once it is off him; and the bot's outing on seed 38 (at the generator's v10 rules, pinned)
// off the summit, landed part of the way down the run he began on and down the rest on his skis,
// held by tests/golden/fall_line_para.txt.
//
// A deliberate change to the rig, its place on the free ride, the bot or the generator moves the
// golden: rewrite it with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_para_test`, and put
// the `free` lab's para table before and after in the PR.
//
// ge:summary Test: the free ride's paramotor: the summit start, the press, the reset, the outing
// ge:keywords test para paramotor free-ride summit launch drop restart machine-press para-outing
// ge:keywords golden
// ge:skill sim paramotor testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/controls.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/para_outing.hpp"
#include "fall_line/sim/para_ride.hpp"
#include "fall_line/sim/simulate.hpp"
#include "golden.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace pm = ge::paramotor;
constexpr const char* kGolden = "tests/golden/fall_line_para.txt";
constexpr double kSeed = 38;
constexpr int kVersion = 10;

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

std::shared_ptr<const mg::Level> map38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(kSeed, kVersion));
  return level;
}

// Steps with `c`, every event kept.
std::vector<fs::GameEvent> run_for(fs::GameState& state, fs::Controls c, double seconds) {
  std::vector<fs::GameEvent> all;
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

bool said(const std::vector<fs::GameEvent>& events, fs::ParaPhase phase) {
  for (const fs::GameEvent& e : events) {
    if (e.kind == fs::EventKind::para && e.gate == static_cast<int32_t>(phase)) return true;
  }
  return false;
}

// The ride begun, then flown on full throttle for `seconds`.
fs::GameState flown(double seconds) {
  fs::GameState state = fs::create_para_ride(map38());
  fs::Controls c;
  c.ski.tuck = 1;
  run_for(state, c, seconds);
  return state;
}

}  // namespace

TEST(the_ride_begins_on_the_highest_head_with_the_rig_ready_over_him) {
  const fs::GameState state = fs::create_para_ride(map38());
  const fs::ParaStart at = fs::para_start(*state.runs);
  for (const mg::Track& run : *state.runs) {
    CHECK(run.points.empty() ||
          run.points.front().y <= (*state.runs)[static_cast<size_t>(at.run)].points.front().y);
  }
  const auto& s = state.racers[0].skier;
  CHECK(std::hypot(s.x - at.x, s.z - at.z) < 0.01 && s.speed == 0);
  CHECK(state.para.present && state.para.body.mode == pm::ParaMode::ready);
  CHECK(fs::under_para(state, 0) && !fs::hung_in_para(state, 0));
  CHECK(state.racers[0].free.start == at.run);
  // A free ride begun anywhere else has no rig.
  CHECK(!fs::create_free_ride(map38()).para.present);
}

TEST(full_throttle_off_the_summit_lifts_him_and_his_skis_wait_in_the_harness) {
  fs::GameState state = fs::create_para_ride(map38());
  fs::Controls c;
  c.ski.tuck = 1;
  const std::vector<fs::GameEvent> ev = run_for(state, c, 15);
  CHECK_MSG(said(ev, fs::ParaPhase::launch) && said(ev, fs::ParaPhase::take_off),
            "full throttle off the summit never lifted him; run `make lab LAB=free ARGS=--para=1` "
            "(skill: sim)");
  CHECK(fs::hung_in_para(state, 0));
  // Hung in the harness his skis are asked nothing, whatever he presses.
  CHECK(fs::para_held(state, 0, c.ski).tuck == 0);
}

TEST(the_press_lets_the_rig_go_and_his_reset_is_his_runs_after) {
  fs::GameState state = flown(15);
  fs::Controls c;
  c.machine = true;
  const std::vector<fs::GameEvent> ev = run_for(state, c, 1 / fs::kHz);
  CHECK(said(ev, fs::ParaPhase::drop) && !fs::under_para(state, 0));
  CHECK(state.para.body.mode == pm::ParaMode::dropped);
  // Off him, the reset is the free ride's: not begun again on the summit.
  const int32_t starts = state.para.starts;
  fs::Controls r;
  r.reset = true;
  const std::vector<fs::GameEvent> after = run_for(state, r, 1 / fs::kHz);
  CHECK(!said(after, fs::ParaPhase::restart) && state.para.starts == starts);
}

TEST(his_reset_under_the_rig_begins_the_ride_again_on_the_summit) {
  fs::GameState state = flown(15);
  const fs::ParaStart at = fs::para_start(*state.runs);
  fs::Controls r;
  r.reset = true;
  const std::vector<fs::GameEvent> ev = run_for(state, r, 1 / fs::kHz);
  CHECK(said(ev, fs::ParaPhase::restart) && state.para.starts == 2);
  CHECK(state.para.body.mode == pm::ParaMode::ready);
  const auto& s = state.racers[0].skier;
  CHECK(std::hypot(s.x - at.x, s.z - at.z) < 0.5);
}

TEST(his_lean_flies_the_acro_and_a_swing_without_speed_tangles_him_until_the_press) {
  fs::GameState state = flown(40);
  CHECK_MSG(state.para.body.flying && state.para.body.agl > 60,
            "40 s of full throttle off the summit left him " + std::to_string(state.para.body.agl) +
                " m over the snow");
  // The throttle shut, a glide; the lean forward is the bar forward, a dive; then back, without
  // the speed to go over.
  fs::Controls c;
  std::vector<fs::GameEvent> seen = run_for(state, c, 4);
  c.lean = -1;
  const std::vector<fs::GameEvent> dive = run_for(state, c, 2);
  seen.insert(seen.end(), dive.begin(), dive.end());
  c.lean = 1;
  const std::vector<fs::GameEvent> back = run_for(state, c, 4);
  seen.insert(seen.end(), back.begin(), back.end());
  CHECK_MSG(said(seen, fs::ParaPhase::tangle) && state.para.body.mode == pm::ParaMode::tangled,
            "a short dive swung up did not tangle him; run `make lab LAB=para "
            "ARGS=--scenario=tangle` (skill: paramotor)");
  CHECK(fs::hung_in_para(state, 0) && !said(seen, fs::ParaPhase::loop));
  // The press cuts the bundle away.
  fs::Controls press;
  press.machine = true;
  CHECK(said(run_for(state, press, 1.0 / fs::kHz), fs::ParaPhase::drop));
  CHECK(state.para.body.mode == pm::ParaMode::dropped);
}

TEST(the_tape_carries_the_speed_bar_on_the_lean) {
  fs::Controls c;
  c.lean = -0.6;
  c.ski.tuck = 0.5;
  const pm::ParaControls p = fs::para_controls(fs::snap_controls(c));
  CHECK(std::abs(p.bar - 0.6) < 0.01 && std::abs(p.throttle - 0.5) < 0.01);
  const fs::Controls back = fs::para_as_controls(p);
  CHECK(back.lean == -p.bar && back.ski.tuck == p.throttle);
}

TEST(the_bot_flies_off_the_summit_on_seed_38_lands_and_skis_down_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto level = map38();
  fs::SimOptions opts;
  opts.max_seconds = 1800;
  const fs::RunReport r = fs::simulate_para_outing(level, kSeed, opts);
  CHECK_MSG(r.finished && r.para_flights == 1,
            "the bot's outing under the paramotor did not end at a foot; run `make lab LAB=free "
            "ARGS=--para=1` and look at seed-38-para.png (skill: sim)");
  CHECK_MSG(r.para_time > 120 && r.wipeouts == 0 && r.resets == 0 && r.max_landing_g < 4,
            "the outing flew " + std::to_string(r.para_time) + " s and landed at " +
                std::to_string(r.max_landing_g) + " g with " + std::to_string(r.wipeouts) +
                " wipeouts; read games/fall_line/sim/para_outing.cpp (skill: sim)");
  const std::string got = std::string(r.finished ? "1" : "0") + " " +
                          std::to_string(r.para_flights) + " " + hex(ge::test::bits(r.time)) + " " +
                          hex(ge::test::bits(r.para_time)) + " " + hex(ge::test::bits(r.distance)) +
                          " " + hex(r.digest);
#ifdef NDEBUG
  const fs::RunReport again = fs::simulate_para_outing(level, kSeed, opts);
  CHECK_MSG(again.digest == r.digest, "the paramotor's outing is not a pure function of its map");
#endif
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# finished flights time-bits hung-bits distance-bits digest "
           "(fall_line_para_test, seed 38 at v10, the reference skis)\n"
        << got << "\n";
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    for (const auto& w : row) want += (want.empty() ? "" : " ") + w;
  }
  CHECK_MSG(got == want, "the paramotor's outing moved:\n  got  " + got + "\n  want " + want +
                             "\nIf the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1, and put the free lab's para table before "
                             "and after in the PR.");
}
