// Fall Line's bot explained (games/fall_line/sim/bot.hpp's records, engine/play/why.hpp): seed
// 38 with a field of four, the pilot's first 20 s replayed with every agent traced keeps the
// log's digest (recording moves no decision), every racer records the mode, the line and the
// speed every step, the pilot agrees with its own log at every step, and each speed record says
// what the controls did: a brake recorded where the log brakes, a tuck where it tucks.
//
// ge:summary Tests: Fall Line's bot decisions traced on seed 38 move no digest and match its input
// ge:keywords test fall_line bot why decisions trace digest rivals speed brake tuck agrees
// ge:skill bots sim testing
// ge:test tests/fall_line_why_test.cpp
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/play.hpp"
#include "play/session.hpp"
#include "play/why.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;

// The axes in the spec's order: steer, tuck, brake, jump, reset.
constexpr size_t kTuck = 1, kBrake = 2;
constexpr int kSteps = 2400;

gp::InputLog pilot_log() {
  gp::Setup s;
  std::string error;
  gp::make_setup(fs::kGame, 38, {{"rivals", "3"}}, s, error);
  gp::Session session(fs::kGame, s);
  std::vector<double> in(fs::kGame.axes.size(), 0);
  for (int i = 0; i < kSteps && !session.done(); i++) {
    session.run().pilot(in);
    session.step(in);
  }
  return session.inputs();
}

}  // namespace

TEST(the_bots_decisions_traced_move_no_digest_and_say_what_the_controls_did) {
  const gp::InputLog log = pilot_log();
  std::string error;
  const auto w = gp::why_replay(fs::kGame, log, ge::ai::Why::kEvery, error);
  CHECK_MSG(w.has_value(), error);
  if (!w) return;
  CHECK(log.digest && w->digest == *log.digest);
  CHECK(w->asked_pilot);
  for (const uint8_t a : w->agrees) CHECK(a == 1);
  std::vector<int> per_agent(4, 0);
  int speeds = 0;
  for (const ge::ai::Decision& d : w->why.decisions()) {
    CHECK(d.agent >= 0 && d.agent < 4);
    per_agent[static_cast<size_t>(d.agent)]++;
    if (d.agent != 0 || std::string(d.name) != "speed") continue;
    speeds++;
    const auto in = log.at(d.step);
    const std::string chosen = d.chosen;
    CHECK((chosen == "brake") == (in[kBrake] > 0));
    CHECK((chosen == "tuck") == (in[kTuck] > 0));
    CHECK(chosen == "glide" || d.margin > 0);
  }
  CHECK(speeds > kSteps / 2);
  for (const int n : per_agent) CHECK(n >= 3 * kSteps / 2);
}
