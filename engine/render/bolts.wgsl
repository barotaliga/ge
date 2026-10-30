// THE LIGHTNING'S CHANNEL (render/bolts.hpp): one camera-facing quad a segment, eight drawn
// widths across: a Gaussian core a drawn width wide and a soft halo round it, the light the rain
// and the air scatter close to the channel (night photographs show every channel in such a glow,
// never with a hard edge). Its luminance is the segment's intensity per metre over the drawn
// width, shared between the core and the halo, dimmed by the fog's or the fall's air on the way,
// added to the picture and marked reactive over the whole quad, so the TAA takes no history there.
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
  @location(0) across: f32,     // -kSpan .. kSpan drawn widths
  @location(1) light: f32,      // the core's luminance, pre-exposed
}

// The drawn width at distance `d`: the true width, never under two pixels.
fn drawn_width(w: f32, d: f32) -> f32 {
  return max(w, 2.0 * params.info.y * d);
}

// The quad's half-width, in drawn widths; the halo's share of the core's peak and its falloff.
const kSpan = 4.0;
const kHalo = 0.03;
const kHaloFall = 0.5;
const kHaloMost = 0.5;
// The brightest a core is drawn, pre-exposed: segments overlap at the joints, so half of a half
// float's largest (65504) leaves room for a few.
const kMostLight = 8000.0;
// The profile's integral across, in drawn widths: the core's sqrt(pi / 6) and the halo's.
const kProfile = 0.7236 + kHalo * 2.5066;

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
  let rel = p + side * (corner.y * kSpan * w);
  var out: VertexOut;
  out.position = frame.view_proj * vec4f(rel, 1.0);
  out.across = corner.y * kSpan;
  // A line source seen side on: its intensity per metre over the drawn width, shared by the
  // profile's integral across, through the air's extinction.
  // Held under half of a half float's largest, as the picture is one: a channel brighter than
  // that would store infinity, and every filter after it (the TAA's, the bloom's) would turn its
  // neighbours to NaN, a black rim round the channel.
  out.light = min(s.candela * params.info.z / (kProfile * w) * exp(-frame.weather.z * d),
                  kMostLight);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let x2 = in.across * in.across;
  let core = exp(-6.0 * x2);
  // The halo (its sqrt(pi / kHaloFall) integral is kProfile's second term) faded to nothing at
  // the quad's edge.
  let edge = 1.0 - smoothstep(0.6 * kSpan, kSpan, abs(in.across));
  // The halo is a glow, not a second channel: at most kHaloMost of white on the screen, so it
  // fades smoothly into the night however bright the core is.
  let halo = min(kHalo * in.light, kHaloMost / exposure()) * exp(-kHaloFall * x2) * edge;
  var out: FragmentOut;
  out.colour = vec4f(vec3f(0.86, 0.9, 1.0) * (in.light * core + halo), 0.0);
  out.motion = vec4f(0.0, 0.0, edge, 0.0);
  return out;
}
