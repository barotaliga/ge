// Fall Line's runs on a hub (games/fall_line/hub/runs.hpp): a race's input log played out into
// the archive's row, its racers' times read at the line; another game's log refused, and so is a
// run edited after it was recorded or one without its digest.
//
// ge:summary Test: Fall Line's runs on a hub: a race's log read into its archive row, times shown
// ge:keywords test hub run input-log run_row finish-time score_text audit forged-ghost
// ge:skill net testing
#include "fall_line/hub/runs.hpp"

#include <optional>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/play.hpp"
#include "hub/spec.hpp"
#include "play/inputs.hpp"
#include "play/session.hpp"

namespace sim = fall_line::sim;

namespace {

// The bot's own race down `seed` with `rivals` (the game's pilot, as `ge sim` drives it), as the
// game's input log text.
std::string bot_race(double seed, int rivals) {
  ge::play::Setup setup;
  std::string error;
  CHECK(ge::play::make_setup(sim::kGame, seed, {{"rivals", std::to_string(rivals)}}, setup, error));
  const std::optional<ge::play::Played> played = ge::play::pilot_run(sim::kGame, setup, error);
  CHECK(played.has_value() && played->over);
  return played ? ge::play::to_text(played->log) : std::string();
}

}  // namespace

TEST(a_race_s_log_is_read_into_its_row) {
  const std::string text = bot_race(38, 0);
  const std::vector<uint8_t> log(text.begin(), text.end());
  ge::hub::DemoRow row;
  std::string error;
  CHECK(fall_line::hub::run_row(log, row, error));
  CHECK(row.game == "fall_line" && row.map == "38" && row.mode == "race");
  CHECK(row.title == "A race on seed 38");
  CHECK(row.players.size() == 1 && row.players[0].name == "Player");
  // The bot finishes: a time at the line, in milliseconds, within the run's length.
  CHECK(row.players[0].scored && row.players[0].score > 10000);
  CHECK(row.duration_ms >= row.players[0].score);
}

TEST(a_run_edited_after_it_was_recorded_is_refused) {
  // A forged ghost: the bot's race with one held input changed, its recorded digest kept.
  const std::string text = bot_race(38, 0);
  std::string error;
  std::optional<ge::play::InputLog> log = ge::play::parse_inputs(text, error);
  CHECK(log.has_value() && log->digest.has_value() && log->runs.size() > 4);
  if (!log) return;
  ge::play::InputLog forged = *log;
  ge::play::InputRun& r = forged.runs[forged.runs.size() / 2];
  r.values[0] = r.values[0] > 0 ? -1.0 : 1.0;  // a turn the other way, held for that stretch
  const std::string forged_text = ge::play::to_text(forged);
  const std::vector<uint8_t> bytes(forged_text.begin(), forged_text.end());
  ge::hub::DemoRow row;
  CHECK(!fall_line::hub::run_row(bytes, row, error));
  CHECK_MSG(error.find("does not play to the digest") != std::string::npos, error);
  // And one with no digest at all cannot be checked, so it is refused too.
  ge::play::InputLog bare = *log;
  bare.digest.reset();
  const std::string bare_text = ge::play::to_text(bare);
  const std::vector<uint8_t> bare_bytes(bare_text.begin(), bare_text.end());
  CHECK(!fall_line::hub::run_row(bare_bytes, row, error));
  CHECK_MSG(error.find("no digest") != std::string::npos, error);
}

TEST(another_game_s_log_is_refused) {
  const std::string text = "ge-inputs 1\ngame downhill\nseed 7\nhz 120\naxes steer\nsteps 1\n1 0\n";
  const std::vector<uint8_t> log(text.begin(), text.end());
  ge::hub::DemoRow row;
  std::string error;
  CHECK(!fall_line::hub::run_row(log, row, error) && !error.empty());
}

TEST(a_time_is_shown_as_minutes_seconds_and_hundredths) {
  ge::hub::HubSpec spec;
  CHECK(ge::hub::score_text(spec, 41) == "41");
  spec.score_ms = true;
  CHECK(ge::hub::score_text(spec, 239685) == "3:59.68");
  CHECK(ge::hub::score_text(spec, 61005) == "1:01.00");
}
