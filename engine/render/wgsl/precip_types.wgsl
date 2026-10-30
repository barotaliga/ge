// What falls round the eye (render/precip.hpp): the parameters its flight and its draw share, and
// a particle's own motion: a flake's fall by its index and its flutter on its phase, a drop's fall
// at the rain's pace, and the storm's eddies that carry both about the wind.

struct PrecipParams {
  eye: vec4f,   // xyz: the eye, m; w: the box's side, m
  wind: vec4f,  // xyz: the air's velocity, m/s; w: this frame's step, s
  step: vec4f,  // x: the clock, s; y: the near plane, m; z: the soft depth, m; w: a flake's size, m
  fall: vec4f,  // x: the rain share; y: a drop's fall, m/s; z: the eddies' speed, m/s; w: a drop's width, m
  info: vec4u,  // x: particles; y: particles drawn; z: splashes drawn; w: 0
  splash: vec4f,  // x: the splashes' reach, m; y: a splash's life, s; z: its size, m; w: 0
  // THE SHELTER's orthographic camera (render/precip.hpp's PrecipShelter): o (world m; w: the
  // half width, m), its axes x, y and z (z along the fall); info: x 1 when there is one, y the
  // half depth (m), z the layer, w a texel (m).
  shelter_o: vec4f,
  shelter_x: vec4f,
  shelter_y: vec4f,
  shelter_z: vec4f,
  shelter_info: vec4f,
}

// Whether particle `i` is a drop: the first `rain` share of every index's rank, so a rising rain
// share turns flakes to drops one by one and a sleet is both.
fn is_drop(sp: PrecipParams, i: u32) -> bool {
  return fract(f32(i) * 0.7548776662) < sp.fall.x;
}

// THE EDDIES: a gusty wind's turbulence as a slow field of swirls some metres across that carry
// every particle near one the same way, so a storm's snow swirls in sheets rather than jittering
// flake by flake.
fn eddies(sp: PrecipParams, p: vec3f) -> vec3f {
  let t = sp.step.x;
  let s = vec3f(sin(p.z * 0.31 + t * 0.83 + 1.3 * sin(p.y * 0.27 + t * 0.5)),
                0.5 * sin(p.x * 0.29 - t * 0.71 + p.z * 0.13),
                cos(p.x * 0.33 + t * 0.67 + 1.1 * sin(p.y * 0.23 - t * 0.4)));
  return sp.fall.z * s;
}

// A particle's velocity at `p`: the air's and its eddies, and its own fall (a flake's pace and
// flutter, or a drop's pace).
fn particle_velocity(sp: PrecipParams, i: u32, phase: f32, p: vec3f) -> vec3f {
  let rank = fract(f32(i) * 0.61803398875);
  if (is_drop(sp, i)) {
    // A heavy drop shrugs off most of the eddies.
    return sp.wind.xyz + 0.3 * eddies(sp, p) + vec3f(0.0, -sp.fall.y * (0.75 + 0.5 * rank), 0.0);
  }
  let pace = 0.6 + 0.8 * rank;
  let t = sp.step.x;
  let flutter = 0.35 * vec3f(sin(t * 1.7 + phase), 0.3 * sin(t * 2.3 + phase * 2.0),
                             cos(t * 1.3 + phase * 1.7));
  return sp.wind.xyz + eddies(sp, p) + vec3f(0.0, -pace, 0.0) + flutter;
}
