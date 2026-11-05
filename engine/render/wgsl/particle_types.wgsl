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
  tint: vec3f,  // vec3f's 16-byte alignment: before `grow`
  grow: f32,
  floor_y: f32,  // its own floor (world y, m), where it hops by `bounce` and comes to rest
  bounce: f32,
  hops: f32,  // the hops it has made (render/particles.hpp's max_hops)
  pad0: f32,
}

const kFloorRest = 0.35;   // m/s: slower than this down, it lies still
const kFloorSlide = 0.55;  // its way along the floor kept at a hop

const kMaxHops = 2.0;

// Twice, a third of them three times: its own, off its size and life (render/particles.cpp's).
fn max_hops(p: Particle) -> f32 {
  return select(kMaxHops, kMaxHops + 1.0, fract(p.size * 977.0 + p.life * 131.0) > 0.66);
}

// Its own floor: a hop back up at `bounce` of its speed while it comes down fast and has hops
// left, else at rest.
fn on_floor(q: Particle) -> Particle {
  var p = q;
  if (p.p.y >= p.floor_y || p.v.y >= 0.0) {
    return p;
  }
  p.p.y = p.floor_y;
  if (-p.v.y * p.bounce > kFloorRest && p.hops < max_hops(p)) {
    p.v = vec3f(p.v.x * kFloorSlide, -p.v.y * p.bounce, p.v.z * kFloorSlide);
    p.hops += 1.0;
  } else {
    p.v = vec3f(0.0);
    p.age = max(p.age, p.life * 0.7);
  }
  return p;
}

struct ParticleParams {
  step: vec4f,  // x: this frame's step, s; y: the near plane, m; z: the soft depth, m
  info: vec4u,  // x: slots
  air: vec4f,   // xyz: the air's velocity, m/s, which a particle's drag pulls it toward
  glow: vec4f,  // x: a perfect radiator's luminance at the table's last row times the
                // emissivity, cd/m^2
  seen_vp: mat4x4f,   // the camera the depth target was drawn by: its eye-relative world -> clip
  seen_inv: mat4x4f,  // and back
  seen_eye: vec4f,    // xyz: its eye, world m; w: 1 when the pool meets what it drew
  table: array<vec4f, 64>,  // render/blackbody.hpp's rows
}

const kGravity = 9.81;
