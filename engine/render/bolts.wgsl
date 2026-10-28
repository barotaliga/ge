// THE LIGHTNING'S CHANNEL (render/bolts.hpp): one camera-facing quad a segment, two drawn
// widths across with a Gaussian core; its luminance the segment's intensity per metre over the
// drawn width, dimmed by the fog's or the fall's air on the way, added to the picture and marked
// reactive.
#include "render/wgsl/frame.wgsl"

struct BoltParams {
  info: vec4f,  // the segments, a pixel's angle (rad), the light's scale (pre-exposed over lux)
}

struct Segment {
  a: vec3f,
  width: f32,
  b: vec3f,
  candela: f32,
}

@group(1) @binding(0) var<uniform> params: BoltParams;
@group(1) @binding(1) var<storage, read> segments: array<Segment>;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) across: f32,     // -1 .. 1 drawn widths
  @location(1) light: f32,      // the core's luminance, pre-exposed
}

// The drawn width at distance `d`: the true width, never under one and a half pixels.
fn drawn_width(w: f32, d: f32) -> f32 {
  return max(w, 1.5 * params.info.y * d);
}

@vertex
fn vs_main(@builtin(vertex_index) v: u32, @builtin(instance_index) i: u32) -> VertexOut {
  let s = segments[i];
  let a = s.a - frame.eye.xyz;
  let b = s.b - frame.eye.xyz;
  let corner = array<vec2f, 6>(vec2f(0.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                               vec2f(0.0, -1.0), vec2f(1.0, 1.0), vec2f(0.0, 1.0))[v];
  let p = mix(a, b, corner.x);
  let along = b - a;
  let d = max(length(p), 1.0);
  var side = cross(along, p);
  side = side / max(length(side), 1e-6);
  let w = drawn_width(s.width, d);
  let rel = p + side * (corner.y * w);
  var out: VertexOut;
  out.position = frame.view_proj * vec4f(rel, 1.0);
  out.across = corner.y;
  // A line source seen side on: its intensity per metre over the drawn width (the Gaussian's
  // integral across is 0.724 of it), through the air's extinction.
  out.light = s.candela * params.info.z / (0.724 * w) * exp(-frame.weather.z * d);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let core = exp(-6.0 * in.across * in.across);
  var out: FragmentOut;
  out.colour = vec4f(vec3f(0.86, 0.9, 1.0) * in.light * core, 0.0);
  out.motion = vec4f(0.0, 0.0, core, 0.0);
  return out;
}
