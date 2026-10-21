// FALL LINE'S FIELD, HELD: rivals on one grid with the player (games/fall_line/sim/field.hpp).
// A field of four down the reference seeds, every racer's finish, place and shoulders and the
// run's digest held bit for bit by tests/golden/fall_line_field.txt; the deal (paces off the
// map's seed, lanes off the grid); skier against skier staged exactly; the standings; and a
// field's run played back off the player's tape.
//
// The debug build holds the first `kDebugSeeds` seeds; the release build holds them all. A
// deliberate change to the field, the bot or the skier moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_field_test`, and put the `sim` lab's field
// table (`make sim ARGS=--rivals=3`) before and after in the PR.
//
// ge:summary Test: a field of four golden over the reference seeds; the deal; shoulders; places
// ge:keywords test field rivals deal shoulder meet_racers standings place golden digest replay
// ge:skill sim testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/field.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "golden.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/fall_line_field.txt";
constexpr double kSeeds[] = {38, 1, 2};
[[maybe_unused]] constexpr size_t kDebugSeeds = 1;
constexpr int32_t kRivals = 3;

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

// One line a seed: the seed, then each racer's finished, finish-time bits, place, gates and
// shoulders, then the run's digest.
std::string row_of(const fs::RunReport& r) {
  std::string s = std::to_string(static_cast<int>(r.seed));
  for (const fs::FieldRow& f : r.field) {
    s += std::string(" ") + (f.finished ? "1" : "0") + " " + hex(ge::test::bits(f.time)) + " " +
         std::to_string(f.place) + " " + std::to_string(f.gates) + " " +
         std::to_string(f.shoulders);
  }
  return s + " " + hex(r.digest);
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::race_map(38));
  return level;
}

}  // namespace

TEST(a_field_of_four_finishes_every_reference_seed_and_keeps_its_golden) {
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
    fs::SimOptions opts;
    opts.rivals = kRivals;
    const fs::RunReport r =
        i == 0 ? fs::simulate_on(seed38(), 38, opts) : fs::simulate(kSeeds[i], opts);
    CHECK(r.field.size() == 1 + kRivals);
    for (size_t k = 0; k < r.field.size(); k++) {
      CHECK_MSG(r.field[k].finished, "seed " + std::to_string(static_cast<int>(kSeeds[i])) +
                                         ": racer " + std::to_string(k) +
                                         " did not finish; run `make sim SEEDS=" +
                                         std::to_string(static_cast<int>(kSeeds[i])) +
                                         " ARGS=--rivals=3` and look at its picture (skill: sim)");
    }
    got.push_back(row_of(r));
    if (!write) {
      CHECK_MSG(i < want.size() && got[i] == want[i],
                "the field's run moved:\n  got  " + got[i] + "\n  want " +
                    (i < want.size() ? want[i] : std::string("(no row)")) +
                    "\nIf the move is meant, rewrite " + kGolden +
                    " with GE_GOLDEN_WRITE=1 on the release build, and put the sim lab's field "
                    "table before and after in the PR.");
    }
  }
  if (write) {
    CHECK_MSG(got.size() == std::size(kSeeds), "write the golden from the release build");
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed, then per racer: finished finish-time-bits place gates shoulders; then the "
           "digest (fall_line_field_test, 3 rivals, generator v1)\n";
    for (const auto& r : got) out << r << "\n";
  }
}

TEST(the_deal_is_the_maps_own_and_the_player_rides_the_plain_bot) {
  const mg::Level& level = *seed38();
  const std::vector<fs::Rider> a = fs::deal_field(level, 4);
  const std::vector<fs::Rider> b = fs::deal_field(level, 4);
  CHECK(a.size() == 4);
  CHECK(a[0].pace == 1 && a[0].lane == 0);
  for (size_t i = 1; i < a.size(); i++) {
    CHECK_BITS(a[i].pace, b[i].pace);
    CHECK(a[i].pace >= fs::kPaceLeast && a[i].pace <= fs::kPaceMost);
    CHECK_MSG(a[i].lane != a[i - 1].lane || i == 1, "two rivals dealt the same lane");
  }
  // The field never outnumbers the grid.
  const fs::GameState state = fs::create_game_on(seed38(), 99);
  CHECK(state.racers.size() == level.grid.size());
  CHECK(state.racers[1].rider.pace == a[1].pace);
}

TEST(two_skiers_who_meet_are_shouldered_apart_and_share_their_closing_speed) {
  fs::GameState state = fs::create_game_on(seed38(), 2);
  auto& a = state.racers[0].skier;
  auto& b = state.racers[1].skier;
  b.x = a.x + 0.5;
  b.z = a.z;
  a.vx = 6;
  b.vx = 2;
  a.vz = b.vz = 10;
  const double momentum = a.vx + b.vx;
  state.events.clear();
  fs::meet_racers(state);
  CHECK(state.events.size() == 1 && state.events[0].kind == fs::EventKind::shoulder);
  CHECK(state.events[0].racer == 0 && state.events[0].gate == 1);
  CHECK(std::abs(state.events[0].value - 4) < 1e-12);
  CHECK(std::abs((b.x - a.x) - fs::kShoulder) < 1e-12);
  CHECK(std::abs((a.vx + b.vx) - momentum) < 1e-12);
  // They part at the bounce's share of the speed they met at.
  CHECK(std::abs((b.vx - a.vx) - fs::kBounce * 4) < 1e-12);
  CHECK(a.vz == 10 && b.vz == 10);
  // Parting already, or apart: nothing.
  state.events.clear();
  fs::meet_racers(state);
  CHECK(state.events.empty());
}

TEST(the_standings_put_the_first_home_first_and_a_finisher_ahead_of_the_field) {
  fs::SimOptions opts;
  opts.rivals = kRivals;
  const fs::RunReport r = fs::simulate_on(seed38(), 38, opts);
  for (const fs::FieldRow& f : r.field) {
    int32_t ahead = 1;
    for (const fs::FieldRow& g : r.field) ahead += g.time < f.time ? 1 : 0;
    CHECK_MSG(f.place == ahead, "a place that does not follow the finish clocks");
  }
  CHECK(r.place == r.field[0].place);
  // Mid-run: a racer through more gates is placed ahead.
  fs::GameState state = fs::create_game_on(seed38(), 2);
  state.racers[1].progress.next = 3;
  CHECK(fs::place_of(state, 1) == 1 && fs::place_of(state, 0) == 2);
  state.racers[0].progress.finished = true;
  state.racers[0].progress.finish_time = 100;
  CHECK(fs::place_of(state, 0) == 1);
}

TEST(a_fields_run_played_back_off_the_players_tape_is_the_same_run) {
  fs::SimOptions opts;
  opts.rivals = kRivals;
  opts.max_seconds = 30;
  const fs::RunReport run = fs::simulate_on(seed38(), 38, opts);
  const fs::RunReport again = fs::replay_tape(seed38(), 38, run.tape, kRivals);
  CHECK_MSG(again.digest == run.digest && again.steps == run.steps,
            "the rivals rode differently on the replay: a rival's controls must be a pure "
            "function of the run (games/fall_line/sim/bot.cpp, field.cpp)");
}
