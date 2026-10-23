// THE DECISION RECORD: why a bot did what it did, written where it chose. A bot that holds a
// `Why*` records each DECISION it makes in a step: its name ("speed", "line"), the FACTS it read
// (named numbers: what it perceived and what it planned on), the OPTIONS it weighed (each a
// score, the sum of named TERMS when it has them, or BARRED with a reason), and the option it
// took with the term or condition that settled it. `ge why` replays a run with one switched on
// and reads the records back (engine/play/why.hpp), so nothing is kept in a normal run.
//
// THE CONTRACT a bot keeps:
// - Off is a null pointer: `if (why) why->fact(...)`. A bot must decide exactly the same with a
//   trace on and off; a test of every game that records holds its digests equal both ways.
// - Every name (a decision's, a fact's, an option's, a term's, a reason) is a string literal:
//   the record keeps the pointer.
// - `decide` opens a decision, `choose` closes it; facts and options in between are its. The
//   choice is the bot's own (the record never decides): the MARGIN is the chosen option's score
//   over the best other option not barred, so a close call reads as a small margin, and a
//   decision with no options (a guard, a controller's output) records its facts and its choice
//   with a margin of NaN.
// - `follow(agent, step)` hands out the recorder for one agent at one step, or null when this
//   trace follows another agent: the caller passes the result straight to the bot.
//
// Each closed decision is also a line on the `why` log channel at debug (`--log=why=debug`),
// carrying its step like every line, so it lines up with the rest of a run's log.
//
// ge:summary The decision record: a bot's facts, options, scores and choice, for ge why
// ge:keywords ai bot why decision trace record explain fact option term score margin choose
// ge:keywords decide bar barred follow agent utility traceable decision-trace ge-why
// ge:skill bots debug
// ge:test tests/ai_why_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::ai {

struct Fact {
  const char* name;
  double value;
};

struct Term {
  const char* name;
  double value;
};

struct Choice {
  const char* name;
  double score;
  const char* barred = nullptr;  // why it was ruled out, or null
  uint32_t first_term = 0, terms = 0;
};

struct Decision {
  int32_t agent = 0;
  int64_t step = 0;
  const char* name = "";
  uint32_t first_fact = 0, facts = 0;
  uint32_t first_option = 0, options = 0;
  const char* chosen = "";   // the option taken (or, with no options, what it did)
  const char* because = "";  // the term or condition that settled it
  double margin = 0;         // its score over the best other option not barred; NaN with none
};

class Why {
 public:
  static constexpr int32_t kEvery = -1;
  // A trace of `agent`, or of every agent.
  explicit Why(int32_t agent = kEvery) : agent_(agent) {}

  int32_t agent() const { return agent_; }
  // The recorder for `agent` at `step`, or null when this trace follows another agent.
  Why* follow(int32_t agent, int64_t step);

  void decide(const char* name);
  void fact(const char* name, double value);
  void option(const char* name, double score);
  // A named part of the last option's score.
  void term(const char* name, double value);
  // The last option is ruled out, for `reason`: it takes no part in the margin.
  void bar(const char* reason);
  // The decision's choice and what settled it; closes the decision.
  void choose(const char* chosen, const char* because);

  std::span<const Decision> decisions() const { return decisions_; }
  std::span<const Fact> facts(const Decision& d) const;
  std::span<const Choice> options(const Decision& d) const;
  std::span<const Term> terms(const Choice& c) const;
  // The fact called `name` of `d`, or NaN.
  double fact_of(const Decision& d, const char* name) const;
  // Every record dropped (the agent followed is kept).
  void clear();

 private:
  int32_t agent_;
  int32_t at_agent_ = 0;
  int64_t at_step_ = 0;
  bool open_ = false;
  std::vector<Decision> decisions_;
  std::vector<Fact> facts_;
  std::vector<Choice> options_;
  std::vector<Term> terms_;
};

// One decision in words, the way `ge why` prints it:
//   step 1234 agent 0 speed: brake by 2.1 (over_plan)
//     saw speed=21.3 cap=18.2
//     brake 2.1 = over_plan 2.6 + deadband -0.5
//     tuck -3.4 (barred: turning)
std::string decision_text(const Why& why, const Decision& d);
// The same as one JSON object: agent, step, name, chosen, because, margin, facts {name: value},
// options [{name, score, barred, terms {name: value}}].
std::string decision_json(const Why& why, const Decision& d);

}  // namespace ge::ai
