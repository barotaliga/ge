// THE SNOW'S FINE MARKS' STAMPS (render/snow_marks.hpp): each instance one stamp, drawn as a quad
// over its groove, its berm and a texel more, into a map whose blend is MAX. The fragment writes
// the stamp's cross-section at its texel: the groove's depth and its pressed floor inside its
// outline, the berm's bump outside it, each as the map encodes it (the root of its share of the
// channel's full scale). A stamp narrower than a texel is widened to three quarters of one and
// scaled by how much of the texel it covers, so the coarse map holds a faint line, not a trench.
//
// THE OUTLINES (`shape`): 0 a capsule from a to b (a disc where a = b), the distance from its
// axis over the half width; 1 a box and 2 an ellipse, centred between a and b with b - a along
// their length, the half length half of it. `u` is 0 on the axis or at the centre and 1 on the
// outline in every case, so the cross-section and the berm read the same way for each.

struct Stamp {
  a: vec2f,
  b: vec2f,
  half_width: f32,
  depth: f32,
  berm: f32,
  wall: f32,
  shape: u32,
  pad0: u32,
  pad1: u32,
  pad2: u32,
}

struct MarkRect {
  x0: f32,
  z0: f32,
  span: f32,
  texel: f32,
}

@group(0) @binding(0) var<uniform> rect: MarkRect;
@group(0) @binding(1) var<storage, read> stamps: array<Stamp>;

const kDepth = 1.0;       // kMarkDepth
const kBerm = 0.15;       // kMarkBerm
const kBermReach = 0.9;   // kMarkBermReach

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

// How far `p` lies across the stamp: 0 on its axis or centre, 1 on its outline.
fn across_of(s: Stamp, p: vec2f, hw: f32) -> f32 {
  let ab = s.b - s.a;
  if (s.shape == 0u) {
    let t = clamp(dot(p - s.a, ab) / max(dot(ab, ab), 1e-12), 0.0, 1.0);
    return length(p - (s.a + ab * t)) / hw;
  }
  let len = length(ab);
  var dir = vec2f(1.0, 0.0);
  if (len > 1e-6) {
    dir = ab / len;
  }
  let q = p - 0.5 * (s.a + s.b);
  // The half length grows with the half width when the stamp is widened to a texel.
  let hl = max(0.5 * len, 1e-6) + (hw - s.half_width);
  let x = dot(q, dir) / hl;
  let y = dot(q, vec2f(-dir.y, dir.x)) / hw;
  if (s.shape == 1u) {
    // A box: inside, the nearer side; outside, the berm's reach measured from the outline.
    let ox = max(abs(x) - 1.0, 0.0) * hl / hw;
    let oy = max(abs(y) - 1.0, 0.0);
    if (ox > 0.0 || oy > 0.0) {
      return 1.0 + length(vec2f(ox, oy));
    }
    return max(abs(y), 1.0 - (1.0 - abs(x)) * hl / hw);
  }
  return length(vec2f(x, y));
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let s = stamps[in.i];
  let k = min(1.0, 2.0 * s.half_width / rect.texel);
  let hw = drawn_half(s);
  let u = across_of(s, in.p, hw);
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
