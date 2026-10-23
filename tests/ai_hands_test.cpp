// The bot's hands (engine/ai/hands.hpp): default hands pass everything through, the delay is
// exact, the slew holds the move under its limit, a spring under critical damping overshoots and
// settles, the wander stays in its amplitude and repeats from its seed, the reach clamps, a
// passed axis is untouched, and record_intent writes the "hands" decision.
//
// ge:summary Tests: ai::Hands' delay, slew, spring, wander, reach and pass; record_intent
// ge:keywords test ai hands delay slew spring overshoot wander reach intent
// ge:skill bots testing
// ge:test tests/ai_hands_test.cpp
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "ai/hands.hpp"
#include "ai/why.hpp"
#include "check.hpp"

namespace ai = ge::ai;

namespace {

constexpr double kHz = 120;

// One axis's outputs for `in` (one value a step).
std::vector<double> run(const ai::AxisHands& spec, const std::vector<double>& in,
                        uint32_t seed = 1) {
  const std::array<ai::AxisHands, 1> axes{spec};
  ai::Hands h(axes, kHz, seed);
  std::vector<double> out;
  for (const double v : in) {
    std::array<double, 1> o{};
    h.step(std::span<const double>(&v, 1), o);
    out.push_back(o[0]);
  }
  return out;
}

std::vector<double> step_input(size_t n, size_t at, double from, double to) {
  std::vector<double> v(n, from);
  for (size_t i = at; i < n; i++) v[i] = to;
  return v;
}

}  // namespace

TEST(default_hands_pass_every_value_through_unchanged) {
  const std::vector<double> in{0, 0.3, -1, 1, 0.25, -0.5};
  CHECK(run(ai::AxisHands{}, in) == in);
}

TEST(the_delay_is_exact_in_steps) {
  ai::AxisHands s;
  s.delay_s = 0.1;  // 12 steps
  const auto out = run(s, step_input(40, 5, 0, 1));
  CHECK(out[16] == 0 && out[17] == 1);
}

TEST(the_slew_holds_the_move_a_step_under_its_limit) {
  ai::AxisHands s;
  s.slew = 6;  // a twentieth a step at 120 Hz
  const auto out = run(s, step_input(60, 0, 0, 1));
  for (size_t i = 1; i < out.size(); i++) CHECK(out[i] - out[i - 1] <= 6 / kHz + 1e-12);
  CHECK(out.back() == 1);
}

TEST(an_underdamped_hand_overshoots_and_settles_a_critically_damped_one_does_not) {
  ai::AxisHands s;
  s.settle_s = 0.2;
  s.damping = 0.5;
  s.lo = -2;
  s.hi = 2;
  const auto under = run(s, step_input(240, 0, 0, 1));
  double most = 0;
  for (const double v : under) most = v > most ? v : most;
  CHECK(most > 1.05);
  CHECK(std::fabs(under.back() - 1) < 0.01);
  s.damping = 1;
  const auto crit = run(s, step_input(240, 0, 0, 1));
  for (const double v : crit) CHECK(v <= 1 + 1e-9);
  CHECK(std::fabs(crit.back() - 1) < 0.01);
}

TEST(the_wander_stays_in_its_amplitude_and_repeats_from_its_seed) {
  ai::AxisHands s;
  s.wander = 0.1;
  s.wander_hz = 2;
  const std::vector<double> still(1200, 0.4);
  const auto a = run(s, still, 7);
  const auto b = run(s, still, 7);
  const auto c = run(s, still, 8);
  CHECK(a == b && a != c);
  double most = 0;
  for (const double v : a) most = std::fabs(v - 0.4) > most ? std::fabs(v - 0.4) : most;
  CHECK(most > 0.01 && most <= 0.1);
  // Let go, or pressed to a stop, it sits still.
  CHECK(run(s, std::vector<double>(1200, 0), 7) == std::vector<double>(1200, 0));
  CHECK(run(s, std::vector<double>(1200, 1), 7) == std::vector<double>(1200, 1));
}

TEST(the_reach_clamps_and_a_passed_axis_is_untouched) {
  ai::AxisHands s;
  s.lo = 0;
  s.hi = 0.8;
  const auto out = run(s, {1, -1, 0.5});
  CHECK(out[0] == 0.8 && out[1] == 0 && out[2] == 0.5);
  ai::AxisHands p;
  p.pass = true;
  p.delay_s = 1;
  CHECK(run(p, {3, 2}) == (std::vector<double>{3, 2}));
}

TEST(reset_puts_the_hands_at_rest_where_they_are_told) {
  ai::AxisHands s;
  s.delay_s = 0.05;
  const std::array<ai::AxisHands, 1> axes{s};
  ai::Hands h(axes, kHz, 1);
  const std::array<double, 1> at{0.5};
  h.reset(at);
  std::array<double, 1> o{};
  const std::array<double, 1> zero{0};
  h.step(zero, o);
  CHECK(o[0] == 0.5);
}

TEST(record_intent_writes_the_hands_decision_with_a_fact_an_axis) {
  ai::Why why;
  constexpr std::array<const char*, 2> kNames{"steer", "brake"};
  const std::array<double, 2> wanted{0.25, 1};
  ai::record_intent(why.follow(0, 9), kNames, wanted);
  ai::record_intent(nullptr, kNames, wanted);
  CHECK(why.decisions().size() == 1);
  const ai::Decision& d = why.decisions()[0];
  CHECK(std::string(d.name) == "hands" && d.step == 9);
  CHECK(why.fact_of(d, "steer") == 0.25 && why.fact_of(d, "brake") == 1);
}
