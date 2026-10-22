// FALL LINE'S LIFT RIDE AND THE BOT'S OUTING, HELD (games/fall_line/sim/lift_ride.hpp,
// outing.hpp) on seed 38 at the generator's v10 rules (pinned, so a new version moves nothing
// here): a skier who comes slowly into a load zone facing up the line is boarded, one too fast or
// turned away is not; the next carrier to pass takes him, carries him up and lets him go at the
// lift's let-go point; his reset lets go of the lift; the outing's legs; and the bot's outing down
// run 2, up the gondola at its foot and down run 3, its time, lifts and digest held bit for bit by
// tests/golden/fall_line_outing.txt (release build only: the debug build rides the drag lift).
//
// A deliberate change to the ride, the outing, the bot, the skier or the generator moves the
// golden: rewrite it with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_lift_ride_test`, and
// put the `free` lab's outings table before and after in the PR.
//
// ge:summary Test: the lift ride's boarding, carry and let-go; the bot's outing on seed 38, golden
// ge:keywords test lift-ride board_lift step_lift outing simulate_outing boarded unloaded golden
// ge:skill sim testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/lift_ride.hpp"
#include "fall_line/sim/outing.hpp"
#include "fall_line/sim/simulate.hpp"
#include "golden.hpp"
#include "mountain/mapgen/lift_line.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
constexpr const char* kGolden = "tests/golden/fall_line_outing.txt";
constexpr double kSeed = 38;
constexpr int kVersion = 10;

[[maybe_unused]] bool env_set(const char* name) {
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

[[maybe_unused]] std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(kSeed, kVersion));
  return level;
}

// The index of the lift `id` names on seed 38, or -1.
int32_t lift_named(const fs::GameState& state, const char* id) {
  const auto& resort = state.level->resort;
  if (!resort) return -1;
  const auto& lifts = resort->lifts;
  for (const ml::LiftPlan& p : *state.lifts) {
    if (lifts[static_cast<size_t>(p.lift)].id == id) return p.lift;
  }
  return -1;
}

// One step with his hands off the controls; this step's events.
std::vector<fs::GameEvent> idle(fs::GameState& state) {
  const fs::Controls none{};
  fs::step(state, std::span<const fs::Controls>(&none, 1));
  return state.events;
}

bool has(const std::vector<fs::GameEvent>& events, fs::EventKind kind) {
  for (const fs::GameEvent& e : events) {
    if (e.kind == kind) return true;
  }
  return false;
}

}  // namespace

TEST(a_skier_coming_slowly_into_a_load_zone_facing_up_the_line_is_boarded) {
  const auto& resort = seed38()->resort;
  if (!resort) {
    CHECK_MSG(false, "seed 38 built no ski area; run `make lab LAB=resort` (skill: mountain)");
    return;
  }
  fs::GameState state = fs::create_free_ride(seed38());
  CHECK(state.lifts && state.lifts->size() == resort->lifts.size());
  const int32_t d1 = lift_named(state, "D1");
  CHECK_MSG(d1 >= 0, "seed 38 has no lift D1; pick the drag lift the map has");
  if (d1 < 0) return;
  const ml::LiftPlan& p = (*state.lifts)[static_cast<size_t>(d1)];
  const ml::Place at = ml::load_spot(p);
  // Too fast: not taken.
  state.racers[0].skier = ge::skier::stand_skier(state.world(), at.x, at.z, p.heading, 9);
  fs::board_lift(state, 0);
  CHECK(!fs::on_lift(state, 0));
  // Turned away from the line: not taken.
  state.racers[0].skier = ge::skier::stand_skier(state.world(), at.x, at.z, p.heading + 2, 1);
  fs::board_lift(state, 0);
  CHECK(!fs::on_lift(state, 0));
  // Slow and facing up it: waiting in the zone.
  state.racers[0].skier = ge::skier::stand_skier(state.world(), at.x, at.z, p.heading + 0.3, 1);
  state.events.clear();
  fs::board_lift(state, 0);
  CHECK(fs::on_lift(state, 0) && state.racers[0].free.lift.lift == d1);
  CHECK(state.racers[0].free.lift.phase == fs::LiftPhase::waiting);
  CHECK(has(state.events, fs::EventKind::boarded) && state.racers[0].skier.speed == 0);
}

