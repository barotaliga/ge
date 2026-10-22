// FALL LINE'S SNOWMOBILE ON THE FREE RIDE, HELD (games/fall_line/sim/sled_ride.hpp and
// sled_outing.hpp): parked asleep on its spot clear of the trunks; taken on the machine press
// only from near enough and slow enough; ridden with the skier held on its boards; hopped off
// onto his skis, the machine left where it stands; the rider thrown by a drop and put back on it
// when the run's down reset would stand him up; the tape carrying the machine press and the
// lean; and the bot's outing on it on seed 38 (at the generator's v10 rules, pinned) from its
// spot up a run and down it on his skis to a foot, held by tests/golden/fall_line_sled.txt.
//
// A deliberate change to the machine, its place on the free ride, the bot or the generator moves
// the golden: rewrite it with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_sled_test`, and
// put the `free` lab's sled table before and after in the PR.
//
// ge:summary Test: the free ride's snowmobile: spot, board, ride, hop, thrown, the seed 38 outing
// ge:keywords test sled snowmobile free-ride board hop thrown remount asleep machine-press lean
// ge:keywords tape sled-outing golden
// ge:skill sim snowmobile testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/controls.hpp"
#include "fall_line/sim/course.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "fall_line/sim/sled_outing.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "golden.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/fall_line_sled.txt";
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

fs::GameState slope_ride() {
  return fs::create_free_ride(std::make_shared<const mg::Level>(ge::skier::synthetic_slope()));
}

void run_for(fs::GameState& state, fs::Controls c, double seconds) {
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&c, 1));
  }
}

// One step with the machine press, its events kept.
std::vector<fs::GameEvent> press(fs::GameState& state) {
  fs::Controls c;
  c.machine = true;
  fs::step(state, std::span<const fs::Controls>(&c, 1));
  return state.events;
}

bool said(const std::vector<fs::GameEvent>& events, fs::SledPhase phase) {
  for (const fs::GameEvent& e : events) {
    if (e.kind == fs::EventKind::sled && e.gate == static_cast<int32_t>(phase)) return true;
  }
  return false;
}

}  // namespace

TEST(the_machine_waits_asleep_on_its_spot_clear_of_the_trunks) {
  fs::GameState state = slope_ride();
  const fs::FreeSled& m = state.sled;
  CHECK(m.present && !m.rider && m.asleep && m.taken == 0);
  CHECK(std::abs(m.body.speed) < 0.1);
  for (const mg::TreeDef& t : state.level->trees) {
    CHECK(std::hypot(t.x - m.body.x, t.z - m.body.z) > fs::sled_ride::kPadRadius);
  }
  // A machine nobody touches is not stepped: its numbers do not move.
  const double x = m.body.x, y = m.body.y, z = m.body.z;
  run_for(state, fs::Controls{}, 3);
  CHECK(m.body.x == x && m.body.y == y && m.body.z == z && m.asleep);
}

TEST(the_press_takes_it_only_from_beside_it_and_slow_enough) {
  fs::GameState state = slope_ride();
  // From the head of the run, far off: the press does nothing.
  CHECK(!fs::sled_within(state, 0));
  CHECK(!said(press(state), fs::SledPhase::board) && !fs::on_sled(state, 0));
  fs::stand_by_sled(state, 0);
  CHECK(fs::sled_within(state, 0));
  // Too fast past it: not taken.
  state.racers[0].skier.vz = fs::sled_ride::kBoardFastest + 2;
  state.racers[0].skier.speed = fs::sled_ride::kBoardFastest + 2;
  CHECK(!fs::sled_within(state, 0));
  fs::stand_by_sled(state, 0);
  CHECK(said(press(state), fs::SledPhase::board));
  CHECK(fs::on_sled(state, 0) && state.sled.taken == 1 && state.sled.body.running);
  CHECK(!state.sled.asleep);
}

