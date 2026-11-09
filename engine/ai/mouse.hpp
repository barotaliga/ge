// THE MOUSE HAND: a view turned the way a person turns it with a mouse on a pad. A bot says where
// it wants the view each step, how close is close enough, and when it must be there (a shot);
// the hand gets there in strokes, with a hand's limits:
//
// 1. STROKES. Each a minimum-jerk stroke (its speed a bell, peak 1.9 times its mean) lasting the
//    Fitts time `fitts_a + fitts_b * log2(distance / width + 1)`, and landing off by a share of
//    its length: a little short on the whole (`gain`), scattered by `gain_sd` along its way and
//    `dir_sd_deg` across it (a longer stroke lands wider). The eye sees where the view goes and
//    plans the next, smaller stroke from where the strokes under way will leave it, so strokes
//    overlap and the view moves smoothly: it never steps at the frame's rate.
// 2. THE EYE'S PACE. A stroke is planned at most every `look_s` (the eye-and-hand loop), and only
//    for an error past the step's tolerance (`MouseWant::tol_x`, `tol_y`): a calm view leaves its
//    band alone; a tracked target is followed closely; pitch is put right only once it is off.
// 3. THE PAD. The hand moves in centimetres (`cm_per_360`) and reaches `reach_cm` either way from
//    where it rests. A stroke that would carry it past is planned to the edge, and once it is done
//    the hand LIFTS: the
//    view stops for `lift_s` while it goes back across (to `back` of its reach on the other
//    side), and SETS DOWN off by about `setdown_cm` in each direction (a degree or two), which the
//    eye then corrects.
// 4. THE ARC. A right hand turns about the wrist and the elbow, which lie behind it and off to its
//    right, so a drag right runs down and a drag left up (`tilt`: centimetres down a centimetre
//    right), and both ends of a long drag bend down (`bow_cm`: the arc's radius). The planner aims
//    in straight lines and does not know it: a long sweep comes out a slanted curve, put right by
//    the eye's next stroke only once the pitch is past tolerance.
// 5. TREMOR. A hand's faint shake, `tremor_cm` at about `tremor_hz` (a hand's 8 to 12 Hz).
// 6. A SHOT'S TIME (`MouseWant::due_s`). With a shot due in `due_s`, no stroke is planned for the
//    target until its Fitts time before the shot (the view meanwhile left where the last stroke
//    put it, drifting with the hand); then one stroke to where the target will be when it lands.
//    A target further off than `MouseWant::loose` is kept in view by a stroke whenever.
//
// THE FINGER on the mouse's button (`Finger`): a person does not hold fire down, he clicks it
// again and again. Each click is held `press_lo..press_hi` s; the next comes a gap later, drawn
// about a mean that shortens from `gap_calm_s` to `gap_stress_s` as he is pressed harder (the
// caller's `stress`, 0..1: health running out), scattered by `gap_sd` so no two gaps match. A
// shot leaves only on a press once the weapon is ready again, so clicking costs a little rate of
// fire, as it does a person; a practised hand times `sync` of its clicks to just after the
// weapon comes ready. A weapon fired as a stream (a beam) is held instead (`hold`).
//
// Deterministic: strokes' scatter, set-downs and tremor phases come off the hand's own
// `core::Rng`; the arithmetic is IEEE doubles with the vendored libm's sin and log2. Fixed-size
// state, no allocation: a copy is cheap.
//
// ge:summary The mouse hand: a view turned by minimum-jerk strokes, Fitts timing, lifts, the arc
// ge:keywords ai bot mouse hand view aim submovement minimum-jerk fitts lift mousepad arc tilt
// ge:keywords tremor stroke human-like sensitivity cm-per-360 shot-timing flick turn-back reversal
// ge:skill bots
// ge:test tests/ai_mouse_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "core/prng.hpp"

