// THE SKY PASS (render/sky.hpp): the sky-view table behind everything the opaque passes drew,
// and the sun's disc. A full-screen triangle at the far plane (depth 0, reversed-z) tested
// `equal`, so only the pixels nothing covered are shaded. Its motion is the camera's turn alone:
// a direction at infinity does not move with the eye.
#include "render/wgsl/frame.wgsl"

// The sun's angular radius, rad.
const kSunRadius = 0.00465;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  out.uv = vec2f(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv); the sky turns with the camera only
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let d = view_ray(frame, in.uv);
  var colour = sky_luminance(d);
  // The disc: the sun's light over its solid angle, darkened toward the limb, and only where
  // the planet does not hide it; behind a deck it is lost in the deck's grey.
  let c = dot(d, frame.sun.xyz);
  let edge = cos(kSunRadius);
  if (c > edge) {
    let x = sqrt(max(1.0 - c * c, 0.0)) / sin(kSunRadius);
    let limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - x * x, 0.0)));
    let disc = sun_light(frame.eye.y) / (PI * kSunRadius * kSunRadius);
    colour += disc * limb * (1.0 - frame.weather.y);
  }
  var out: FragmentOut;
  out.colour = vec4f(min(colour, vec3f(60000.0)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(d, 0.0), in.position.xy), 0.0, 0.0);
  return out;
}
