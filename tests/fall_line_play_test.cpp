// AN AGENT AT THE CONTROLS, HELD (games/fall_line/sim/play.hpp): the protocol's answers to a
// scripted session, its refusals, an agent that sends the bot's own moves riding the bot's own
// run to the bit, and an agent's tape played back to its digest.
//
// ge:summary Test: the play protocol scripted, its errors, an agent's run equal to the bot's
// ge:keywords test play agent protocol json observe act tape end replay digest session
// ge:skill sim agent-interface testing
#include <memory>
#include <string>

#include "check.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/simulate.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::race_map(38));
  return level;
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
  fs::PlaySession s(seed38(), 38, 1);
  const std::string hello = s.hello();
  CHECK(has(hello, "\"op\":\"hello\"") && has(hello, "\"racers\":2") &&
        has(hello, "\"hz\":120.000"));
  const std::string first = s.handle("{\"op\":\"observe\"}");
  CHECK(has(first, "\"op\":\"observe\"") && has(first, "\"step\":0") && has(first, "\"gate\":0") &&
        has(first, "\"events\":[]"));
  const std::string moved =
      s.handle(" { \"op\" : \"act\", \"input\": {\"tuck\": 1}, \"steps\": 240 } ");
  CHECK(has(moved, "\"op\":\"act\"") && has(moved, "\"step\":240") && has(moved, "\"t\":2.000"));
  const std::string tape = s.handle("{\"op\":\"tape\"}");
  CHECK(has(tape, "\"steps\":240") && has(tape, "\"steer\":") && has(tape, "\"flags\":"));
  const std::string end = s.handle("{\"op\":\"end\"}");
  CHECK(has(end, "\"op\":\"end\"") && has(end, "\"steps\":240") && has(end, "\"digest\":\""));
  CHECK(s.ended());
  CHECK(has(s.handle("{\"op\":\"observe\"}"), "\"op\":\"error\""));
}

TEST(a_bad_line_is_refused_with_what_to_send_instead) {
  fs::PlaySession s(seed38(), 38, 0);
  const char* bad[] = {
      "",
      "not json",
      "[1,2]",
      "{\"op\":\"observe\"} trailing",
      "{\"op\":7}",
      "{\"op\":\"fly\"}",
      "{\"op\":\"act\",\"input\":{\"steer\":\"left\"}}",
      "{\"op\":\"act\",\"input\":{\"jump\":1}}",
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
  CHECK(s.driver().state().steps == 0);
}

TEST(an_agent_sending_the_bots_moves_rides_the_bots_run_to_the_bit) {
  fs::SimOptions opts;
  opts.max_seconds = 20;
  opts.rivals = 1;
  const fs::RunReport bot = fs::simulate_on(seed38(), 38, opts);
  fs::PlaySession s(seed38(), 38, 1);
  while (s.driver().state().steps < bot.steps) {
    const fs::Controls c = fs::bot_controls(s.driver().state(), 0, s.driver().plan());
    CHECK(!has(s.handle(act_line(c)), "\"op\":\"error\""));
  }
  CHECK_MSG(s.driver().digest() == bot.digest,
            "the agent's run left the bot's: the protocol's reader or the session's step "
            "(games/fall_line/sim/play.cpp) changed a control on the way in");
}

TEST(an_agents_tape_plays_back_to_its_digest) {
  fs::PlaySession s(seed38(), 38, 2);
  // A clumsy agent: a long tuck, a hard turn, a jump held and let go, a reset.
  s.handle("{\"op\":\"act\",\"input\":{\"tuck\":1},\"steps\":600}");
  s.handle("{\"op\":\"act\",\"input\":{\"steer\":0.7,\"brake\":0.2},\"steps\":240}");
  s.handle("{\"op\":\"act\",\"input\":{\"jump\":true},\"steps\":30}");
  s.handle("{\"op\":\"act\",\"input\":{\"steer\":-1},\"steps\":360}");
  s.handle("{\"op\":\"act\",\"input\":{\"reset\":true},\"steps\":120}");
  const fs::RunReport again = fs::replay_tape(seed38(), 38, s.driver().tape(), 2);
  CHECK(again.steps == s.driver().state().steps);
  CHECK_MSG(again.digest == s.driver().digest(),
            "the agent's tape played back to a different run (games/fall_line/sim/play.cpp)");
}
