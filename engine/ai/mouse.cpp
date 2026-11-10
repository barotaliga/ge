// The mouse hand (mouse.hpp): strokes planned at the eye's pace, the pad, the lifts, the arc.
//
// ge:summary The mouse hand: strokes, Fitts timing, the pad's reach and lifts, the arc, tremor
// ge:keywords ai mouse stroke minimum-jerk fitts lift arc tremor
// ge:skill bots
// ge:test tests/ai_mouse_test.cpp
#include "ai/mouse.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::ai {

namespace {

constexpr double kSlip_s = 0.05;  // a set-down's slide, s

// A minimum-jerk stroke's share of its length done at `u` (0..1) of its time.
double done(double u) {
  if (u <= 0) return 0;
  if (u >= 1) return 1;
  return u * u * u * (10 + u * (-15 + 6 * u));
}

// About a standard normal off four uniforms (no transcendental): their sum less 2, scaled.
double normal(core::Rng& r) {
  const double s = core::next(r) + core::next(r) + core::next(r) + core::next(r);
  return (s - 2) * 1.7320508075688772;
}

}  // namespace

Mouse::Mouse(const MouseHand& hand, uint32_t seed) : hand_(hand), rng_(seed) {
  phase_x_ = core::next(rng_) * 2 * core::kPi;
  phase_y_ = core::next(rng_) * 2 * core::kPi;
}

void Mouse::reset(double x, double y) {
  x_ = x;
  y_ = y;
  hx_ = hy_ = 0;
  count_ = 0;
  since_ = 1e9;
  lift_left_ = 0;
  lift_after_ = 0;
  rate_x_ = rate_y_ = 0;
  seen_ = false;
  timed_ = false;
  last_due_ = 0;
}

double Mouse::fitts(double d, double width) const {
  const double w = width > 0.25 ? width : 0.25;
  const double t = hand_.fitts_a + hand_.fitts_b * core::libm::log2(d / w + 1);
  return t > hand_.min_s ? t : hand_.min_s;
}

void Mouse::plan(const MouseWant& want) {
  const double per_deg = hand_.cm_per_360 / 360;
  // Where the strokes under way will leave the view.
  double px = x_, py = y_;
  for (int k = 0; k < count_; k++) {
    const Stroke& s = strokes_[static_cast<size_t>(k)];
    const double left = 1 - done(s.age / s.dur);
    px += s.ax * left / per_deg;
    py += s.ay * left / per_deg;
  }
  double ex = want.x - px, ey = want.y - py;
  double d = std::sqrt(ex * ex + ey * ey);
  double dur = fitts(d, want.width) * want.pace;
  if (want.lead) {  // where it will be when the stroke lands
    ex += rate_x_ * dur;
    ey += rate_y_ * dur;
    d = std::sqrt(ex * ex + ey * ey);
    dur = fitts(d, want.width) * want.pace;
  }
  const bool timed = want.due_s > 0 && d < want.loose;
  if (timed) {
    // A shot due: the one stroke onto him its Fitts time before it, none before that.
    if (timed_ || want.due_s > dur) return;
    timed_ = true;
    stats.timed++;
  } else if (since_ < hand_.look_s) {
    return;
  }
  // Only an axis past its tolerance is put right.
  if (ex < want.tol_x && ex > -want.tol_x) ex = 0;
  if (ey < want.tol_y && ey > -want.tol_y) ey = 0;
  if (ex == 0 && ey == 0) return;
  if (count_ == kStrokes) return;  // the hand is busy enough
  // A hand does not turn back across while a stroke is under way: one the other way waits until
  // it is nearly done (`turn_back` of its time), so the view never reverses at speed.
  for (int k = 0; k < count_; k++) {
    const Stroke& s = strokes_[static_cast<size_t>(k)];
    if ((s.ax * ex < 0 || s.ay * ey < 0) && s.age < want.turn_back * s.dur &&
        s.ax * ex + s.ay * ey < 0)
      return;
  }
  d = std::sqrt(ex * ex + ey * ey);
  dur = fitts(d, want.width) * want.pace * (0.9 + 0.2 * core::next(rng_));
  // Off by a share of its length: along it (mostly short), and across it.
  const double along = hand_.gain + hand_.gain_sd * normal(rng_);
  const double across = hand_.dir_sd_deg * (core::kPi / 180) * normal(rng_);
  double sx = (ex * along - ey * across) * per_deg, sy = (ey * along + ex * across) * per_deg;
  // No further than the pad's edge: a stroke that would run off it ends there, and the hand
  // lifts once it has (the rest of the turn after the lift). Shorter, it is quicker too.
  double hand_x = hx_;
  for (int k = 0; k < count_; k++) {
    const Stroke& s = strokes_[static_cast<size_t>(k)];
    hand_x += s.ax * (1 - done(s.age / s.dur));
  }
  const int side = sx > 0 ? 1 : -1;
  const double room = side * hand_.reach_cm * 0.97 - hand_x;
  lift_after_ = 0;
  if (sx * side > room * side) {
    if (room * side < 0.3) {  // at the edge already: lift now
      lift_after_ = side;
      return;
    }
    const double cut = room / sx;
    sx *= cut;
    sy *= cut;
    dur = fitts(d * cut, want.width) * want.pace * (0.9 + 0.2 * core::next(rng_));
    lift_after_ = side;
  }
  strokes_[static_cast<size_t>(count_++)] = {0, dur, sx, sy};
  since_ = 0;
  stats.strokes++;
}

