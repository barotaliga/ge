// FALL LINE'S FREE RIDE, HELD (games/fall_line/sim/free.hpp): the run under the skis, the foot,
// the runs skied and the odometer on the hand-built slope; the reset on his run and before any;
// the chain of runs the bot skis; and the bot on the free ride down seed 38's pistes at the
// generator's v10 rules (pinned, so a new version moves nothing here), every run's arrival, time
// and digest held bit for bit by tests/golden/fall_line_free.txt.
//
// The debug build holds the first `kDebugRuns` pistes; the release build holds them all. A
// deliberate change to the free ride, the bot, the skier or the generator moves the golden:
// rewrite it with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_free_test`, and put the `free`
// lab's table before and after in the PR.
//
// ge:summary Test: the free ride's run under the skis, foot, reset and chain; seed 38 golden
// ge:keywords test free-ride step_free reset_free run_chain foot arrived onto skied golden
// ge:skill sim testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/course.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "golden.hpp"
#include "mountain/mapgen/query.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/fall_line_free.txt";
constexpr double kSeed = 38;
constexpr int kVersion = 10;
[[maybe_unused]] constexpr size_t kDebugRuns = 1;

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

std::shared_ptr<const mg::Level> slope() {
  return std::make_shared<const mg::Level>(ge::skier::synthetic_slope());
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(kSeed, kVersion));
  return level;
}

int count(const std::vector<fs::GameEvent>& events, fs::EventKind kind) {
  int n = 0;
  for (const fs::GameEvent& e : events) n += e.kind == kind ? 1 : 0;
  return n;
}

// The bot on the free ride for `seconds` (or to the foot), every event kept.
std::vector<fs::GameEvent> ride(fs::GameState& state, const fs::BotPlan& plan, double seconds) {
  std::vector<fs::GameEvent> all;
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz) && !state.racers[0].free.arrived; i++) {
    const fs::Controls c = fs::bot_controls(state, 0, plan);
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

}  // namespace

TEST(on_the_slope_the_piste_is_the_one_run_and_the_bot_skis_it_to_its_foot) {
  fs::GameState state = fs::create_free_ride(slope());
  CHECK(state.mode == fs::Mode::free && state.runs->size() == 1);
  CHECK(state.racers.size() == 1 && state.racers[0].free.skied.capacity() >= 1);
  const fs::BotPlan plan = fs::plan_free(*state.level, 0, state.spec);
  CHECK(plan.track->points.size() == state.level->track.points.size());
  const auto events = ride(state, plan, 120);
  const fs::FreeRide& f = state.racers[0].free;
  CHECK_MSG(f.arrived,
            "the bot did not reach the slope's foot; run `make lab LAB=free` (skill: sim)");
  CHECK(count(events, fs::EventKind::arrived) == 1 && count(events, fs::EventKind::onto) == 1);
  CHECK(f.skied.size() == 1 && f.skied[0] == 0 && f.run == 0);
  CHECK(f.last_s >= state.level->track.length - fs::kFootReach);
  // The odometer reads the plan's metres on the snow from the head to the foot: less the flight
  // off the slope's kicker, more the turns.
  CHECK(f.distance > 0.8 * (f.last_s - fs::kHeadIn) && f.distance < f.last_s * 1.2);
  CHECK(count(events, fs::EventKind::gate) == 0 && !state.racers[0].progress.finished);
}

TEST(the_reset_stands_him_down_his_last_run_or_at_the_head_before_any) {
  fs::GameState state = fs::create_free_ride(slope());
  fs::reset_free(state, 0);
  const fs::Racer& r = state.racers[0];
  CHECK(r.free.run == 0 && r.skier.speed == 0 && r.progress.resets == 1);
  CHECK(state.events.back().kind == fs::EventKind::reset);
  const fs::BotPlan plan = fs::plan_free(*state.level, 0, state.spec);
  state.racers[0].free.last = -1;
  ride(state, plan, 10);
  const double was = state.racers[0].free.last_s;
  CHECK(was > fs::kHeadIn + 20);
  // Off the run by 60 m: the reset stands him back where he was last on it, a little further.
  state.racers[0].skier.x += 60;
  state.events.clear();
  fs::reset_free(state, 0);
  const mg::TrackHit hit =
      mg::nearest_track_point((*state.runs)[0], state.racers[0].skier.x, state.racers[0].skier.z);
  CHECK(std::abs(hit.s - (was + fs::kResetPast)) < 0.5 && hit.distance < 0.5);
}

