// Feedback (engine/input/feedback.hpp): rumble that runs out, each effect's force, the envelope,
// a delay and a duration, the effect slots, and the device's sum clamped with its gain and
// autocentre.
//
// ge:summary Tests: rumble timing, every force-feedback effect, envelope, slots, force_at
// ge:keywords test input feedback rumble effect spring damper friction inertia envelope force
// ge:skill input testing
#include <cmath>

#include "check.hpp"
#include "input/feedback.hpp"

namespace in = ge::input;

namespace {
bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }
in::Effect make(in::EffectKind k) {
  in::Effect e;
  e.kind = k;
  return e;
}
}  // namespace

TEST(rumble_runs_for_its_duration_and_then_stops) {
  in::Feedback f;
  in::rumble(f, 3, 0.5f, 1.0f, 0.2, 10.0);
  float lo = 0, hi = 0;
  in::rumble_at(*f.find(3), 10.1, lo, hi);
  CHECK(lo == 0.5f && hi == 1.0f);
  in::rumble_at(*f.find(3), 10.2, lo, hi);
  CHECK(lo == 0 && hi == 0);
  in::rumble(f, 3, 0.3f, 0, 0, 11);  // 0: until changed
  in::rumble_at(*f.find(3), 1000, lo, hi);
  CHECK(lo == 0.3f);
  f.forget(3);
  CHECK(f.find(3) == nullptr);
}

TEST(the_waves_and_the_ramp_have_their_shapes) {
  in::Effect e = make(in::EffectKind::sine);
  e.magnitude = 0.5f;
  e.period_s = 1;
  CHECK(near(in::effect_force(e, 0.25, 0, 0, 0), 0.5));
  CHECK(std::fabs(in::effect_force(e, 0.5, 0, 0, 0)) < 1e-12);
  e.kind = in::EffectKind::square;
  CHECK(in::effect_force(e, 0.1, 0, 0, 0) == 0.5 && in::effect_force(e, 0.6, 0, 0, 0) == -0.5);
  e.kind = in::EffectKind::triangle;
  CHECK(near(in::effect_force(e, 0.25, 0, 0, 0), 0.5));
  CHECK(near(in::effect_force(e, 0.75, 0, 0, 0), -0.5));
  e.kind = in::EffectKind::saw_up;
  CHECK(near(in::effect_force(e, 0.0, 0, 0, 0), -0.5));
  in::Effect r = make(in::EffectKind::ramp);
  r.start = 0;
  r.end = 1;
  r.duration_s = 2;
  CHECK(near(in::effect_force(r, 1, 0, 0, 0), 0.5));
  CHECK(in::effect_force(r, 2, 0, 0, 0) == 0);  // over
}

TEST(the_conditions_push_against_the_offset_the_speed_and_the_acceleration) {
  in::Effect s = make(in::EffectKind::spring);
  s.coeff = 2;
  s.deadband = 0.1f;
  s.saturation = 0.5f;
  CHECK(in::effect_force(s, 0, 0.05, 0, 0) == 0);
  CHECK(near(in::effect_force(s, 0, 0.2, 0, 0), -2 * (0.2 - static_cast<double>(0.1f))));
  CHECK(in::effect_force(s, 0, -0.9, 0, 0) == 0.5);  // saturated
  in::Effect d = make(in::EffectKind::damper);
  d.coeff = 0.1f;
  CHECK(near(in::effect_force(d, 0, 0, 3, 0), -0.3));
  in::Effect fr = make(in::EffectKind::friction);
  fr.coeff = 0.2f;
  CHECK(near(in::effect_force(fr, 0, 0, 5, 0), -static_cast<double>(0.2f)));
  CHECK(in::effect_force(fr, 0, 0, 0, 0) == 0);
  in::Effect in_ = make(in::EffectKind::inertia);
  in_.coeff = 0.01f;
  CHECK(in::effect_force(in_, 0, 0, 0, 10) < 0);
}

TEST(the_envelope_attacks_and_fades_and_a_delay_holds_the_start) {
  in::Effect e = make(in::EffectKind::constant);
  e.magnitude = 1;
  e.duration_s = 1;
  e.delay_s = 0.5f;
  e.envelope = {0.2f, 0, 0.2f, 0};
  CHECK(in::effect_force(e, 0.4, 0, 0, 0) == 0);
  CHECK(near(in::effect_force(e, 0.6, 0, 0, 0), 0.1 / static_cast<double>(0.2f)));
  CHECK(in::effect_force(e, 1.0, 0, 0, 0) == 1);
  CHECK(in::effect_force(e, 1.45, 0, 0, 0) < 0.3);
  CHECK(in::effect_force(e, 1.6, 0, 0, 0) == 0);
}

TEST(a_device_sums_its_effects_with_its_gain_and_autocentre_clamped_to_its_motor) {
  in::Feedback f;
  in::Effect push = make(in::EffectKind::constant);
  push.magnitude = 0.8f;
  const int a = in::play_effect(f, 9, push, 0);
  const int b = in::play_effect(f, 9, push, 0);
  CHECK(a == 0 && b == 1);
  CHECK(in::force_at(*f.find(9), 1, 0, 0, 0) == 1);  // 1.6, clamped
  f.get(9)->gain = 0.5f;
  CHECK(near(in::force_at(*f.find(9), 1, 0, 0, 0), static_cast<double>(0.8f)));
  in::stop_effect(f, 9, b);
  f.get(9)->autocenter = 0.5f;
  CHECK(near(in::force_at(*f.find(9), 1, 0.4, 0, 0), 0.8 * 0.5 - 0.5 * 0.4));
  for (int i = 0; i < in::kMaxEffects; i++) in::play_effect(f, 9, push, 0);
  CHECK(in::play_effect(f, 9, push, 0) == -1);  // every slot plays
}
