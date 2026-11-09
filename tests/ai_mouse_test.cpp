// THE MOUSE HAND (engine/ai/mouse.hpp): a 90-degree turn is made in a few strokes whose speed
// rises and falls as a bell (its peak well over its mean), never turning back at the step's rate;
// a long drag right runs the view down and one left runs it up (the arc); a turn past the pad's
// reach lifts the hand and still gets there; with a shot due the hand waits, then lands on the
// target as the shot comes due; the same seed moves the same bits.
//
// ge:summary Tests: the mouse hand's strokes, the arc's tilt, lifts, the shot-timed stroke, bits
// ge:keywords test mouse hand stroke minimum-jerk bell tilt arc lift shot-timing jitter
// ge:skill bots testing
#include <cmath>
#include <cstring>
#include <vector>

#include "ai/mouse.hpp"
#include "check.hpp"

namespace ai = ge::ai;

namespace {

constexpr double kDt = 1.0 / 77;

struct Run {
  std::vector<double> x, y;
};

// `secs` of steps toward `want` from the view at 0, 0.
Run run(const ai::MouseHand& hand, const ai::MouseWant& want, double secs, uint32_t seed = 7) {
  ai::Mouse m(hand, seed);
  m.reset(0, 0);
  Run r;
  const int steps = static_cast<int>(secs / kDt);
  for (int k = 0; k < steps; k++) {
    double x = 0, y = 0;
    m.step(kDt, want, x, y);
    r.x.push_back(x);
    r.y.push_back(y);
  }
  return r;
}

}  // namespace

TEST(a_turn_is_strokes_with_a_bell_of_speed_and_no_jitter) {
  ai::MouseHand hand;
  hand.tremor_cm = 0;
  ai::MouseWant w;
  w.x = 90;
  w.tol_x = w.tol_y = 1.5;
  w.width = 3;
  const Run r = run(hand, w, 1.5);
  CHECK_MSG(std::fabs(r.x.back() - 90) < 3, "the turn did not end near 90 degrees");
  double peak = 0, travel = 0;
  int reversals = 0, moving = 0;
  for (size_t k = 1; k < r.x.size(); k++) {
    const double v = (r.x[k] - r.x[k - 1]) / kDt;
    peak = std::fmax(peak, std::fabs(v));
    travel += std::fabs(r.x[k] - r.x[k - 1]);
    if (k >= 2) {
      const double u = (r.x[k - 1] - r.x[k - 2]) / kDt;
      reversals += u * v < 0 && std::fabs(u) > 60 && std::fabs(v) > 60;
    }
    moving += std::fabs(v) > 5;
  }
  const double mean = travel / (moving * kDt);
  CHECK_MSG(peak > 1.4 * mean, "the stroke's speed was flat, not a bell");
  CHECK_MSG(reversals == 0, "the view turned back and forth at the step's rate");
  CHECK_MSG(peak < 900, "a 90-degree turn went faster than a hand");
}

TEST(a_drag_right_runs_down_and_a_drag_left_up) {
  ai::MouseHand hand;
  hand.tremor_cm = 0;
  hand.dir_sd_deg = 0;  // the arc alone, no stroke's scatter across
  ai::MouseWant w;
  w.tol_y = 30;  // the pitch let be, as on a calm look round
  w.tol_x = 1;
  w.x = 80;
  const Run right = run(hand, w, 1.2);
  w.x = -80;
  const Run left = run(hand, w, 1.2);
  CHECK_MSG(right.y.back() < -0.3, "a long drag right did not run the view down");
  CHECK_MSG(left.y.back() > 0.3, "a long drag left did not run the view up");
  hand.tilt = -hand.tilt;  // a left hand
  w.x = 80;
  CHECK_MSG(run(hand, w, 1.2).y.back() > 0.3, "a left hand's drag right did not run up");
}

