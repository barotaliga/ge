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
  kelvin: f32,
  cool_s: f32,
}

struct ParticleParams {
  step: vec4f,  // x: this frame's step, s; y: the near plane, m; z: the soft depth, m
  info: vec4u,  // x: slots
  air: vec4f,   // xyz: the air's velocity, m/s, which a particle's drag pulls it toward
  glow: vec4f,  // x: a perfect radiator's luminance at the table's last row times the
                // emissivity, cd/m^2
  table: array<vec4f, 64>,  // render/blackbody.hpp's rows
}

const kGravity = 9.81;