TEST(a_skier_standing_still_is_never_reset_and_one_pushing_nowhere_is) {
  // Stood across the fall line on the brake: he waits at a viewpoint well past `kStuckReset`.
  fs::GameState state = fs::create_free_ride(slope());
  fs::Controls c;
  c.ski.brake = 1;
  for (int i = 0; i < static_cast<int>((fs::kStuckReset + 4) * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&c, 1));
  }
  CHECK_MSG(state.racers[0].progress.resets == 0 && state.racers[0].skier.speed < fs::kStuckSpeed,
            "a skier standing still on the free ride was reset; read the stuck clock in "
            "games/fall_line/sim/game.cpp (skill: sim)");
  // Pushing (the tuck at a crawl) and going nowhere for as long: he is stood back on his run.
  c.ski.tuck = 1;
  for (int i = 0; i < static_cast<int>((fs::kStuckReset + 1) * fs::kHz); i++) {
    state.racers[0].skier.vx = state.racers[0].skier.vy = state.racers[0].skier.vz = 0;
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    if (state.racers[0].progress.resets > 0) break;
  }
  CHECK(state.racers[0].progress.resets == 1);
}

TEST(a_runs_chain_flows_into_the_runs_it_joins_down_to_a_foot) {
  const mg::Level& level = *seed38();
  if (!level.resort) {
    CHECK_MSG(false, "seed 38 built no ski area; run `make lab LAB=resort` (skill: mountain)");
    return;
  }
  const auto& runs = level.resort->runs;
  bool joined = false;
  for (size_t i = 0; i < runs.size(); i++) {
    const mg::Track chain = fs::run_chain(level, static_cast<int32_t>(i));
    CHECK(chain.points.size() > 1 && chain.points.front().s == 0);
    for (size_t k = 1; k < chain.points.size(); k++)
      CHECK(chain.points[k].s > chain.points[k - 1].s);
    if (!runs[i].into) {
      CHECK(std::abs(chain.length - runs[i].length) < 1);
      continue;
    }
    joined = true;
    CHECK(chain.length > runs[i].length);
    // The chain ends at the foot of the last run it joins.
    int32_t k = static_cast<int32_t>(i);
    for (int n = 0; n < 64; n++) {
      const auto& into = runs[static_cast<size_t>(k)].into;
      if (!into) break;
      k = fs::run_index(level, into->run);
    }
    const mg::TrackPoint end = runs[static_cast<size_t>(k)].points.back();
    CHECK(std::abs(chain.points.back().x - end.x) < 1 &&
          std::abs(chain.points.back().z - end.z) < 1);
  }
  CHECK_MSG(joined, "seed 38 has no run that flows into another; pick a seed that has");
}

TEST(the_bot_rides_every_piste_of_seed_38_to_a_foot_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  const mg::Level& level = *seed38();
  if (!level.resort) {
    CHECK_MSG(false, "seed 38 built no ski area; run `make lab LAB=resort` (skill: mountain)");
    return;
  }
  const auto& runs = level.resort->runs;
  std::vector<std::string> got;
#ifdef NDEBUG
  const size_t held = runs.size();
#else
  const size_t held = kDebugRuns;
#endif
  for (size_t i = 0; i < runs.size() && got.size() < held; i++) {
    const mg::Run& run = runs[i];
    if (run.kind != mg::RunKind::piste) continue;
    const fs::RunReport r = fs::simulate_free(seed38(), kSeed, static_cast<int32_t>(i), {});
    CHECK_MSG(r.finished, "run " + run.id +
                              ": the bot did not reach a foot; run `make lab LAB=free` and look "
                              "at its picture (skill: sim)");
    CHECK(r.runs >= 1 && r.distance > 0 && r.gates == 0);
    const fs::RunReport again = fs::simulate_free(seed38(), kSeed, static_cast<int32_t>(i), {});
    CHECK_MSG(again.digest == r.digest, "the free ride is not a pure function of its map");
    const size_t at = got.size();
    got.push_back(run.id + " " + (r.finished ? "1" : "0") + " " + hex(ge::test::bits(r.time)) +
                  " " + std::to_string(r.runs) + " " + hex(ge::test::bits(r.distance)) + " " +
                  hex(r.digest));
    if (!write) {
      CHECK_MSG(at < want.size() && got[at] == want[at],
                "the free ride moved:\n  got  " + got[at] + "\n  want " +
                    (at < want.size() ? want[at] : std::string("(no row)")) +
                    "\nIf the move is meant, rewrite " + kGolden +
                    " with GE_GOLDEN_WRITE=1 on the release build, and put the free lab's table "
                    "before and after in the PR.");
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# run, arrived, time bits, runs skied, metres bits, digest (fall_line_free_test, seed "
           "38, generator v10)\n";
    for (const auto& r : got) out << r << "\n";
  }
}