TEST(ridden_it_carries_him_and_the_hop_leaves_it_where_it_stands) {
  fs::GameState state = slope_ride();
  fs::stand_by_sled(state, 0);
  press(state);
  fs::Controls go;
  go.ski.tuck = 1;
  run_for(state, go, 3);
  const fs::FreeSled& m = state.sled;
  const auto& k = state.racers[0].skier;
  CHECK_MSG(m.body.speed > 5,
            "three seconds on the throttle went nowhere; run `make lab LAB=sled` "
            "(skill: snowmobile)");
  // Held on its boards, moving with it.
  CHECK(std::hypot(k.x - m.body.x, k.z - m.body.z) < 1.5 && !k.down);
  CHECK(std::abs(k.speed - m.body.speed) < 1e-9);
  fs::Controls stop;
  stop.ski.brake = 1;
  run_for(state, stop, 6);
  CHECK(m.body.speed < 1);
  CHECK(said(press(state), fs::SledPhase::hop));
  CHECK(!fs::on_sled(state, 0) && !k.down && m.mode == fs::SledMode::parked);
  CHECK(std::hypot(k.x - m.body.x, k.z - m.body.z) > 0.8);
  // Left to itself it settles and sleeps where it stopped.
  run_for(state, fs::Controls{}, 4);
  const double x = m.body.x, z = m.body.z;
  run_for(state, fs::Controls{}, 4);
  CHECK(m.asleep && m.body.x == x && m.body.z == z);
  CHECK(std::hypot(k.x - m.body.x, k.z - m.body.z) > 0.8);
}

TEST(thrown_off_by_a_drop_he_is_put_back_on_it_where_it_lies) {
  fs::GameState state = slope_ride();
  fs::stand_by_sled(state, 0);
  press(state);
  // Sixteen metres over the snow: the landing throws him.
  state.sled.body.y += 16;
  std::vector<fs::GameEvent> all;
  for (int i = 0; i < static_cast<int>(4 * fs::kHz) && !state.racers[0].skier.down; i++) {
    const fs::Controls c;
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  CHECK_MSG(said(all, fs::SledPhase::crash) && state.racers[0].skier.down && state.sled.thrown,
            "a 16 m drop did not throw the rider; read games/fall_line/sim/sled_ride.cpp");
  CHECK(!fs::on_sled(state, 0) && state.racers[0].progress.wipeouts == 1);
  // The press does not take a machine he was thrown off; the run's down reset puts him back.
  CHECK(!fs::sled_within(state, 0));
  bool righted = false;
  for (int i = 0; i < static_cast<int>(20 * fs::kHz) && !righted; i++) {
    const fs::Controls c;
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    righted = said(state.events, fs::SledPhase::right);
  }
  CHECK(righted && fs::on_sled(state, 0) && !state.sled.thrown);
  CHECK(state.racers[0].progress.resets == 0);
}

TEST(the_tape_carries_the_machine_press_and_the_lean) {
  fs::Controls c;
  c.machine = true;
  c.lean = -0.7;
  c.ski.tuck = 0.5;
  const fs::Controls s = fs::snap_controls(c);
  CHECK(s.machine && !s.reset && !s.ski.jump);
  CHECK(std::abs(s.lean + 0.7) < 0.01 && std::abs(s.ski.tuck - 0.5) < 0.01);
}

TEST(the_bot_rides_it_up_a_run_on_seed_38_and_skis_down_to_a_foot_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto level = std::make_shared<const mg::Level>(fs::free_map(kSeed, kVersion));
  fs::SimOptions opts;
  opts.max_seconds = 1800;
  if (std::getenv("EXP")) fs::simulate_free(level, kSeed, 0, {});
  const fs::RunReport r = fs::simulate_sled_outing(level, kSeed, opts);
  CHECK_MSG(r.finished && r.sled_rides == 1,
            "the bot's outing on the snowmobile did not end at a foot; run `make lab LAB=free` "
            "and look at seed-38-sled.png (skill: sim)");
  CHECK_MSG(r.climbed > 200, "the snowmobile climbed only " + std::to_string(r.climbed) + " m");
  CHECK(r.sled_time > 30 && r.wipeouts == 0);
  const std::string got = std::string(r.finished ? "1" : "0") + " " + std::to_string(r.sled_rides) +
                          " " + hex(ge::test::bits(r.time)) + " " + hex(ge::test::bits(r.climbed)) +
                          " " + hex(ge::test::bits(r.distance)) + " " + hex(r.digest);
#ifdef NDEBUG
  const fs::RunReport again = fs::simulate_sled_outing(level, kSeed, opts);
  CHECK_MSG(again.digest == r.digest, "the snowmobile's outing is not a pure function of its map");
#endif
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# finished taken time-bits climbed-bits distance-bits digest "
           "(fall_line_sled_test, seed 38 at v10, the reference skis)\n"
        << got << "\n";
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    for (const auto& w : row) want += (want.empty() ? "" : " ") + w;
  }
  CHECK_MSG(got == want, "the snowmobile's outing moved:\n  got  " + got + "\n  want " + want +
                             "\nIf the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1, and put the free lab's sled table before "
                             "and after in the PR.");
}
