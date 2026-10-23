// Fall Line's hand profiles (games/fall_line/sim/hands.hpp): the names parse, `none` presses what
// the bot wants and moves no digest (the sim's and the play contract's), `human` presses it late
// and leads the bot's look, a pilot on human hands still finishes seed 38 with a field of four,
// and its log replays to its digest with the pilot agreeing at every step (pilot() asks on a
// copy of the hands; only the step commits it).
//
// ge:summary Tests: Fall Line's none and human hands: no digest moved, late presses, replays agree
// ge:keywords test fall_line hands profile human none digest replay pilot copy lead finish
// ge:skill bots sim testing
// ge:test tests/fall_line_hands_test.cpp
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/hands.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/simulate.hpp"
#include "play/session.hpp"
#include "play/why.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;

constexpr int kSteps = 2400;

// The pilot's first `kSteps` on seed 38 with a field of four, under `options`.
gp::InputLog pilot_log(const std::map<std::string, std::string>& options) {
  gp::Setup s;
  std::string error;
  CHECK_MSG(gp::make_setup(fs::kGame, 38, options, s, error), error);
  gp::Session session(fs::kGame, s);
  std::vector<double> in(fs::kGame.axes.size(), 0);
  for (int i = 0; i < kSteps && !session.done(); i++) {
    session.run().pilot(in);
    session.step(in);
  }
  return session.inputs();
}

}  // namespace

TEST(the_profiles_parse_by_name_and_nothing_else) {
  CHECK(fs::hands_profile("none") == fs::HandsProfile::none);
  CHECK(fs::hands_profile("human") == fs::HandsProfile::human);
  CHECK(!fs::hands_profile("robot").has_value());
  CHECK(std::string(fs::hands_name(fs::HandsProfile::human)) == "human");
  CHECK(fs::hands_seed(38, 0) != fs::hands_seed(38, 1));
}

TEST(no_hands_press_what_the_bot_wants_and_human_hands_press_it_late) {
  fs::BotHands none(fs::HandsProfile::none, 1);
  fs::BotHands human(fs::HandsProfile::human, 1);
  CHECK(none.lead() == 0 && human.lead() > 0.2);
  fs::Controls want;
  want.ski.steer = 1;
  CHECK(none.press(want, nullptr).ski.steer == 1);
  // 0.2 s at 120 Hz is 24 steps before the thumb moves at all.
  for (int i = 0; i < 20; i++) CHECK(human.press(want, nullptr).ski.steer == 0);
  double steer = 0;
  for (int i = 0; i < 120; i++) steer = human.press(want, nullptr).ski.steer;
  CHECK(steer > 0.9);
}

TEST(no_hands_move_no_digest_of_the_sim_or_the_play_contract) {
  fs::SimOptions opts;
  opts.max_seconds = 20;
  const fs::RunReport plain = fs::simulate(38, opts);
  opts.hands = fs::HandsProfile::none;
  CHECK(fs::simulate(38, opts).digest == plain.digest);
  const gp::InputLog a = pilot_log({{"rivals", "3"}});
  const gp::InputLog b = pilot_log({{"rivals", "3"}, {"hands", "none"}});
  CHECK(a.digest && b.digest && *a.digest == *b.digest);
}

TEST(a_pilot_on_human_hands_finishes_seed_38_in_a_field_of_four) {
  fs::SimOptions opts;
  opts.rivals = 3;
  opts.hands = fs::HandsProfile::human;
  const fs::RunReport r = fs::simulate(38, opts);
  CHECK(r.finished);
  for (const fs::FieldRow& row : r.field) CHECK(row.finished);
}

TEST(a_human_hands_log_replays_to_its_digest_and_the_pilot_agrees_at_every_step) {
  const gp::InputLog log = pilot_log({{"rivals", "3"}, {"hands", "human"}});
  std::string error;
  const auto w = gp::why_replay(fs::kGame, log, 0, error);
  CHECK_MSG(w.has_value(), error);
  if (!w) return;
  CHECK(log.digest && w->digest == *log.digest);
  CHECK(w->asked_pilot);
  for (const uint8_t a : w->agrees) CHECK(a == 1);
  int hands = 0;
  for (const ge::ai::Decision& d : w->why.decisions()) {
    if (std::string(d.name) == "hands") hands++;
  }
  CHECK(hands == kSteps);
}