void Mouse::step(double dt, const MouseWant& want, double& x, double& y) {
  const double per_deg = hand_.cm_per_360 / 360;
  t_ += dt;
  since_ += dt;
  // The target's own motion as the eye has seen it, over about 0.15 s; a jump (another target, a
  // turn of the bot's mind) is no motion.
  if (seen_ && dt > 0) {
    const double dx = want.x - want_x_, dy = want.y - want_y_;
    if (dx > 8 || dx < -8 || dy > 8 || dy < -8) {
      rate_x_ = rate_y_ = 0;
    } else {
      const double k = dt / 0.15 < 1 ? dt / 0.15 : 1;
      rate_x_ += (dx / dt - rate_x_) * k;
      rate_y_ += (dy / dt - rate_y_) * k;
    }
  }
  want_x_ = want.x;
  want_y_ = want.y;
  seen_ = true;
  // A new shot's wait (the due time grew): its timed stroke still to come.
  if (want.due_s <= 0 || want.due_s > last_due_ + 1e-9) timed_ = false;
  last_due_ = want.due_s;

  if (lift_left_ > 0) {
    lift_left_ -= dt;
    stats.lifted_s += dt;
    if (lift_left_ <= 0) {
      // Set down across the pad, a little off where it was meant to go.
      hx_ = -lift_side_ * hand_.back * hand_.reach_cm;
      hy_ *= 0.5;
      // The slip as the mouse meets the pad: a short slide, not a jump.
      const double sx = hand_.setdown_cm * normal(rng_), sy = hand_.setdown_cm * normal(rng_);
      if (count_ < kStrokes) strokes_[static_cast<size_t>(count_++)] = {0, kSlip_s, sx, sy};
      since_ = hand_.look_s * 0.5;  // the eye takes in where it landed first
    }
  } else if (lift_after_ != 0 && count_ == 0) {
    // The stroke to the pad's edge is done: up and back across.
    lift_side_ = lift_after_;
    lift_after_ = 0;
    lift_left_ = hand_.lift_s;
    stats.lifts++;
  } else {
    plan(want);
    double dx = 0, dy = 0;
    int kept = 0;
    for (int k = 0; k < count_; k++) {
      Stroke s = strokes_[static_cast<size_t>(k)];
      const double was = done(s.age / s.dur);
      s.age += dt;
      const double now = done(s.age / s.dur);
      dx += s.ax * (now - was);
      dy += s.ay * (now - was);
      if (s.age < s.dur) strokes_[static_cast<size_t>(kept++)] = s;
    }
    count_ = kept;
    const double w = 2 * core::kPi * hand_.tremor_hz;
    dx += hand_.tremor_cm *
          (core::libm::sin(w * t_ + phase_x_) - core::libm::sin(w * (t_ - dt) + phase_x_));
    dy += hand_.tremor_cm * (core::libm::sin(w * t_ * 1.13 + phase_y_) -
                             core::libm::sin(w * (t_ - dt) * 1.13 + phase_y_));
    // The pad's reach: across, the hand stops at the edge and lifts; up and down, it stops.
    if (hx_ + dx > hand_.reach_cm || hx_ + dx < -hand_.reach_cm) {
      const int side = hx_ + dx > 0 ? 1 : -1;
      dx = side * hand_.reach_cm - hx_;
      lift_side_ = side;
      lift_left_ = hand_.lift_s;
      count_ = 0;
      stats.lifts++;
    }
    if (hy_ + dy > hand_.reach_y_cm) dy = hand_.reach_y_cm - hy_;
    if (hy_ + dy < -hand_.reach_y_cm) dy = -hand_.reach_y_cm - hy_;
    // The arc: what the hand does across bends the view down to the right, and down at both
    // ends of a long drag.
    const double nx = hx_ + dx;
    const double sy = dy - hand_.tilt * dx - (nx * nx - hx_ * hx_) / (2 * hand_.bow_cm);
    hx_ = nx;
    hy_ += dy;
    x_ += dx / per_deg;
    y_ += sy / per_deg;
  }
  if (y_ > 88) y_ = 88;
  if (y_ < -88) y_ = -88;
  x = x_;
  y = y_;
}

bool Finger::step(double dt, bool fire, bool hold, double stress, double due_s) {
  if (!fire) {
    on_ = false;
    down_ = false;
    left_ = 0;
    return false;
  }
  if (hold) {
    on_ = true;
    if (!down_) clicks++;
    down_ = true;
    return true;
  }
  if (!on_) {  // the first click a moment after the mind says fire
    on_ = true;
    down_ = false;
    left_ = spec_.first_s * core::next(rng_);
  }
  left_ -= dt;
  while (left_ <= 0) {
    const double s = stress < 0 ? 0 : stress > 1 ? 1 : stress;
    if (!down_) {
      down_ = true;
      clicks++;
      press_ = core::range(rng_, spec_.press_lo, spec_.press_hi);
      left_ += press_;
    } else {
      down_ = false;
      const double mean = spec_.gap_calm_s + (spec_.gap_stress_s - spec_.gap_calm_s) * s;
      double gap = mean * (1 + spec_.gap_sd * normal(rng_));
      // Kept in time with the weapon: the next click just after it is ready (`due_s` from now,
      // the press already past).
      if (due_s > 0 && core::next(rng_) < spec_.sync)
        gap = press_ + due_s + core::range(rng_, spec_.sync_lo, spec_.sync_hi);
      if (gap < press_ + 0.03) gap = press_ + 0.03;
      left_ += gap - press_;
    }
  }
  return down_;
}

}  // namespace ge::ai
