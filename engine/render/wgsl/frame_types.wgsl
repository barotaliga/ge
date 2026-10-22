// The per-frame block every pass reads: render/frame.hpp's `FrameUniforms`, field for field.
// Positions are CAMERA-RELATIVE (world minus `eye.xyz`) in every matrix; depth is reversed-z
// with an infinite far plane (1 at the near plane, 0 at infinity).

struct Frame {
  view_proj: mat4x4f,      // camera-relative world -> clip, jittered
  inv_view_proj: mat4x4f,  // clip -> camera-relative world, jittered
  prev_view_proj: mat4x4f, // the last frame's camera-relative world -> clip, unjittered
  eye: vec4f,              // world xyz (m); w: the eye's altitude over the sea, km
  sun: vec4f,              // unit direction toward the sun; w: its illuminance
  size: vec4f,             // render width, height, 1/width, 1/height
  time: vec4f,             // seconds, frame number
  world: vec4f,            // x: y = 0's altitude over the sea, km
  planes: array<vec4f, 6>, // the frustum: left, right, bottom, top, near (and one zero)
  eye_delta: vec4f,        // this eye minus the last frame's, m
  jitter: vec4f,           // the sub-pixel offset: render pixels (x right, y down), then uv
}

// THE MOTION a surface at camera-relative `rel` (w = 1; w = 0 for a direction at infinity) made
// since the last frame, in uv (this frame's unjittered uv minus the last frame's), from the pixel
// it lands on (`frag`, a fragment's position.xy).
fn screen_motion(f: Frame, rel: vec4f, frag: vec2f) -> vec2f {
  let prev = f.prev_view_proj * (rel + vec4f(f.eye_delta.xyz * rel.w, 0.0));
  let prev_uv = vec2f(prev.x / prev.w * 0.5 + 0.5, 0.5 - prev.y / prev.w * 0.5);
  let uv = frag * f.size.zw - f.jitter.zw;
  return uv - prev_uv;
}

const PI = 3.14159265358979;

// The camera-relative direction through a pixel's uv (0,0 top left .. 1,1 bottom right).
fn view_ray(f: Frame, uv: vec2f) -> vec3f {
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let p = f.inv_view_proj * vec4f(ndc, 1.0, 1.0);
  return normalize(p.xyz / p.w);
}
