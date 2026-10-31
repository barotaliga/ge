// What the frame throws into the air each frame: the spray, the breath, the area emitters.
//
// ge:summary Fall Line's particles each frame: spray, breath and area births into the pool
// ge:keywords fall-line particles spray breath births emit_grains
// ge:skill render skier
#include "fall_line/app/thrown.hpp"

#include "mountain/air/air.hpp"
#include "wind/climate.hpp"
#include "wind/cold.hpp"

namespace fall_line::app {

namespace {

namespace r = ge::render;

// The grains in `f.thrown` into the particle pool.
void emit_grains(GameFrame& f) {
  f.births.resize(f.thrown.size());
  for (size_t i = 0; i < f.thrown.size(); ++i) {
    const ge::skier::SprayGrain& g = f.thrown[i];
    r::Particle& p = f.births[i];
    p = r::Particle{};
    p.p[0] = static_cast<float>(g.x);
    p.p[1] = static_cast<float>(g.y);
    p.p[2] = static_cast<float>(g.z);
    p.v[0] = static_cast<float>(g.vx);
    p.v[1] = static_cast<float>(g.vy);
    p.v[2] = static_cast<float>(g.vz);
    p.life = static_cast<float>(g.life);
    p.size = static_cast<float>(g.size);
    p.drag = static_cast<float>(g.drag);
    p.fall = static_cast<float>(g.fall);
    p.streak = static_cast<float>(g.streak);
    p.albedo = static_cast<float>(g.albedo);
    p.opacity = static_cast<float>(g.opacity);
  }
  r::emit_particles(f.particles, f.births);
}

}  // namespace

void throw_spray(GameFrame& f, const FrameScene& scene, double dt) {
  if (!f.options.spray) return;
  for (size_t k = f.spray_pens.size(); k < scene.skiers.size(); ++k) {
    f.spray_pens.push_back(ge::skier::spray_pen(static_cast<uint32_t>(k)));
  }
  f.thrown.clear();
  for (size_t k = 0; k < scene.skiers.size(); ++k) {
    ge::skier::spray_grains(scene.skiers[k], dt, f.spray_pens[k], f.thrown);
  }
  emit_grains(f);
}

void breathe(GameFrame& f, const FrameScene& scene, double hour, double dt) {
  if (!f.options.breath || scene.skiers.empty()) return;
  for (size_t k = f.breath_pens.size(); k < scene.skiers.size(); ++k)
    f.breath_pens.push_back(ge::skier::breath_pen(static_cast<uint32_t>(k)));
  const ge::wind::Climate climate = ge::mountain::air::climate_of(*f.level);
  f.thrown.clear();
  for (size_t k = 0; k < scene.skiers.size(); ++k) {
    double mouth[3];
    ge::skier::mouth_of(scene.skiers[k], mouth);
    const ge::wind::Site site{.y = mouth[1], .snow = 1};
    const double show = ge::wind::breath_of(ge::wind::air_at(climate, site, hour));
    ge::skier::breath_puffs(scene.skiers[k], show, dt, f.breath_pens[k], f.thrown);
  }
  emit_grains(f);
}

void emit_areas(GameFrame& f, const FrameScene& scene, double t0, double t1) {
  if (scene.emitters.empty()) return;
  f.births.clear();
  for (const r::AreaEmitter& e : scene.emitters) r::emit_area(e, t0, t1, f.births);
  r::emit_particles(f.particles, f.births);
}

}  // namespace fall_line::app
