// A player with nothing to do (idle.hpp states the acts).
//
// ge:summary The idle acts drawn by traits, their lengths and pauses, the eased turn
// ge:keywords idle_begin idle_end idle_weights idle_turn idle-act bored
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooter/idle.hpp"

namespace ge::shooter {

namespace {

constexpr float kSpinRate = 540;  // degrees/s a spin turns at (the turn's own rate is the caller's)

float unit(uint8_t v) { return (static_cast<float>(v) - 1) / 9.f; }

}  // namespace

const char* idle_act_name(IdleAct a) {
  switch (a) {
    case IdleAct::none:
      return "none";
    case IdleAct::spin:
      return "spin";
    case IdleAct::look:
      return "look";
    case IdleAct::glance:
      return "glance";
    case IdleAct::hop:
      return "hop";
    case IdleAct::jump:
      return "jump";
    case IdleAct::sway:
      return "sway";
  }
  return "?";
}

IdleWeights idle_weights(const Traits& t) {
  const float moves = unit(t[Trait::moves]);
  const float calm = unit(t[Trait::discipline]);
  const float team = unit(t[Trait::teamplay]);
  IdleWeights w;
  w.w[static_cast<int>(IdleAct::spin)] = 0.3f + 0.6f * (1 - calm);
  w.w[static_cast<int>(IdleAct::look)] = 0.5f + 0.5f * calm;
  w.w[static_cast<int>(IdleAct::glance)] = 0.4f + 0.4f * team;
  w.w[static_cast<int>(IdleAct::hop)] = 0.15f + 0.85f * moves;
  w.w[static_cast<int>(IdleAct::jump)] = 0.2f + 0.4f * moves;
  w.w[static_cast<int>(IdleAct::sway)] = 0.3f;
  return w;
}

IdleAct idle_begin(IdleState& s, const IdleWeights& w, core::Rng& rng, double t, bool look_ok) {
  float total = 0;
  for (int i = 1; i < kIdleActs; i++)
    if (i != static_cast<int>(IdleAct::look) || look_ok) total += w.w[i];
  IdleAct pick = IdleAct::glance;
  if (total > 0) {
    float r = static_cast<float>(core::next(rng)) * total;
    for (int i = 1; i < kIdleActs; i++) {
      if (i == static_cast<int>(IdleAct::look) && !look_ok) continue;
      r -= w.w[i];
      pick = static_cast<IdleAct>(i);
      if (r < 0) break;
    }
  }
  s.act = pick;
  s.acts[static_cast<int>(pick)]++;
  s.flip_at = t;
  switch (pick) {
    case IdleAct::spin: {
      const float turns = core::chance(rng, 0.7) ? 360.f : 720.f;
      s.turn = core::chance(rng, 0.5) ? turns : -turns;
      s.until = t + static_cast<double>(turns / kSpinRate) + 0.3;
      break;
    }
    case IdleAct::look:
      s.until = t + core::range(rng, 1.5, 3.0);
      break;
    case IdleAct::glance:
      s.until = t + core::range(rng, 1.0, 2.0);
      break;
    case IdleAct::hop:
      s.until = t + core::range(rng, 2.5, 4.0);
      break;
    case IdleAct::jump:
      s.until = t + core::range(rng, 1.0, 2.5);
      break;
    case IdleAct::sway:
      s.turn = core::chance(rng, 0.5) ? 1.f : -1.f;
      s.until = t + core::range(rng, 1.5, 3.0);
      break;
    case IdleAct::none:
      break;
  }
  return pick;
}

void idle_end(IdleState& s, core::Rng& rng, double t) {
  s.act = IdleAct::none;
  s.turn = 0;
  s.next_at = t + core::range(rng, 0.6, 2.5);
}

float idle_turn(float now, float want, float rate, double dt) {
  float d = want - now;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  float step = rate * static_cast<float>(dt);
  const float ad = d < 0 ? -d : d;
  if (ad < 30) step *= ad / 30 > 0.15f ? ad / 30 : 0.15f;
  if (ad <= step) return now + d;
  return now + (d < 0 ? -step : step);
}

}  // namespace ge::shooter