namespace ge::ai {

// A player's hand, pad and setting. The defaults are an average right-handed player.
struct MouseHand {
  double cm_per_360 = 30;   // the mouse's travel for a whole turn, cm
  double reach_cm = 11;     // the hand's reach either way across the pad before it lifts, cm
  double reach_y_cm = 7;    // and up and down the pad, cm
  double back = 0.5;        // a lift sets the hand down this share of its reach the other side
  double lift_s = 0.17;     // a lift: the view still while the hand goes back across, s
  double setdown_cm = 0.1;  // a set-down's slip each way (a standard deviation), cm
  double fitts_a = 0.08;    // a stroke's Fitts time: a + b log2(D / W + 1), s (180: 0.32 s)
  double fitts_b = 0.04;    // s a bit
  double min_s = 0.06;      // the quickest stroke, s
  double gain = 0.97;       // a stroke's mean length over the distance meant (under 1: short)
  double gain_sd = 0.056;   // its scatter along the way, a share of it (a 180 flip: +-15)
  double dir_sd_deg = 3.2;  // its scatter across, degrees off the stroke's direction
  double look_s = 0.12;     // the eye's loop: a new stroke at most this often, s
  double tilt = 0.01;       // a drag right runs down: cm down a cm right (people: 0.008 deg/deg)
  double bow_cm = 1000;     // the arc's radius as the sensor feels it (the mouse turns too), cm
  double tremor_cm = 0.004;
  double tremor_hz = 10;
};

// What the view is asked for this step. x is to the right, y up, both in degrees (the caller
// unwraps x near the view).
struct MouseWant {
  double x = 0, y = 0;
  double tol_x = 1, tol_y = 1;  // an error under these is left alone, degrees
  double width = 2;             // the target's width for the Fitts time, degrees
  double due_s = 0;             // > 0: a shot in this long; the target aimed for at that time
  double loose = 25;            // with a shot due, an error past this is followed anyway, degrees
  bool lead = true;             // aim for where the target will be (its rate seen so far)
  // The stroke's time over its Fitts time: 1 a turn meant (onto an enemy, an about-face); more
  // for the view's moves while walking and jumping about, which a person makes at ease.
  double pace = 1;
  // A stroke the other way waits until the one under way is this share of its time done (a
  // minimum-jerk stroke still runs at 1.3 times its mean speed at 0.7, a quarter at 0.9).
  double turn_back = 0.7;
};

struct MouseStats {
  int64_t strokes = 0, lifts = 0, timed = 0;
  double lifted_s = 0;
};

class Mouse {
 public:
  Mouse() = default;
  Mouse(const MouseHand& hand, uint32_t seed);
  // The hand at rest in the middle of its pad and the view at `x`, `y`; nothing under way.
  void reset(double x, double y);
  // One step of `dt` seconds toward `want`; the view after it in `x`, `y`.
  void step(double dt, const MouseWant& want, double& x, double& y);
  double x() const { return x_; }
  double y() const { return y_; }
  bool lifted() const { return lift_left_ > 0; }
  const MouseHand& hand() const { return hand_; }
  // A stroke's Fitts time this many degrees onto a target this wide, s (at a turn meant's pace).
  double fitts(double distance, double width) const;
  MouseStats stats;

 private:
  struct Stroke {
    double age = 0, dur = 0;  // s
    double ax = 0, ay = 0;    // its length on the pad, cm
  };
  static constexpr int kStrokes = 4;
  void plan(const MouseWant& want);
  MouseHand hand_;
  core::Rng rng_{0};
  std::array<Stroke, kStrokes> strokes_{};
  int count_ = 0;
  double x_ = 0, y_ = 0;    // the view, degrees
  double hx_ = 0, hy_ = 0;  // the hand on the pad from where it rests, cm
  double since_ = 1e9;      // since the last stroke was planned, s
  double t_ = 0;            // the hand's clock (the tremor's phase), s
  double phase_x_ = 0, phase_y_ = 0;
  double lift_left_ = 0;  // s of a lift still to go
  int lift_side_ = 0;     // the side the hand was carried to before the lift
  int lift_after_ = 0;    // a stroke runs to the pad's edge on this side: lift once it is done
  double want_x_ = 0, want_y_ = 0, rate_x_ = 0, rate_y_ = 0;
  bool seen_ = false;
  bool timed_ = false;   // this shot's timed stroke planned
  double last_due_ = 0;  // the last step's `due_s`
};

// The finger's clicking (above, THE FINGER).
struct FingerSpec {
  double gap_calm_s = 0.2;     // the mean time from one click to the next, unhurried, s
  double gap_stress_s = 0.11;  // and at the end of his tether, s
  double gap_sd = 0.22;        // a gap's scatter, a share of its mean
  double press_lo = 0.045, press_hi = 0.09;  // a click held this long, s
  double first_s = 0.03;                     // the first click after wanting to fire, at most, s
  // The share of clicks a player times to the weapon's coming ready (a practised hand keeps the
  // weapon's rhythm): such a click lands `sync_lo..sync_hi` s after it is ready.
  double sync = 0.5;
  double sync_lo = 0.01, sync_hi = 0.06;
};

class Finger {
 public:
  Finger() = default;
  Finger(const FingerSpec& spec, uint32_t seed) : spec_(spec), rng_(seed) {}
  // One step of `dt`: whether the button is down. `fire`: it means to shoot; `hold`: held down
  // (a beam) rather than clicked; `stress` 0..1; `due_s` the weapon ready again in this long
  // (0: ready or not known).
  bool step(double dt, bool fire, bool hold, double stress, double due_s = 0);
  int64_t clicks = 0;

 private:
  FingerSpec spec_;
  core::Rng rng_{0};
  bool down_ = false;
  double left_ = 0;   // s until the button changes (down: let go; up: the next click)
  bool on_ = false;   // meant to fire last step
  double press_ = 0;  // the click under way's hold, s
};

}  // namespace ge::ai