TEST(the_next_carrier_takes_him_up_and_lets_him_go_at_the_lifts_let_go_point) {
  if (!seed38()->resort) return;
  fs::GameState state = fs::create_free_ride(seed38());
  const int32_t d1 = lift_named(state, "D1");
  if (d1 < 0) return;
  const ml::LiftPlan& p = (*state.lifts)[static_cast<size_t>(d1)];
  fs::wait_for_lift(state, 0, d1);
  int steps = 0;
  while (state.racers[0].free.lift.phase == fs::LiftPhase::waiting && steps < 120 * 30) {
    idle(state);
    steps++;
  }
  CHECK_MSG(state.racers[0].free.lift.phase == fs::LiftPhase::carried,
            "no carrier took him in 30 s; run `make lab LAB=lift-ride` (skill: sim)");
  const double headway = p.loop / p.carriers / p.ride.speed;
  CHECK(steps <= static_cast<int>(headway * 120) + 1);
  bool off = false;
  double ride = 0;
  for (int i = 0; i < 120 * 600 && !off; i++) {
    const fs::Racer& me = state.racers[0];
    const ml::LoopSpot spot = ml::carrier_at(p, me.free.lift.carrier, state.t + fs::kDt);
    for (const fs::GameEvent& e : idle(state)) {
      if (e.kind != fs::EventKind::unloaded) continue;
      off = true;
      ride = e.value;
      CHECK(e.gate == d1);
    }
    if (off) break;
    // Carried: he is where his carrier is.
    const ml::Place at = ml::carrier_place(p, spot);
    CHECK(std::abs(me.skier.x - at.x) < 1e-9 && std::abs(me.skier.z - at.z) < 1e-9);
  }
  CHECK_MSG(off, "the drag lift never let him go; run `make lab LAB=lift-ride` (skill: sim)");
  const fs::Racer& me = state.racers[0];
  const mg::LetGo at = mg::let_go_of(p.kind, p.bottom, p.top);
  CHECK(std::abs(me.skier.x - at.x) < 1e-9 && std::abs(me.skier.z - at.z) < 1e-9);
  CHECK(me.skier.heading == at.heading && me.skier.speed == 0);
  CHECK(!fs::on_lift(state, 0) && me.free.lift.ridden == 1 && !me.free.arrived);
  // The ride takes the line's length less the let-go, at the rope's speed, give or take a step.
  const double want = (ml::off_at(p) - ml::load_at(p)) / p.ride.speed;
  CHECK(std::abs(ride - want) < 0.05);
}

TEST(his_reset_lets_go_of_the_lift) {
  if (!seed38()->resort) return;
  fs::GameState state = fs::create_free_ride(seed38());
  fs::wait_for_lift(state, 0, 0);
  fs::Controls reset{};
  reset.reset = true;
  fs::step(state, std::span<const fs::Controls>(&reset, 1));
  CHECK(!fs::on_lift(state, 0) && has(state.events, fs::EventKind::reset));
}

TEST(an_outing_runs_from_a_pistes_head_to_the_lift_at_its_foot_and_on_from_its_top) {
  const mg::Level& level = *seed38();
  if (!level.resort) return;
  const auto lifts = ml::lift_plans(level);
  const int32_t two = fs::run_index(level, "2");
  const fs::Outing o = fs::outing_from(level, lifts, two);
  CHECK(o.first == two && o.lift >= 0 && o.next == fs::run_index(level, "3"));
  CHECK(level.resort->lifts[static_cast<size_t>(o.lift)].id == "G1");
  const mg::Track up = fs::to_lift_line(level, lifts, o);
  const ml::LiftPlan& p = lifts[static_cast<size_t>(o.lift)];
  // The way up ends past the load zone's middle, on the line up, after a stretch along it.
  const mg::LinePoint end = ml::line_frame(p, up.points.back().x, up.points.back().z);
  CHECK(std::abs(end.u - (p.ride.entry.at + fs::outing::kRunOn)) < 1e-6);
  CHECK(std::abs(ge::core::angle_diff(up.points.back().heading, p.heading)) < 0.01);
  for (size_t i = 1; i < up.points.size(); i++) CHECK(up.points[i].s > up.points[i - 1].s);
  // The way down starts where the lift lets him go and ends at a foot.
  const mg::Track down = fs::from_top_line(level, lifts, o);
  const mg::LetGo off = mg::let_go_of(p.kind, p.bottom, p.top);
  CHECK(std::abs(down.points.front().x - off.x) < 1e-9);
  const mg::Track chain = fs::run_chain(level, o.next);
  CHECK(ge::core::hypot(down.points.back().x - chain.points.back().x,
                        down.points.back().z - chain.points.back().z) < 1);
  // A run whose foot has no lift near it has no outing.
  const fs::Outing none = fs::outing_from(level, lifts, fs::run_index(level, "7"));
  CHECK(none.lift < 0);
}

TEST(the_bot_rides_an_outing_on_seed_38_and_keeps_its_golden) {
#ifdef NDEBUG
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_words(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  if (!seed38()->resort) return;
  fs::SimOptions opts;
  opts.max_seconds = 1800;
  const int32_t two = fs::run_index(*seed38(), "2");
  const fs::RunReport r = fs::simulate_outing(seed38(), kSeed, two, opts);
  CHECK_MSG(r.finished && r.lifts == 1,
            "the bot did not ride its outing down run 2, up G1 and down run 3 to a foot; run "
            "`make lab LAB=free ARGS=--outings=9` and look at its outings picture (skill: sim)");
  CHECK(r.runs >= 2 && r.resets == 0 && r.lift_ride > 0 && r.lift_wait >= 0);
  const fs::RunReport again = fs::simulate_outing(seed38(), kSeed, two, opts);
  CHECK_MSG(again.digest == r.digest, "the outing is not a pure function of its map");
  const std::string got = std::string("2 ") + (r.finished ? "1" : "0") + " " +
                          hex(ge::test::bits(r.time)) + " " + std::to_string(r.lifts) + " " +
                          std::to_string(r.runs) + " " + hex(ge::test::bits(r.lift_ride)) + " " +
                          hex(ge::test::bits(r.distance)) + " " + hex(r.digest);
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# run, finished, time bits, lifts, runs skied, ride bits, metres bits, digest "
           "(fall_line_lift_ride_test, seed 38, generator v10)\n"
        << got << "\n";
    return;
  }
  std::string line;
  if (!want.empty()) {
    for (const auto& w : want[0]) line += (line.empty() ? "" : " ") + w;
  }
  CHECK_MSG(got == line, "the outing moved:\n  got  " + got + "\n  want " + line +
                             "\nIf the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1 on the release build, and put the free "
                             "lab's outings table before and after in the PR.");
#endif
}
