// AN AGENT AT THE CONTROLS OF FALL LINE, HELD (games/fall_line/sim/play.hpp on the engine's play
// session): the protocol's answers to a scripted session, its refusals, an agent that sends the
// bot's own moves riding the bot's own run to the bit, the pilot's run equal to the bot's, an
// agent's input log played back to its digest, and two logs one input apart diverging there.
//
// ge:summary Test: Fall Line's play protocol, its errors, replay to the digest, diverge's step
// ge:keywords test play agent protocol json observe act pilot inputs end replay digest diverge
// ge:skill sim agent-interface testing
// ge:test tests/fall_line_play_test.cpp
#include <memory>
#include <string>

#include "check.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/simulate.hpp"
#include "play/session.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace gp = ge::play;

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::race_map(38));
  return level;
}

gp::Setup setup(int rivals) {
  gp::Setup s;
  std::string error;
  gp::make_setup(fs::kGame, 38, {{"rivals", std::to_string(rivals)}}, s, error);
  return s;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

std::string act_line(const fs::Controls& c) {
  namespace num = ge::core::num;
  return "{\"op\":\"act\",\"input\":{\"steer\":" + num::to_string(c.ski.steer) +
         ",\"tuck\":" + num::to_string(c.ski.tuck) + ",\"brake\":" + num::to_string(c.ski.brake) +
         ",\"jump\":" + (c.ski.jump ? "true" : "false") +
         ",\"reset\":" + (c.reset ? "true" : "false") + "}}";
}

}  // namespace

TEST(a_scripted_session_is_answered_line_for_line) {
  gp::Session s(fs::kGame, setup(1));
  CHECK_MSG(s.ok(), s.error());
  const std::string hello = s.hello();
  CHECK(has(hello, "\"game\":\"fall_line\"") && has(hello, "\"name\":\"steer\"") &&
        has(hello, "\"rivals\":\"1\""));
  const std::string first = s.handle("{\"op\":\"observe\"}");
  CHECK(has(first, "\"op\":\"observe\"") && has(first, "\"step\":0") && has(first, "\"gate\":0"));
  const std::string after =
      s.handle(" { \"op\" : \"act\", \"input\": {\"tuck\": 1}, \"steps\": 240 } ");
  CHECK(has(after, "\"step\":240") && has(after, "\"racers\":2"));
  const std::string state = s.handle("{\"op\":\"state\"}");
  CHECK(has(state, "\"op\":\"state\"") && has(state, "\"steps\":240") && has(state, "\"vx\":"));
  const std::string inputs = s.handle("{\"op\":\"inputs\"}");
  CHECK(has(inputs, "\"steps\":240") && has(inputs, "axes steer tuck brake jump reset"));
  const std::string piloted = s.handle("{\"op\":\"pilot\",\"steps\":120}");
  CHECK(has(piloted, "\"step\":360"));
  const std::string end = s.handle("{\"op\":\"end\"}");
  CHECK(has(end, "\"op\":\"end\"") && has(end, "\"digest\":\"") && has(end, "\"gate_count\":"));
  CHECK(s.ended());
  CHECK(has(s.handle("{\"op\":\"observe\"}"), "\"op\":\"error\""));
}

TEST(a_bad_line_is_refused_with_what_to_send_instead) {
  gp::Session s(fs::kGame, setup(0));
  const char* bad[] = {
      "",
      "not json",
      "[1,2]",
      "{\"op\":\"observe\"} trailing",
      "{\"op\":7}",
      "{\"op\":\"fly\"}",
      "{\"op\":\"act\",\"input\":{\"steer\":\"left\"}}",
      "{\"op\":\"act\",\"input\":{\"thrust\":1}}",
      "{\"op\":\"act\",\"steps\":0}",
      "{\"op\":\"act\",\"steps\":100000}",
      "{\"op\":\"act\",\"input\":[1]}",
      "{\"op\":\"unterminated}",
  };
  for (const char* line : bad) {
    const std::string got = s.handle(line);
    CHECK_MSG(has(got, "\"op\":\"error\"") && has(got, "\"next\":"),
              std::string("not refused: ") + line + " -> " + got);
  }
  // Nothing refused moved the run.
  CHECK(s.steps() == 0);
}

