// FALL LINE'S RUN, HELD: the bot down the reference seeds (games/fall_line/sim), each run's
// report and digest held bit for bit by tests/golden/fall_line_sim.txt, every one finished, a
// run's tape played back to the same digest, and the course's rules (a gate taken, a gate
// missed, the finish's clock, the reset) staged on the hand-built slope.
//
// The debug build holds the first `kDebugSeeds` seeds (a map build is seconds under the
// sanitizers); the release build holds them all. A deliberate change to the skier, the course
// or the bot moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_sim_test`, and put the `sim` lab's table
// before and after in the PR.
//
// ge:summary Test: the bot's runs over the reference seeds golden; replay; the course's rules
// ge:keywords test sim simulate bot golden digest replay tape course gate missed finish reset
// ge:skill sim testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/course.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "golden.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/fall_line_sim.txt";
// The reference seeds, the order the golden lists them in.
constexpr double kSeeds[] = {38, 1, 2, 3, 4, 5, 6, 7, 8};
[[maybe_unused]] constexpr size_t kDebugSeeds = 2;

// Whether an environment variable is set (MSVC deprecates std::getenv in favour of its own).
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

std::string row_of(const fs::RunReport& r) {
  return std::to_string(static_cast<int>(r.seed)) + " " + (r.finished ? "1" : "0") + " " +
         hex(ge::test::bits(r.time)) + " " + std::to_string(r.gates) + " " +
         std::to_string(r.missed) + " " + std::to_string(r.resets) + " " +
         std::to_string(r.wipeouts) + " " + std::to_string(r.flights) + " " + hex(r.digest);
}

std::shared_ptr<const mg::Level> slope() {
  return std::make_shared<const mg::Level>(ge::skier::synthetic_slope());
}

// Steps racer 0 straight on with nothing pressed until `seconds` pass, returning every event.
std::vector<fs::GameEvent> coast(fs::GameState& state, double seconds) {
  std::vector<fs::GameEvent> all;
  const fs::Controls none{};
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

int count(const std::vector<fs::GameEvent>& events, fs::EventKind kind) {
  int n = 0;
  for (const auto& e : events) n += e.kind == kind ? 1 : 0;
  return n;
}

}  // namespace

TEST(the_bot_finishes_every_reference_seed_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
#ifdef NDEBUG
  constexpr size_t kHeld = std::size(kSeeds);
#else
  constexpr size_t kHeld = kDebugSeeds;
#endif
  std::vector<std::string> got;
  for (size_t i = 0; i < kHeld; i++) {
    const fs::RunReport r = fs::simulate(kSeeds[i]);
    CHECK_MSG(r.finished, "seed " + std::to_string(static_cast<int>(kSeeds[i])) +
                              ": the bot did not finish; run `make sim SEEDS=" +
                              std::to_string(static_cast<int>(kSeeds[i])) +
                              "` and look at its "
                              "picture (skill: sim)");
    got.push_back(row_of(r));
    if (!write) {
      CHECK_MSG(i < want.size() && got[i] == want[i],
                "the run moved:\n  got  " + got[i] + "\n  want " +
                    (i < want.size() ? want[i] : std::string("(no row)")) +
                    "\nIf the move is meant, rewrite " + kGolden +
                    " with GE_GOLDEN_WRITE=1 on the release build, and put the sim lab's table "
                    "before and after in the PR.");
    }
  }
  if (write) {
    CHECK_MSG(got.size() == std::size(kSeeds), "write the golden from the release build");
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed finished time-bits gates missed resets wipeouts flights digest "
           "(fall_line_sim_test, the bot, generator v1)\n";
    for (const auto& r : got) out << r << "\n";
  }
}

TEST(a_run_played_back_off_its_tape_is_the_same_run) {
  const auto level = std::make_shared<const mg::Level>(fs::race_map(38));
  const fs::RunReport run = fs::simulate_on(level, 38, {});
  const fs::RunReport again = fs::replay_tape(level, 38, run.tape);
  CHECK_MSG(again.digest == run.digest && again.steps == run.steps,
            "the tape played back skied a different run: snap_controls or the tape's schema "
            "(games/fall_line/sim/controls.hpp) dropped a figure the run was skied on");
  CHECK(again.finished && again.time == run.time);
}

TEST(a_gate_crossed_inside_its_width_is_taken_and_outside_it_missed) {
  {
    fs::GameState state = fs::create_game_on(slope(), 1);
    // The grid stands him on the start line, 8 m above the start gate; send him down it.
    state.racers[0].skier = ge::skier::stand_skier(state.world(), 320, 10, 0, 8);
    const auto events = coast(state, 2);
    CHECK(count(events, fs::EventKind::gate) == 1 && state.racers[0].progress.next == 1);
    CHECK(count(events, fs::EventKind::missed) == 0);
  }
  {
    fs::GameState state = fs::create_game_on(slope(), 1);
    // 25 m right of the centre is outside the 40 m gate and its grace.
    state.racers[0].skier = ge::skier::stand_skier(state.world(), 345, 10, 0, 8);
    const auto events = coast(state, 2);
    CHECK(count(events, fs::EventKind::missed) == 1 && state.racers[0].progress.missed);
    CHECK(state.racers[0].progress.next == 0);
    // The reset stands him back on his slot, above the gate he owes.
    const fs::Controls reset{{}, true};
    fs::step(state, std::span<const fs::Controls>(&reset, 1));
    CHECK(state.racers[0].skier.z < state.level->checkpoints[0].z);
    CHECK(!state.racers[0].progress.missed && state.racers[0].progress.resets == 1);
  }
}

TEST(the_finish_line_stops_the_clock_inside_its_step) {
  fs::GameState state = fs::create_game_on(slope(), 1);
  const auto& finish = state.level->checkpoints.back();
  state.racers[0].progress.next = static_cast<int32_t>(state.level->checkpoints.size()) - 1;
  state.racers[0].skier = ge::skier::stand_skier(state.world(), 320, finish.z - 5, 0, 10);
  const auto events = coast(state, 1);
  const auto& p = state.racers[0].progress;
  CHECK(p.finished && count(events, fs::EventKind::finish) == 1);
  // About half a second to cover 5 m; the clock is read inside the step, never on its edge.
  CHECK(p.finish_time > 0.3 && p.finish_time < 0.7);
  CHECK(p.finish_time * fs::kHz != static_cast<double>(static_cast<int>(p.finish_time * fs::kHz)));
}

TEST(a_reset_stands_him_past_the_last_gate_taken_facing_down) {
  fs::GameState state = fs::create_game_on(slope(), 1);
  state.racers[0].progress.next = 1;
  fs::reset_racer(state, 0);
  const auto& gate = state.level->checkpoints[0];
  const auto& s = state.racers[0].skier;
  CHECK(s.z > gate.z && s.z < gate.z + fs::kResetPast + 1 && s.speed == 0 && s.on_snow);
}
