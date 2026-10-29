// A RUN SAVED AND LOADED, HELD (engine/play/saves.hpp over Fall Line's Run::save and Run::load):
// the pilot's run on seed 38, saved at the start, a step in, mid-run and near the end, loaded
// into a fresh run and played on along the same log, has the digest of the run never saved at
// every later step; so does a field of rivals with human hands. A save carries its setup and the
// step it was made at; another game's save, a damaged one and a game that keeps none are refused
// with a line that says what to do.
//
// ge:summary Test: a Fall Line run saved and loaded steps on to the digests of the run never saved
// ge:keywords test save load check_saves digest round-trip refuse damaged setup meta pilot
// ge:skill save testing
// ge:test tests/play_saves_test.cpp
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/play.hpp"
#include "play/saves.hpp"
#include "play/session.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;

gp::Setup setup(int rivals, const char* hands = "none") {
  gp::Setup s;
  std::string error;
  gp::make_setup(fs::kGame, 38, {{"rivals", std::to_string(rivals)}, {"hands", hands}}, s, error);
  return s;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

// The pilot's log on `st`, and each save made along it checked against the straight run.
void check_log(const gp::Setup& st) {
  std::string error;
  const auto played = gp::pilot_run(fs::kGame, st, error);
  CHECK_MSG(played.has_value(), error);
  if (!played) return;
  const int64_t n = played->log.steps();
  const std::vector<int64_t> at{0, 1, 600, n / 2, n - 5, n};
  const auto saved = gp::check_saves(fs::kGame, played->log, at, error);
  CHECK_MSG(saved.size() == at.size(), error);
  for (const gp::SavedAt& s : saved)
    CHECK_MSG(s.parted == -1, "the run saved at step " + std::to_string(s.step) +
                                  " and loaded parts from the run never saved at step " +
                                  std::to_string(s.parted) +
                                  ": a field the step writes is missing from a describe "
                                  "(run `make lab LAB=save-run` to name the chunk)");
}

}  // namespace

TEST(a_run_saved_and_loaded_steps_on_as_the_run_never_saved) { check_log(setup(0)); }

TEST(a_field_with_human_hands_saved_and_loaded_steps_on_as_the_run_never_saved) {
  check_log(setup(3, "human"));
}

TEST(a_save_carries_its_setup_and_its_step) {
  const gp::Setup st = setup(1);
  std::string error;
  auto run = fs::kGame.open(st, error);
  CHECK_MSG(run != nullptr, error);
  if (!run) return;
  std::vector<double> in(fs::kGame.axes.size(), 0);
  for (int i = 0; i < 240; i++) {
    run->pilot(in);
    run->step(in);
  }
  ge::save::Meta meta;
  meta.title = "Seed 38";
  std::vector<uint8_t> bytes;
  CHECK_MSG(gp::save_run(fs::kGame, st, *run, 240, meta, bytes, error), error);
  ge::save::FileReader r;
  CHECK_MSG(r.open(bytes, "run.save", "fall_line"), r.problem());
  CHECK(r.meta().title == "Seed 38" && r.meta().game == "fall_line");
  CHECK(r.meta().play_s == 240.0 / fs::kGame.hz);
  CHECK(has(r.meta().setup, "rivals"));
  int64_t step = -1;
  auto loaded = gp::load_run(fs::kGame, bytes, step, error, "run.save");
  CHECK_MSG(loaded != nullptr, error);
  if (!loaded) return;
  CHECK(step == 240);
  CHECK(loaded->digest() == run->digest());
  CHECK(loaded->state() == run->state());
}

TEST(another_games_save_and_a_damaged_one_are_refused) {
  const gp::Setup st = setup(0);
  std::string error;
  auto run = fs::kGame.open(st, error);
  if (!run) return;
  std::vector<uint8_t> bytes;
  CHECK(gp::save_run(fs::kGame, st, *run, 0, {}, bytes, error));
  gp::GameSpec other = fs::kGame;
  other.name = "other";
  int64_t step = 0;
  CHECK(gp::load_run(other, bytes, step, error) == nullptr);
  CHECK_MSG(has(error, "fall_line"), error);
  std::vector<uint8_t> cut(bytes.begin(), bytes.end() - 9);
  CHECK(gp::load_run(fs::kGame, cut, step, error) == nullptr);
  CHECK(!error.empty());
  bytes[bytes.size() / 2] ^= 0x40;
  CHECK(gp::load_run(fs::kGame, bytes, step, error) == nullptr);
  CHECK(!error.empty());
}
