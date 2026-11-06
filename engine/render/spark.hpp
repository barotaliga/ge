// A THROWN SPECK INTO THE POOL: engine/particles' `Spark` (an effect thrown off its text) copied
// into the render pool's `Particle` (render/particles.hpp), field for field, born now.
//
// ge:summary A data-driven effect's speck copied into the render pool's particle
// ge:keywords spark to_particle particles particle-effects pool birth
// ge:skill render
// ge:test tests/particles_effect_test.cpp
#pragma once

#include <span>
#include <vector>

#include "particles/effect.hpp"
#include "render/particles.hpp"

namespace ge::render {

inline Particle to_particle(const ge::particles::Spark& s) {
  Particle p{};
  for (int k = 0; k < 3; ++k) {
    p.p[k] = s.p[k];
    p.v[k] = s.v[k];
    p.tint[k] = s.tint[k];
  }
  p.age = 0;
  p.life = s.life;
  p.size = s.size;
  p.drag = s.drag;
  p.fall = s.fall;
  p.streak = s.streak;
  p.albedo = s.albedo;
  p.opacity = s.opacity;
  p.kelvin = s.kelvin;
  p.cool_s = s.cool_s;
  p.grow = s.grow;
  p.floor_y = s.floor_y;
  p.bounce = s.bounce;
  return p;
}

// Every speck of `sparks` appended to `out`.
inline void add_sparks(std::span<const ge::particles::Spark> sparks, std::vector<Particle>& out) {
  out.reserve(out.size() + sparks.size());
  for (const ge::particles::Spark& s : sparks) out.push_back(to_particle(s));
}

}  // namespace ge::render
