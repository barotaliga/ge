// THE TRAIL MAP'S STAMPS (mountain/app/trails.hpp): each instance one capsule, drawn as a quad
// over its groove, its berm and a texel more, into a map whose blend is MAX. The fragment writes
// the capsule's cross-section at its texel: the groove's depth and its pressed floor inside the
// half width, the berm's bump outside it, each as the map encodes it (the root of its share of the
// channel's full scale). A stamp narrower than a texel is widened to three quarters of one and
// scaled by how much of the texel it covers, so the coarse map holds a faint line, not a trench.

struct Stamp {
  a: vec2f,
  b: vec2f,
  half_width: f32,
  depth: f32,
  berm: f32,
  wall: f32,
}

struct TrailRect {
  x0: f32,
  z0: f32,
  span: f32,
  texel: f32,
}

@group(0) @binding(0) var<uniform> rect: TrailRect;
@group(0) @binding(1) var<storage, read> stamps: array<Stamp>;

const kDepth = 1.0;       // kTrailDepth
const kBerm = 0.15;       // kTrailBerm
const kBermReach = 0.9;   // kTrailBermReach

struct VertexOut {
  @builtin(position) pos: vec4f,
  @location(0) p: vec2f,
  @location(1) @interpolate(flat) i: u32,
}

// The groove's half width as drawn: never under three quarters of a texel.
fn drawn_half(s: Stamp) -> f32 {
  return max(s.half_width, 0.75 * rect.texel);
}

@vertex
fn vs_main(@builtin(vertex_index) v: u32, @builtin(instance_index) i: u32) -> VertexOut {
  let s = stamps[i];
  let reach = drawn_half(s) * (1.0 + kBermReach) + rect.texel;
  let d = s.b - s.a;
  let len = length(d);
  var dir = vec2f(1.0, 0.0);
  if (len > 1e-6) {
    dir = d / len;
  }
  let across = vec2f(-dir.y, dir.x);
  // Two triangles: (a, -), (b, -), (a, +); (a, +), (b, -), (b, +).
  let at_b = v == 1u || v == 4u || v == 5u;
  let side = select(-1.0, 1.0, v == 2u || v == 3u || v == 5u);
  let end = select(s.a - dir * reach, s.b + dir * reach, at_b);
  let p = end + across * side * reach;
  let uv = (p - vec2f(rect.x0, rect.z0)) / rect.span;
  var out: VertexOut;
  out.pos = vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
  out.p = p;
  out.i = i;
  return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let s = stamps[in.i];
  let k = min(1.0, 2.0 * s.half_width / rect.texel);
  let hw = drawn_half(s);
  let ab = s.b - s.a;
  let t = clamp(dot(in.p - s.a, ab) / max(dot(ab, ab), 1e-12), 0.0, 1.0);
  let u = length(in.p - (s.a + ab * t)) / hw;
  var depth = 0.0;
  var pressed = 0.0;
  if (u < 1.0) {
    depth = s.depth * k * (1.0 - pow(u, s.wall));
    pressed = k;
  }
  let w = (u - 1.0) / kBermReach;
  var berm = 0.0;
  if (w > 0.0 && w < 1.0) {
    berm = s.berm * k * 4.0 * w * (1.0 - w);
  }
  if (depth <= 0.0 && berm <= 0.0) {
    discard;
  }
  return vec4f(sqrt(depth / kDepth), sqrt(berm / kBerm), pressed, 0.0);
}
