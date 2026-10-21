// The per-frame block every pass reads: render/frame.hpp's `FrameUniforms`, field for field.
// Positions are CAMERA-RELATIVE (world minus `eye.xyz`) in every matrix; depth is reversed-z
// with an infinite far plane (1 at the near plane, 0 at infinity).

struct Frame {
  view_proj: mat4x4f,      // camera-relative world -> clip
  inv_view_proj: mat4x4f,  // clip -> camera-relative world
  eye: vec4f,              // world xyz (m); w: the eye's altitude over the sea, km
  sun: vec4f,              // unit direction toward the sun; w: its illuminance
  size: vec4f,             // render width, height, 1/width, 1/height
  time: vec4f,             // seconds, frame number
  world: vec4f,            // x: y = 0's altitude over the sea, km
  planes: array<vec4f, 6>, // the frustum: left, right, bottom, top, near (and one zero)
}

const PI = 3.14159265358979;

// The camera-relative direction through a pixel's uv (0,0 top left .. 1,1 bottom right).
fn view_ray(f: Frame, uv: vec2f) -> vec3f {
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let p = f.inv_view_proj * vec4f(ndc, 1.0, 1.0);
  return normalize(p.xyz / p.w);
}
