// DEPTH OF FIELD (render/dof.hpp): one half-size texel a thread. Each texel gathers kTaps taps of
// the post pass's input over a disc as wide as the widest blur circle, each tap kept as far as its
// own circle reaches the texel (a gather standing in for each tap's scatter). A tap behind the
// texel is held to the texel's own circle, so a sharp subject never takes the blurred background's
// light over its edge. A texel in focus keeps its own light (the post pass, mixing by each pixel's
// own circle, never takes it). Alpha is the texel's own circle.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/dof_coc.wgsl"

@group(1) @binding(0) var src: texture_2d<f32>;  // the post pass's input, at the output's size
@group(1) @binding(1) var lin: sampler;
@group(1) @binding(2) var depth: texture_depth_2d;  // at the render size
@group(1) @binding(3) var dst: texture_storage_2d<rgba16float, write>;
struct Params {
  lens: vec4f,  // the focus (m), the sharp share, the widest circle (a share of the height)
}
@group(1) @binding(4) var<uniform> params: Params;

const kTaps = 16;
const kGolden = 2.39996323;  // the golden angle, rad: taps spread evenly over the disc

// The blur circle and the distance (m) of what is drawn at uv.
fn circle_at(uv: vec2f) -> vec2f {
  let size = vec2i(textureDimensions(depth));
  let p = clamp(vec2i(uv * vec2f(size)), vec2i(0), size - 1);
  let dist = dof_distance(frame.inv_view_proj, uv, textureLoad(depth, p, 0));
  return vec2f(dof_circle(dist, params.lens.x, params.lens.y), dist);
}

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3u) {
  let half = textureDimensions(dst);
  if (any(id.xy >= half)) {
    return;
  }
  let full = vec2f(textureDimensions(src));
  let uv = (vec2f(id.xy) + 0.5) / vec2f(half);
  let own = circle_at(uv);
  let widest = params.lens.z * full.y;  // output pixels
  var sum = textureSampleLevel(src, lin, uv, 0.0).rgb;
  // A texel in focus is mixed in by no pixel round it (post reads each pixel's own circle).
  if (own.x * widest < 0.5) {
    textureStore(dst, vec2i(id.xy), vec4f(sum, own.x));
    return;
  }
  var weights = 1.0;
  // The disc turned per texel, so the taps' gaps break up rather than print a pattern.
  let turn = fract(52.9829189 * fract(dot(vec2f(id.xy), vec2f(0.06711056, 0.00583715)))) * 6.2832;
  for (var i = 0; i < kTaps; i++) {
    let r = sqrt((f32(i) + 0.5) / f32(kTaps)) * widest;
    let a = f32(i) * kGolden + turn;
    let q = uv + vec2f(cos(a), sin(a)) * r / full;
    let d = circle_at(q);
    // A tap farther than the texel blurs it no more than the texel's own circle allows.
    let reach = select(d.x, min(d.x, own.x), d.y > own.y) * widest;
    // Under half a pixel a circle reaches nothing.
    let w = select(clamp(reach - r + 1.0, 0.0, 1.0), 0.0, reach < 0.5);
    sum += textureSampleLevel(src, lin, q, 0.0).rgb * w;
    weights += w;
  }
  textureStore(dst, vec2i(id.xy), vec4f(sum / weights, own.x));
}
