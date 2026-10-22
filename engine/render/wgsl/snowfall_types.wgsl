// The falling snow's parameters (render/snowfall.hpp), shared by its flight and its draw, and a
// flake's own motion: its fall's pace by its index and its flutter on its phase.

struct SnowfallParams {
  eye: vec4f,   // xyz: the eye, m; w: the box's side, m
  wind: vec4f,  // xyz: the air's velocity, m/s; w: this frame's step, s
  step: vec4f,  // x: the flutter's clock, s; y: the near plane, m; z: the soft depth, m; w: size, m
  info: vec4u,  // x: flakes; y: flakes drawn
}

// A flake's velocity at `time`: the air's, its own fall, and its flutter.
fn flake_velocity(sp: SnowfallParams, i: u32, phase: f32) -> vec3f {
  let pace = 0.6 + 0.8 * fract(f32(i) * 0.61803398875);
  let t = sp.step.x;
  let flutter = 0.35 * vec3f(sin(t * 1.7 + phase), 0.3 * sin(t * 2.3 + phase * 2.0),
                             cos(t * 1.3 + phase * 1.7));
  return sp.wind.xyz + vec3f(0.0, -pace, 0.0) + flutter;
}
