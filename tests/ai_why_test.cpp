// The decision record (engine/ai/why.hpp): a decision's facts, options, terms and bars, the
// margin over the best other option not barred, a decision with no options, the agent a trace
// follows, records made outside a decision dropped, clear, and both printed forms.
//
// ge:summary Tests: ai::Why's records, margin, bars, follow, and its text and JSON forms
// ge:keywords test ai why decision margin bar follow decision_text decision_json
// ge:skill bots testing
// ge:test tests/ai_why_test.cpp
#include <cmath>
#include <string>

#include "ai/why.hpp"
#include "check.hpp"

namespace ai = ge::ai;

namespace {

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(a_decision_keeps_its_facts_options_and_terms_and_its_margin) {
  ai::Why why;
  ai::Why* w = why.follow(2, 40);
  CHECK(w == &why);
  w->decide("speed");
  w->fact("speed", 12);
  w->fact("cap", 10);
  w->option("brake", 1.5);
  w->term("over_plan", 2);
  w->term("deadband", -0.5);
  w->option("tuck", 3);
  w->bar("turning");
  w->option("glide", 0);
  w->choose("brake", "over_plan");
  CHECK(why.decisions().size() == 1);
  const ai::Decision& d = why.decisions()[0];
  CHECK(d.agent == 2 && d.step == 40 && std::string(d.name) == "speed");
  CHECK(std::string(d.chosen) == "brake" && std::string(d.because) == "over_plan");
  // The barred tuck scores higher but takes no part: the margin is over the glide.
  CHECK(d.margin == 1.5);
  CHECK(why.facts(d).size() == 2 && why.fact_of(d, "cap") == 10);
  CHECK(std::isnan(why.fact_of(d, "missing")));
  CHECK(why.options(d).size() == 3);
  CHECK(why.terms(why.options(d)[0]).size() == 2);
  CHECK(why.options(d)[1].barred && std::string(why.options(d)[1].barred) == "turning");
}

TEST(a_decision_with_no_options_has_no_margin) {
  ai::Why why;
  why.follow(0, 0)->decide("mode");
  why.fact("speed", 3);
  why.choose("reset", "missed_gate");
  CHECK(std::isnan(why.decisions()[0].margin));
}

TEST(a_trace_of_one_agent_hands_out_nothing_for_another) {
  ai::Why why(1);
  CHECK(why.follow(0, 5) == nullptr);
  CHECK(why.follow(1, 5) == &why);
  CHECK(ai::Why().follow(7, 5) != nullptr);
}

TEST(records_outside_a_decision_are_dropped_and_clear_empties_it) {
  ai::Why why;
  why.fact("stray", 1);
  why.option("stray", 1);
  why.term("stray", 1);
  why.choose("stray", "none");
  CHECK(why.decisions().empty());
  why.decide("line");
  why.term("no_option_yet", 1);
  why.choose("left", "pursuit");
  CHECK(why.decisions().size() == 1 && why.options(why.decisions()[0]).empty());
  why.clear();
  CHECK(why.decisions().empty());
}

TEST(the_text_and_the_json_say_what_it_saw_weighed_and_chose) {
  ai::Why why;
  why.follow(0, 1234)->decide("speed");
  why.fact("speed", 21.25);
  why.option("brake", 2);
  why.term("over_plan", 2.5);
  why.term("deadband", -0.5);
  why.option("tuck", -3);
  why.bar("turning");
  why.option("glide", 0);
  why.choose("brake", "over_plan");
  const ai::Decision& d = why.decisions()[0];
  const std::string text = ai::decision_text(why, d);
  CHECK_MSG(has(text, "step 1234 agent 0 speed: brake by 2.000 (over_plan)"), text);
  CHECK_MSG(has(text, "saw speed=21.250"), text);
  CHECK_MSG(has(text, "brake 2.000 = over_plan 2.500 + deadband -0.500"), text);
  CHECK_MSG(has(text, "tuck -3.000 (barred: turning)"), text);
  const std::string json = ai::decision_json(why, d);
  CHECK_MSG(has(json, "\"chosen\":\"brake\",\"because\":\"over_plan\",\"margin\":2"), json);
  CHECK_MSG(has(json, "\"facts\":{\"speed\":21.25}"), json);
  CHECK_MSG(has(json, "{\"name\":\"tuck\",\"score\":-3,\"barred\":\"turning\",\"terms\":{}}"),
            json);
}

TEST(no_margin_prints_as_null_in_json) {
  ai::Why why;
  why.follow(0, 0)->decide("mode");
  why.choose("ride", "on_course");
  CHECK(has(ai::decision_json(why, why.decisions()[0]), "\"margin\":null"));
}
