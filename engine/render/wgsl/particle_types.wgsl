// A particle's slot and the pool's parameters (render/particles.hpp's `Particle` and the
// params block), shared by the flight and the draw.

struct Particle {
  p: vec3f,
  age: f32,
  v: vec3f,
  life: f32,
  size: f32,
  drag: f32,
  fall: f32,
  streak: f32,
  albedo: f32,
  opacity: f32,
  pad: vec2f,
}

struct ParticleParams {
  step: vec4f,  // x: this frame's step, s; y: the near plane, m; z: the soft depth, m
  info: vec4u,  // x: slots
  air: vec4f,   // xyz: the air's velocity, m/s, which a particle's drag pulls it toward
}

const kGravity = 9.81;