TEST(an_agent_sending_the_bots_moves_rides_the_bots_run_to_the_bit) {
  fs::SimOptions opts;
  opts.max_seconds = 20;
  opts.rivals = 1;
  const fs::RunReport bot = fs::simulate_on(seed38(), 38, opts);
  gp::Session s(fs::kGame, setup(1));
  fs::Driver shadow(seed38(), 1);
  while (s.steps() < bot.steps) {
    const fs::Controls c = fs::bot_controls(shadow.state(), 0, shadow.plan());
    shadow.step(c);
    CHECK(!has(s.handle(act_line(c)), "\"op\":\"error\""));
  }
  CHECK_MSG(s.run().digest() == bot.digest,
            "the agent's run left the bot's: the protocol's reader or the session's snap "
            "(engine/play/session.cpp, games/fall_line/sim/play.cpp) changed a control");
}

TEST(the_pilot_rides_the_bots_run_and_its_log_replays_to_its_digest) {
  gp::Setup st = setup(2);
  st.options["rivals"] = "2";
  fs::SimOptions opts;
  opts.rivals = 2;
  const fs::RunReport bot = fs::simulate_on(seed38(), 38, opts);
  std::string error;
  const auto played = gp::pilot_run(fs::kGame, st, error);
  CHECK_MSG(played.has_value(), error);
  if (!played) return;
  CHECK_MSG(played->digest == bot.digest && played->steps == bot.steps,
            "the pilot's run is not the bot's (games/fall_line/sim/play.cpp's pilot)");
  const auto text = gp::to_text(played->log);
  const auto log = gp::parse_inputs(text, error);
  CHECK_MSG(log.has_value(), error);
  if (!log) return;
  const auto again = gp::replay(fs::kGame, *log, error);
  CHECK_MSG(again && log->digest && again->digest == *log->digest,
            "the pilot's input log played back to a different run");
}

TEST(two_logs_one_input_apart_diverge_at_that_step) {
  gp::Session s(fs::kGame, setup(0));
  s.handle("{\"op\":\"act\",\"input\":{\"tuck\":1},\"steps\":600}");
  s.handle("{\"op\":\"act\",\"input\":{\"steer\":0.7,\"brake\":0.2},\"steps\":240}");
  s.handle("{\"op\":\"act\",\"input\":{\"jump\":true},\"steps\":30}");
  s.handle("{\"op\":\"act\",\"input\":{\"reset\":true},\"steps\":120}");
  const gp::InputLog a = s.inputs();
  std::string error;
  const auto again = gp::replay(fs::kGame, a, error);
  CHECK_MSG(again && a.digest && again->digest == *a.digest,
            "an agent's log played back to another run");

  gp::InputLog b = a;
  // Split the steer run at its 101st step and steer the other way there for one step.
  gp::InputRun& steer = b.runs[1];
  gp::InputRun before{100, steer.values}, flip{1, steer.values},
      rest{steer.count - 101, steer.values};
  flip.values[0] = -0.7;
  b.runs.erase(b.runs.begin() + 1);
  b.runs.insert(b.runs.begin() + 1, {before, flip, rest});
  const auto d = gp::diverge(fs::kGame, a, b, error);
  CHECK_MSG(d.has_value(), error);
  if (!d) return;
  CHECK(!d->same && !d->setup_differs);
  CHECK_MSG(d->input_step == 700, "input_step " + std::to_string(d->input_step));
  CHECK_MSG(d->state_step == 701, "state_step " + std::to_string(d->state_step));
  bool yaw = false;
  for (const gp::FieldDiff& f : d->fields) yaw = yaw || f.key == "racers.0.yaw_rate";
  CHECK_MSG(yaw, "the first differing state does not name the skier's yaw rate");
  const auto same = gp::diverge(fs::kGame, a, a, error);
  CHECK(same && same->same && same->state_step < 0 && same->input_step < 0);
}
