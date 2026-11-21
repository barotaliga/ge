// THE HAND, READ OFF A RECORDING (se-people): the view as a mouse turns it, every kHandEvery s for
// each player alive: the sweeps, their tilt, peak and pitch, the lifts and the reversals, and the
// metrics written off them. People and bots by one rule.
//
// ge:summary se-people's hand: sweeps, lifts and reversals of the view read at 40 Hz,
// ge:summary and their metrics
// ge:keywords hand sweep lift reversal view-turn mouse se-people humanness
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "lab/lab.hpp"

namespace shooters_edge::labs {

constexpr double kHandEvery = 0.025;  // the hand read at 40 Hz
constexpr double kSweepRate = 30;     // a sweep turns faster than this across, degrees/s
constexpr double kSweepMin = 30;      // a sweep this long or more is measured, degrees
constexpr double kLiftBefore = 60;    // a lift follows a sweep this long or more, degrees
constexpr double kLiftStillLo = 0.05, kLiftStillHi = 0.35;  // and a still stretch this long, s
constexpr double kReverseRate = 60;  // a reversal: both turns past this, degrees/s
constexpr double kTeleport = 3000;   // a turn past this is the server's (a teleport), degrees/s

// THE HAND (the view as a mouse turns it), every kHandEvery s for each player alive: rightward and
// upward turn rates off the recorded angles. A SWEEP is a run of ticks turning one way across
// past kSweepRate; one of kSweepMin degrees or more is measured: its pitch against its yaw (the
// tilt a hand's arc gives: a right hand's drag right runs down), its peak rate over its mean (a
// hand's bell 1.9, a constant turn 1.0), and whether its pitch held dead level. A LIFT is a sweep
// of kLiftBefore or more, a still stretch of kLiftStill_lo..hi s, then a sweep the same way of
// kSweepMin or more (the hand back across the pad). A REVERSAL is a tick whose turn runs the
// other way from the last one's, both past kReverseRate (jitter).
struct HandRead {
  int64_t ticks = 0, sweeps = 0, level = 0, lifts = 0, big = 0, reversals = 0, flat = 0,
          turning = 0;
  double xy = 0, xx = 0, ratio = 0, peak = 0, secs = 0, size = 0;
  double up_right = 0, right_right = 0, up_left = 0, right_left = 0;
};

void add(HandRead& a, const HandRead& h) {
  a.ticks += h.ticks;
  a.sweeps += h.sweeps;
  a.level += h.level;
  a.lifts += h.lifts;
  a.big += h.big;
  a.reversals += h.reversals;
  a.flat += h.flat;
  a.turning += h.turning;
  a.xy += h.xy;
  a.xx += h.xx;
  a.ratio += h.ratio;
  a.peak += h.peak;
  a.secs += h.secs;
  a.size += h.size;
  a.up_right += h.up_right;
  a.right_right += h.right_right;
  a.up_left += h.up_left;
  a.right_left += h.right_left;
}
struct HandState {
  bool live = false;
  float yaw = 0, pitch = 0;
  double rate = 0;  // the last tick's rightward rate
  int sign = 0;
  double sx = 0, sy = 0, dur = 0, peak = 0;  // the sweep under way
  double last_sweep = 0;                     // the last sweep measured's size, signed
  double still = 0;                          // still since it, s
};

// The sweep under way in `h` ends: measured into `r` when it is long enough.
inline void end_sweep(HandState& h, HandRead& r) {
  const double size = std::fabs(h.sx);
  if (h.sign != 0 && size >= kSweepMin) {
    r.sweeps++;
    r.size += size;
    r.xy += h.sx * h.sy;
    r.xx += h.sx * h.sx;
    r.ratio += std::fabs(h.sy) / size;
    r.peak += h.dur > 0 ? h.peak / (size / h.dur) : 1;
    r.level += std::fabs(h.sy) < 0.25;
    r.big += size >= 90;
    if (h.sx > 0) {
      r.up_right += h.sy;
      r.right_right += size;
    } else {
      r.up_left += h.sy;
      r.right_left += size;
    }
    const bool same = (h.last_sweep > 0) == (h.sx > 0);
    if (same && std::fabs(h.last_sweep) >= kLiftBefore && h.still >= kLiftStillLo &&
        h.still <= kLiftStillHi)
      r.lifts++;
    h.last_sweep = h.sx;
    h.still = 0;
  } else if (h.sign != 0) {
    h.still += h.dur;  // a small turn between two sweeps counts as the hand at rest
  }
  h.sign = 0;
  h.sx = h.sy = h.dur = h.peak = 0;
}

// One 40 Hz tick of a live player's view (`yaw`, `pitch` as recorded: pitch down positive).
inline void hand_tick(HandState& h, HandRead& r, float yaw, float pitch, double dt) {
  if (!h.live) {
    h = HandState{};
    h.live = true;
    h.yaw = yaw;
    h.pitch = pitch;
    return;
  }
  float dyaw = yaw - h.yaw;
  while (dyaw > 180) dyaw -= 360;
  while (dyaw < -180) dyaw += 360;
  const double rx = -dyaw / dt, ry = -(pitch - h.pitch) / dt;  // to the right, up
  h.yaw = yaw;
  h.pitch = pitch;
  r.ticks++;
  r.secs += dt;
  if (std::fabs(rx) > kTeleport) {
    h.sign = 0;
    h.sx = h.sy = h.dur = h.peak = 0;
    h.last_sweep = 0;
    h.rate = 0;
    return;
  }
  if (std::fabs(rx) > kReverseRate && std::fabs(h.rate) > kReverseRate && rx * h.rate < 0)
    r.reversals++;
  if (std::fabs(rx) > kReverseRate) {
    r.turning++;
    r.flat += ry == 0;
  }
  h.rate = rx;
  const int sg = rx > kSweepRate ? 1 : rx < -kSweepRate ? -1 : 0;
  if (sg != h.sign) end_sweep(h, r);
  if (sg != 0) {
    h.sign = sg;
    h.sx += rx * dt;
    h.sy += ry * dt;
    h.dur += dt;
    h.peak = std::fmax(h.peak, std::fabs(rx));
  } else {
    h.still += dt;
  }
}

// The hand's measures under `key` (HandRead).
inline void hand_metrics(ge::lab::Lab& lab, const std::string& key, const HandRead& hand) {
  const auto ratio = [](double a, double b) { return b > 0 ? a / b : 0; };
  const double sw = static_cast<double>(hand.sweeps), mins = hand.secs / 60;
  lab.metric(key + "sweeps-per-min", ratio(sw, mins), "per-min");
  lab.metric(key + "big-per-min", ratio(static_cast<double>(hand.big), mins), "per-min");
  lab.metric(key + "sweep-size", ratio(hand.size, sw), "deg");
  lab.metric(key + "tilt", ratio(hand.xy, hand.xx), "deg-up-per-deg-right");
  lab.metric(key + "up-per-right", ratio(hand.up_right, hand.right_right), "deg/deg");
  lab.metric(key + "up-per-left", ratio(hand.up_left, hand.right_left), "deg/deg");
  lab.metric(key + "sweep-pitch", ratio(hand.ratio, sw), "deg/deg");
  lab.metric(key + "sweep-peak", ratio(hand.peak, sw), "peak/mean");
  lab.metric(key + "sweep-level", ratio(static_cast<double>(hand.level), sw), "share");
  lab.metric(key + "lifts-per-min", ratio(static_cast<double>(hand.lifts), mins), "per-min");
  lab.metric(key + "lifts-per-big",
             ratio(static_cast<double>(hand.lifts), static_cast<double>(hand.big)), "ratio");
  lab.metric(key + "reversals-per-s", ratio(static_cast<double>(hand.reversals), hand.secs),
             "per-s");
  lab.metric(key + "flat-pitch",
             ratio(static_cast<double>(hand.flat), static_cast<double>(hand.turning)), "share");
}

}  // namespace shooters_edge::labs
