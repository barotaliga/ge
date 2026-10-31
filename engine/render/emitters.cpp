// The area emitters' births: the cadence, each kind's draw, the embers over a glow.
//
// ge:summary The area emitters' seeded births by kind, and the embers over a glowing volume
// ge:keywords render emitter embers motes drips steam births
// ge:skill render
// ge:test tests/render_emitters_test.cpp
#include "render/emitters.hpp"

#include <algorithm>
#include <cmath>

#include "core/prng.hpp"

namespace ge::render {

namespace {

constexpr double kFlightStep = 1.0 / 60;  // s

float f32(double v) { return static_cast<float>(v); }

// Birth `k` of `e`, born at `t` (its age 0), placed and launched by its kind.
Particle birth(const AreaEmitter& e, uint64_t k) {
  core::Rng rng = core::make_rng(e.seed * 0x9e3779b1u ^ static_cast<uint32_t>(k) ^
                                 static_cast<uint32_t>(k >> 32) * 0x85ebca6bu);
  core::next(rng);  // the slot's jitter, drawn first (`moment`)
  Particle p{};
  const double floor_y = e.centre[1] - e.half[1];
  const double top_y = e.centre[1] + e.half[1];
  auto across = [&](double share) {
    p.p[0] = f32(e.centre[0] + core::range(rng, -share, share) * e.half[0]);
    p.p[2] = f32(e.centre[2] + core::range(rng, -share, share) * e.half[2]);
  };
  switch (e.kind) {
    case AreaEmitter::Kind::embers:
      across(0.6);
      p.p[1] = f32(floor_y + core::range(rng, 0, 0.3) * e.half[1]);
      p.v[0] = f32(core::range(rng, -0.4, 0.4));
      p.v[1] = f32(core::range(rng, 0.8, 2.0));
      p.v[2] = f32(core::range(rng, -0.4, 0.4));
      p.life = f32(core::range(rng, 1.5, 3.0));
      p.size = 0.03f;  // a glowing fleck, its glow smeared a few times wider by the eye
      p.drag = 0.8f;
      p.fall = -0.06f;   // the hot air lifts it: up to some 0.7 m/s
      p.streak = 0.12f;  // an ember is seen as the streak it draws
      p.albedo = 0.2f;
      p.opacity = 0.9f;
      p.kelvin = f32(core::range(rng, 1300, 1700));
      p.cool_s = f32(core::range(rng, 0.8, 1.6));
      break;
    case AreaEmitter::Kind::motes:
      across(1.0);
      p.p[1] = f32(e.centre[1] + core::range(rng, -1, 1) * e.half[1]);
      for (float& v : p.v) v = f32(core::range(rng, -0.05, 0.05));
      p.life = f32(core::range(rng, 4, 8));
      p.size = 0.004f;
      p.drag = 0.5f;
      p.fall = 0.005f;
      p.albedo = 0.9f;
      p.opacity = 0.5f;
      break;
    case AreaEmitter::Kind::drips: {
      across(1.0);
      p.p[1] = f32(top_y);
      // Its life: the fall to the floor, and a little over.
      p.life = f32(1.05 * std::sqrt(2 * std::max(top_y - floor_y, 0.01) / kGravity));
      p.size = 0.008f;
      p.drag = 0.05f;
      p.fall = 1;
      p.streak = 0.03f;
      p.albedo = 0.5f;
      p.opacity = 0.7f;
      break;
    }
    case AreaEmitter::Kind::steam:
      across(0.9);
      p.p[1] = f32(floor_y);
      p.v[0] = f32(core::range(rng, -0.05, 0.05));
      p.v[1] = f32(core::range(rng, 0.1, 0.3));
      p.v[2] = f32(core::range(rng, -0.05, 0.05));
      p.life = f32(core::range(rng, 2, 4));
      p.size = 0.25f;
      p.drag = 1.0f;
      p.fall = -0.02f;
      p.albedo = 1;
      p.opacity = 0.08f;
      break;
  }
  return p;
}

// Birth `k`'s moment, s: its slot's start for a drip, anywhere in its slot otherwise.
double moment(const AreaEmitter& e, uint64_t k) {
  if (e.kind == AreaEmitter::Kind::drips) return static_cast<double>(k) / e.rate;
  core::Rng rng = core::make_rng(e.seed * 0x9e3779b1u ^ static_cast<uint32_t>(k) ^
                                 static_cast<uint32_t>(k >> 32) * 0x85ebca6bu);
  return (static_cast<double>(k) + core::next(rng)) / e.rate;
}

}  // namespace

void emit_area(const AreaEmitter& e, double t0, double t1, std::vector<Particle>& out) {
  if (!(e.rate > 0) || !(t1 > t0) || t1 < 0) return;
  // A long span keeps its latest births only.
  t0 = std::max({t0, t1 - kMaxAreaBirths / e.rate, 0.0});
  const auto first = static_cast<uint64_t>(std::floor(t0 * e.rate));
  const auto last = static_cast<uint64_t>(std::floor(t1 * e.rate));
  for (uint64_t k = first; k <= last; ++k) {
    const double at = moment(e, k);
    if (at < t0 || at >= t1) continue;
    Particle p = birth(e, k);
    // Flown as the pool flies it, a sixtieth of a second at a time.
    const double flight = t1 - at;
    const auto steps = static_cast<int>(std::ceil(flight / kFlightStep));
    for (int i = 0; i < steps; ++i) {
      step_particle(p, f32(std::min(flight - i * kFlightStep, kFlightStep)));
    }
    out.push_back(p);
  }
}

AreaEmitter embers_over(const FogVolume& v, double floor_y, uint32_t seed) {
  AreaEmitter e;
  e.kind = AreaEmitter::Kind::embers;
  for (int a = 0; a < 3; ++a) {
    e.centre[a] = v.centre[a];
    e.half[a] = v.half[a];
  }
  if (v.hug_ground) e.centre[1] = floor_y + v.half[1];
  e.rate = std::clamp(fog_glow_light(v, floor_y).candela / 10.0, 4.0, 60.0);
  e.seed = seed;
  return e;
}

}  // namespace ge::render