TEST(a_turn_past_the_pads_reach_lifts_and_gets_there) {
  ai::MouseHand hand;
  ai::Mouse m(hand, 3);
  m.reset(0, 0);
  ai::MouseWant w;
  w.x = 540;
  w.tol_x = w.tol_y = 2;
  double x = 0, y = 0;
  for (int k = 0; k < 4 * 77; k++) m.step(kDt, w, x, y);
  CHECK_MSG(m.stats.lifts >= 2, "a turn and a half went without lifting the hand");
  CHECK_MSG(std::fabs(x - 540) < 4, "after its lifts the turn did not get there");
}

TEST(with_a_shot_due_the_hand_waits_then_lands_as_it_comes_due) {
  ai::MouseHand hand;
  hand.tremor_cm = 0;
  ai::Mouse m(hand, 5);
  m.reset(0, 0);
  ai::MouseWant w;
  w.x = 12;
  w.tol_x = w.tol_y = 1;
  w.width = 2;
  double x = 0, y = 0;
  double moved_at = -1;
  const double due_at = 0.6;
  for (int k = 0; k < 77; k++) {
    const double t = k * kDt;
    w.due_s = std::fmax(0.0, due_at - t);
    m.step(kDt, w, x, y);
    if (moved_at < 0 && std::fabs(x) > 0.5) moved_at = t;
    if (t + kDt >= due_at && t < due_at) {
      CHECK_MSG(std::fabs(x - 12) < 3, "the timed stroke had not landed as the shot came due");
    }
  }
  CHECK_MSG(moved_at > 0.2, "the hand chased the target long before the shot was due");
  CHECK(m.stats.timed == 1);
}

TEST(the_same_seed_moves_the_same_bits) {
  ai::MouseHand hand;
  ai::MouseWant w;
  w.x = 200;
  w.y = -15;
  const Run a = run(hand, w, 2, 11), b = run(hand, w, 2, 11), c = run(hand, w, 2, 12);
  CHECK(std::memcmp(a.x.data(), b.x.data(), a.x.size() * sizeof(double)) == 0);
  CHECK(std::memcmp(a.y.data(), b.y.data(), a.y.size() * sizeof(double)) == 0);
  CHECK_MSG(a.x != c.x, "another seed moved the same way");
}

TEST(the_finger_clicks_unevenly_and_faster_under_stress) {
  const auto clicks = [](double stress, double& sd) {
    ai::Finger f({}, 9);
    std::vector<double> at;
    bool was = false;
    double t = 0;
    for (int k = 0; k < 77 * 10; k++, t += kDt) {
      const bool down = f.step(kDt, true, false, stress);
      if (down && !was) at.push_back(t);
      was = down;
    }
    double mean = 0;
    for (size_t k = 1; k < at.size(); k++) mean += at[k] - at[k - 1];
    mean /= static_cast<double>(at.size() - 1);
    double var = 0;
    for (size_t k = 1; k < at.size(); k++)
      var += (at[k] - at[k - 1] - mean) * (at[k] - at[k - 1] - mean);
    sd = std::sqrt(var / static_cast<double>(at.size() - 1));
    return static_cast<int>(at.size());
  };
  double sd_calm = 0, sd_hurt = 0;
  const int calm = clicks(0, sd_calm), hurt = clicks(1, sd_hurt);
  CHECK_MSG(calm >= 35 && calm <= 65, "an unhurried finger did not click 4-6 times a second");
  CHECK_MSG(hurt > calm * 1.4, "a finger near death did not click faster");
  CHECK_MSG(sd_calm > 0.02, "the clicks came like a metronome");
  ai::Finger beam({}, 9);
  bool held = true;
  for (int k = 0; k < 77; k++) held = held && beam.step(kDt, true, true, 0);
  CHECK_MSG(held, "a beam was not held down");
  CHECK_MSG(!beam.step(kDt, false, true, 0), "the button stayed down with nothing to fire at");
}
