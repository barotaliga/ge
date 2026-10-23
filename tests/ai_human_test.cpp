// The humanness metrics (engine/ai/human.hpp): a bang-bang signal saturates and jumps, a smooth
// one does neither; reversals are counted with hysteresis; the hold is the median run; the lag
// finds a known delay; a decision's flips, close calls and variety; the bands flag what is out.
//
// ge:summary Tests: axis_humanness, decision_humanness and the out-of-band check
// ge:keywords test ai humanness saturation jumps reversals hold lag flips variety bands
// ge:skill bots testing
// ge:test tests/ai_human_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "ai/human.hpp"
#include "ai/why.hpp"
#include "check.hpp"

namespace ai = ge::ai;

namespace {

constexpr double kHz = 120;

// A triangle wave between -a and a with `period` steps.
std::vector<double> triangle(size_t n, size_t period, double a) {
  std::vector<double> v(n);
  for (size_t i = 0; i < n; i++) {
    const double p = static_cast<double>(i % period) / static_cast<double>(period);
    v[i] = a * (p < 0.5 ? 4 * p - 1 : 3 - 4 * p);
  }
  return v;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(a_bang_bang_signal_saturates_and_jumps_and_a_smooth_one_does_neither) {
  std::vector<double> bang(1200);
  for (size_t i = 0; i < bang.size(); i++) bang[i] = (i / 30) % 2 ? 1 : -1;
  const ai::AxisHumanness b = ai::axis_humanness(bang, {}, -1, 1, 0, kHz);
  CHECK(b.saturated == 1);
  CHECK(b.jumps > 0.02);
  CHECK(std::isnan(b.lag_s));
  const ai::AxisHumanness s = ai::axis_humanness(triangle(1200, 120, 0.8), {}, -1, 1, 0, kHz);
  CHECK(s.saturated == 0 && s.jumps == 0);
}

TEST(reversals_count_changes_of_direction_past_the_hysteresis_only) {
  // Ten periods of a triangle in 10 s: two turns a period, the first one not a reversal.
  const ai::AxisHumanness t = ai::axis_humanness(triangle(1200, 120, 0.8), {}, -1, 1, 0, kHz);
  CHECK(std::fabs(t.reversals_min - 19 * 6) <= 6);
  // A wobble under the hysteresis is no reversal at all.
  const ai::AxisHumanness w = ai::axis_humanness(triangle(1200, 12, 0.02), {}, -1, 1, 0, kHz);
  CHECK(w.reversals_min == 0);
}

TEST(the_hold_is_the_median_time_the_value_holds_still) {
  // Steps of 0.05 every half second, a tremor of 0.01 under them: holds of half a second.
  std::vector<double> v(1200);
  for (size_t i = 0; i < v.size(); i++) {
    const size_t held = i / 60;  // the half second it is in
    v[i] = static_cast<double>(held) * 0.05 + (i % 2 ? 0.01 : 0);
  }
  const ai::AxisHumanness h = ai::axis_humanness(v, {}, -1, 1, 0, kHz);
  CHECK(std::fabs(h.hold_s - 0.5) < 1e-9);
}

TEST(the_lag_finds_a_known_delay_between_wanted_and_pressed) {
  const std::vector<double> wanted = triangle(1200, 90, 0.7);
  std::vector<double> pressed(wanted.size(), wanted[0]);
  for (size_t i = 24; i < wanted.size(); i++) pressed[i] = wanted[i - 24];
  const ai::AxisHumanness h = ai::axis_humanness(pressed, wanted, -1, 1, 0, kHz);
  CHECK(std::fabs(h.lag_s - 0.2) < 1e-9);
  CHECK(ai::axis_humanness(wanted, wanted, -1, 1, 0, kHz).lag_s == 0);
}

TEST(a_decisions_flips_close_calls_and_variety) {
  ai::Why why;
  const char* chosen[] = {"brake", "brake", "tuck", "brake", "tuck", "tuck"};
  for (int64_t i = 0; i < 6; i++) {
    ai::Why* w = why.follow(0, i);
    w->decide("speed");
    w->option("brake", chosen[i][0] == 'b' ? 1 : 0.9);
    w->option("tuck", chosen[i][0] == 't' ? 1 : 0.2);
    w->choose(chosen[i], "test");
  }
  const ai::DecisionHumanness h = ai::decision_humanness(why, 0, "speed", 60, 0.25);
  CHECK(h.calls == 6);
  CHECK(h.flips_min == 3);
  CHECK(std::fabs(h.close_share - 0.5) < 1e-12);
  CHECK(std::fabs(h.variety - 1) < 1e-12);
  CHECK(ai::decision_humanness(why, 1, "speed", 60, 0.25).calls == 0);
}

TEST(the_bands_flag_what_is_out_and_say_why) {
  ai::AxisHumanness h;
  h.saturated = 0.9;
  h.lag_s = 0;
  h.hold_s = 0.5;
  std::vector<ai::OutOfBand> out;
  ai::check_axis("steer", ai::Feel::stick, h, out);
  CHECK(out.size() == 2);
  std::vector<ai::OutOfBand> trigger;
  ai::check_axis("tuck", ai::Feel::trigger, h, trigger);
  CHECK(trigger.size() == 1 && std::string(trigger[0].metric) == "lag_s");
  CHECK(out.size() == 2);
  const std::string text = ai::out_of_band_text(out[0]);
  CHECK(has(text, "steer saturated 0.900 (band 0.00..0.35): "));
  ai::DecisionHumanness d;
  d.flips_min = 56;
  ai::check_decision("speed", d, out);
  CHECK(out.size() == 3 && std::string(out[2].metric) == "flips_min");
}
