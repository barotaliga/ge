// Feedback's records and the effects' forces: the envelope, the waves, the conditions.
//
// ge:summary The feedback records and each force-feedback effect's force over time
// ge:keywords input feedback effect force envelope wave spring damper friction inertia
// ge:skill input
// ge:test tests/input_feedback_test.cpp
#include "input/feedback.hpp"

#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::input {

namespace {

constexpr double kPi = 3.14159265358979323846;

double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

double envelope_at(const Effect& e, double t) {
  const Envelope& v = e.envelope;
  double k = 1;
  if (v.attack_s > 0 && t < v.attack_s)
    k = v.attack_level + (1 - v.attack_level) * (t / v.attack_s);
  if (e.duration_s > 0 && v.fade_s > 0) {
    const double fade_from = e.duration_s - v.fade_s;
    if (t > fade_from) {
      const double u = clampd((t - fade_from) / v.fade_s, 0, 1);
      k *= 1 + (v.fade_level - 1) * u;
    }
  }
  return k;
}

// The wave's value at `u`, a share of its period (0..1).
double wave(EffectKind k, double u) {
  switch (k) {
    case EffectKind::sine:
      return core::libm::sin(2 * kPi * u);
    case EffectKind::square:
      return u < 0.5 ? 1 : -1;
    case EffectKind::triangle:
      return u < 0.25 ? 4 * u : (u < 0.75 ? 2 - 4 * u : 4 * u - 4);
    case EffectKind::saw_up:
      return 2 * u - 1;
    case EffectKind::saw_down:
      return 1 - 2 * u;
    default:
      return 0;
  }
}

}  // namespace

DeviceFeedback* Feedback::get(DeviceId id) {
  if (id == 0) return nullptr;
  for (DeviceFeedback& x : d) {
    if (x.id == id) return &x;
  }
  for (DeviceFeedback& x : d) {
    if (x.id == 0) {
      x = {};
      x.id = id;
      return &x;
    }
  }
  return nullptr;
}

const DeviceFeedback* Feedback::find(DeviceId id) const {
  for (const DeviceFeedback& x : d) {
    if (id != 0 && x.id == id) return &x;
  }
  return nullptr;
}

void Feedback::forget(DeviceId id) {
  for (DeviceFeedback& x : d) {
    if (x.id == id) x = {};
  }
}

void rumble(Feedback& f, DeviceId id, float low, float high, double duration_s, double now_s) {
  DeviceFeedback* d = f.get(id);
  if (!d) return;
  d->low = low;
  d->high = high;
  d->rumble_until_s = duration_s > 0 ? now_s + duration_s : 0;
}

void rumble_triggers(Feedback& f, DeviceId id, float left, float right, double duration_s,
                     double now_s) {
  DeviceFeedback* d = f.get(id);
  if (!d) return;
  d->left = left;
  d->right = right;
  d->triggers_until_s = duration_s > 0 ? now_s + duration_s : 0;
}

void set_light(Feedback& f, DeviceId id, uint8_t r, uint8_t g, uint8_t b) {
  DeviceFeedback* d = f.get(id);
  if (!d) return;
  d->light_set = true;
  d->r = r;
  d->g = g;
  d->b = b;
}

int play_effect(Feedback& f, DeviceId id, const Effect& e, double now_s) {
  DeviceFeedback* d = f.get(id);
  if (!d) return -1;
  for (int i = 0; i < kMaxEffects; i++) {
    Playing& p = d->effects[static_cast<size_t>(i)];
    if (!p.on || !playing(p, now_s)) {
      p = {true, e, now_s};
      return i;
    }
  }
  return -1;
}

void stop_effect(Feedback& f, DeviceId id, int handle) {
  DeviceFeedback* d = f.get(id);
  if (d && handle >= 0 && handle < kMaxEffects) d->effects[static_cast<size_t>(handle)].on = false;
}

void update_effect(Feedback& f, DeviceId id, int handle, const Effect& e) {
  DeviceFeedback* d = f.get(id);
  if (d && handle >= 0 && handle < kMaxEffects) d->effects[static_cast<size_t>(handle)].effect = e;
}

void rumble_at(const DeviceFeedback& d, double now_s, float& low, float& high) {
  const bool live = d.rumble_until_s == 0 || now_s < d.rumble_until_s;
  low = live ? d.low : 0;
  high = live ? d.high : 0;
}

bool playing(const Playing& p, double now_s) {
  if (!p.on) return false;
  const Effect& e = p.effect;
  return e.duration_s <= 0 || now_s < p.started_s + e.delay_s + e.duration_s;
}

double effect_force(const Effect& e, double t, double pos, double vel, double acc) {
  if (t < e.delay_s) return 0;
  t -= e.delay_s;
  if (e.duration_s > 0 && t >= e.duration_s) return 0;
  const double env = envelope_at(e, t);
  switch (e.kind) {
    case EffectKind::constant:
      return e.magnitude * env;
    case EffectKind::ramp: {
      const double u = e.duration_s > 0 ? t / e.duration_s : 0;
      return (e.start + (e.end - e.start) * u) * env;
    }
    case EffectKind::sine:
    case EffectKind::square:
    case EffectKind::triangle:
    case EffectKind::saw_up:
    case EffectKind::saw_down: {
      const double p = e.period_s > 0 ? e.period_s : 1;
      double u = t / p + e.phase;
      u -= std::floor(u);
      return e.offset + e.magnitude * env * wave(e.kind, u);
    }
    case EffectKind::spring: {
      const double off = pos - e.center;
      const double beyond =
          std::fabs(off) <= e.deadband ? 0 : off - (off > 0 ? 1 : -1) * e.deadband;
      return clampd(-e.coeff * beyond, -e.saturation, e.saturation);
    }
    case EffectKind::damper:
      return clampd(-e.coeff * vel, -e.saturation, e.saturation);
    case EffectKind::friction:
      // Against the motion, smoothed across a standstill (full by 0.05 units a second).
      return -e.coeff * clampd(vel / 0.05, -1, 1) * (e.saturation < 1 ? e.saturation : 1);
    case EffectKind::inertia:
      return clampd(-e.coeff * acc, -e.saturation, e.saturation);
  }
  return 0;
}

double force_at(const DeviceFeedback& d, double now_s, double pos, double vel, double acc) {
  double f = 0;
  for (const Playing& p : d.effects) {
    if (playing(p, now_s)) f += effect_force(p.effect, now_s - p.started_s, pos, vel, acc);
  }
  f = f * d.gain - d.autocenter * pos;
  return clampd(f, -1, 1);
}

}  // namespace ge::input
