// The hands' filter: the delay ring, the wander, the damped slewed spring (hands.hpp).
//
// ge:summary Hands::step: delay, wander, spring, slew and reach per axis; record_intent
// ge:keywords ai hands delay ring wander spring slew reach intent
// ge:skill bots
// ge:test tests/ai_hands_test.cpp
#include "ai/hands.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::ai {

namespace {

namespace num = ge::core::num;

// Within this of an end or the rest, the wander fades to nothing.
constexpr double kWanderFade = 0.1;

}  // namespace

Hands::Hands(std::span<const AxisHands> axes, double hz, uint32_t seed)
    : dt_(hz > 0 ? 1 / hz : 0), rng_(core::make_rng(seed)) {
  count_ = axes.size() < kMostHandAxes ? axes.size() : kMostHandAxes;
  for (size_t k = 0; k < count_; k++) {
    Axis& a = axes_[k];
    a.spec = axes[k];
    const double steps = num::max(0.0, num::round(axes[k].delay_s * hz));
    a.len = 1 + static_cast<size_t>(num::min(steps, static_cast<double>(kMostDelaySteps)));
  }
}

void Hands::reset(std::span<const double> at) {
  for (size_t k = 0; k < count_; k++) {
    Axis& a = axes_[k];
    const double v = k < at.size() ? at[k] : 0;
    std::fill(a.ring.begin(), a.ring.end(), v);
    a.head = 0;
    a.wander = a.wander_to = 0;
    a.pos = v;
    a.vel = 0;
  }
}

void Hands::step(std::span<const double> wanted, std::span<double> out) {
  for (size_t k = 0; k < count_ && k < wanted.size() && k < out.size(); k++) {
    Axis& a = axes_[k];
    const AxisHands& s = a.spec;
    if (s.pass) {
      out[k] = wanted[k];
      continue;
    }
    // 1. The delay: the ring holds the last delay's steps; the oldest is what the hand acts on.
    a.ring[a.head] = wanted[k];
    a.head = (a.head + 1) % a.len;
    double target = a.ring[a.head];
    // 2. The wander: a new aim at about `wander_hz`, eased toward; faded out near the ends and
    // the rest.
    if (s.wander > 0) {
      if (core::next(rng_) < s.wander_hz * dt_)
        a.wander_to = core::range(rng_, -s.wander, s.wander);
      a.wander += (a.wander_to - a.wander) * num::min(1.0, 2 * s.wander_hz * dt_);
      const double near = num::min(num::min(std::fabs(target - s.lo), std::fabs(s.hi - target)),
                                   std::fabs(target - s.rest));
      target += a.wander * core::clamp(near / kWanderFade, 0, 1);
    }
    target = core::clamp(target, s.lo, s.hi);
    // 3. The hand: a damped spring toward the target, or the target itself, slewed.
    const double most = s.slew > 0 ? s.slew : 0;
    if (s.settle_s > 0) {
      const double zeta = num::max(s.damping, 0.1);
      const double w = 4 / (zeta * s.settle_s);
      a.vel += (w * w * (target - a.pos) - 2 * zeta * w * a.vel) * dt_;
      if (most > 0) a.vel = core::clamp(a.vel, -most, most);
      a.pos += a.vel * dt_;
    } else {
      double move = target - a.pos;
      if (most > 0) move = core::clamp(move, -most * dt_, most * dt_);
      a.pos += move;
    }
    // 4. The reach.
    if (a.pos > s.hi || a.pos < s.lo) {
      a.pos = core::clamp(a.pos, s.lo, s.hi);
      a.vel = 0;
    }
    out[k] = a.pos;
  }
}

void record_intent(Why* why, std::span<const char* const> names, std::span<const double> wanted) {
  if (!why) return;
  why->decide("hands");
  for (size_t k = 0; k < names.size() && k < wanted.size(); k++) why->fact(names[k], wanted[k]);
  why->choose("press", "intent");
}

}  // namespace ge::ai
