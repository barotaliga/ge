// AMBIENT OCCLUSION (render/ao.hpp): one half-size texel a thread. The texel stands for one of
// the four render pixels under it (a different one each frame); its surface is rebuilt from the
// depth, its normal from the depth beside it, and its sky is searched along kSlices slices
// through the view ray, kSteps steps out each way: the highest thing within the radius on each
// side is that side's horizon, and the arc of sky between the two horizons, weighted by the
// cosine to the normal, is the share of the sky's light the surface gets (a ground-truth horizon
// search: the integral is exact for a slice; the slices and the steps are what is sampled).
#include "render/wgsl/frame_types.wgsl"

override kSlices: u32 = 2u;
override kSteps: u32 = 4u;

@group(0) @binding(0) var<uniform> frame: Frame;
struct Params {
  look: vec4f,  // the radius (m), the strength (0..1)
}
@group(0) @binding(1) var<uniform> params: Params;
@group(0) @binding(2) var depth: texture_depth_2d;  // reversed-z: 0 where nothing was drawn
@group(0) @binding(3) var dst: texture_storage_2d<rgba16float, write>;

const kHalfPi = 1.57079632679490;
const kFalloff = 0.6;    // the outer share of the radius over which a blocker fades out
const kMostPx = 160.0;   // the farthest a step reaches, render pixels: a surface at the lens
const kLeastPx = 2.0;    // a radius under this many pixels shades nothing worth the search
const kFloor = 0.06;     // the occlusion read as none

// The camera-relative point drawn at render pixel `p` at device depth `z`.
fn point_at(p: vec2f, z: f32) -> vec3f {
  let uv = p * frame.size.zw;
  let c = frame.inv_view_proj * vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, z, 1.0);
  return c.xyz / c.w;
}

fn depth_at(p: vec2i) -> f32 {
  return textureLoad(depth, clamp(p, vec2i(0), vec2i(frame.size.xy) - 1), 0);
}

// The nearer of the two differences beside a point, so an edge takes the side it belongs to.
fn nearer(c: vec3f, a: vec3f, b: vec3f) -> vec3f {
  let da = c - a;
  let db = b - c;
  return select(db, da, dot(da, da) < dot(db, db));
}

// Interleaved gradient noise (a pixel's place in a pattern every small window holds evenly).
fn noise(p: vec2f) -> f32 {
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
}

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3u) {
  let half = textureDimensions(dst);
  if (any(id.xy >= half)) {
    return;
  }
  let t = vec2i(id.xy);
  let n_frame = u32(frame.time.y);
  // The render pixel this frame's texel stands for.
  let sub = vec2i(i32(n_frame & 1u), i32((n_frame >> 1u) & 1u));
  let px = t * 2 + sub;
  let z = depth_at(px);
  if (z <= 0.0) {
    textureStore(dst, t, vec4f(0.0, 0.0, 0.0, 1.0));
    return;
  }
  let pc = vec2f(px) + 0.5;
  let p = point_at(pc, z);
  let v = normalize(-p);
  // The normal off the depth beside the pixel, turned to face the eye.
  let l = point_at(pc - vec2f(1.0, 0.0), depth_at(px - vec2i(1, 0)));
  let r = point_at(pc + vec2f(1.0, 0.0), depth_at(px + vec2i(1, 0)));
  let u = point_at(pc - vec2f(0.0, 1.0), depth_at(px - vec2i(0, 1)));
  let d = point_at(pc + vec2f(0.0, 1.0), depth_at(px + vec2i(0, 1)));
  var n = normalize(cross(nearer(p, l, r), nearer(p, u, d)));
  if (dot(n, v) < 0.0) {
    n = -n;
  }
  let radius = params.look.x;
  let range = radius * kFalloff;
  // The metres a render pixel spans at this depth: a step across the picture at the same depth
  // stays in the image plane.
  let across = point_at(pc + vec2f(1.0, 0.0), z) - p;
  let reach_px = min(radius / max(length(across), 1e-6), kMostPx);
  if (reach_px < kLeastPx) {
    textureStore(dst, t, vec4f(0.0, z, 0.0, 1.0));
    return;
  }
  let shift = f32(n_frame % 64u) * 5.588238;
  let turn = noise(vec2f(t) + shift);
  let jitter = noise(vec2f(f32(t.y), f32(t.x)) + shift + 17.0);
  var seen = 0.0;
  for (var s = 0u; s < kSlices; s++) {
    let phi = (f32(s) + turn) * (PI / f32(kSlices));
    let w = vec2f(cos(phi), sin(phi));  // the slice's direction on screen, render pixels
    let q = point_at(pc + w, z) - p;
    let dir = normalize(q);
    // The slice's plane holds the view ray and `dir`; the normal is projected into it.
    let ortho = dir - dot(dir, v) * v;
    let axis = normalize(cross(ortho, v));
    let pn = n - axis * dot(n, axis);
    let pn_len = length(pn);
    let cos_n = clamp(dot(pn, v) / max(pn_len, 1e-6), 0.0, 1.0);
    let ang_n = sign(dot(ortho, pn)) * acos(cos_n);
    // The horizons' cosines to the view ray, each side starting flat on the surface.
    let low_pos = cos(ang_n + kHalfPi);
    let low_neg = cos(ang_n - kHalfPi);
    var h_pos = low_pos;
    var h_neg = low_neg;
    for (var k = 0u; k < kSteps; k++) {
      var f = (f32(k) + jitter) / f32(kSteps);
      f = f * f;
      let step = max(f * reach_px, f32(k) + 1.0);
      let o = w * step;
      let a = vec2i(floor(pc + o));
      let b = vec2i(floor(pc - o));
      let za = depth_at(a);
      let zb = depth_at(b);
      if (za > 0.0) {
        let e = point_at(vec2f(a) + 0.5, za) - p;
        let dist = length(e);
        let fade = clamp((radius - dist) / range, 0.0, 1.0);
        h_pos = max(h_pos, mix(low_pos, dot(e, v) / max(dist, 1e-6), fade));
      }
      if (zb > 0.0) {
        let e = point_at(vec2f(b) + 0.5, zb) - p;
        let dist = length(e);
        let fade = clamp((radius - dist) / range, 0.0, 1.0);
        h_neg = max(h_neg, mix(low_neg, dot(e, v) / max(dist, 1e-6), fade));
      }
    }
    // The two horizons as angles from the view ray, held to the normal's hemisphere, and the
    // cosine-weighted arc between them.
    var h1 = acos(clamp(h_pos, -1.0, 1.0));
    var h0 = -acos(clamp(h_neg, -1.0, 1.0));
    h0 = ang_n + clamp(h0 - ang_n, -kHalfPi, kHalfPi);
    h1 = ang_n + clamp(h1 - ang_n, -kHalfPi, kHalfPi);
    let sn = sin(ang_n);
    let arc0 = (cos_n + 2.0 * h0 * sn - cos(2.0 * h0 - ang_n)) * 0.25;
    let arc1 = (cos_n + 2.0 * h1 * sn - cos(2.0 * h1 - ang_n)) * 0.25;
    seen += pn_len * (arc0 + arc1);
  }
  // The few hundredths open ground reads (the depth's own steps, the normal rebuilt off them)
  // are dropped, so a flat slope stays as lit as with the pass off.
  let shut = clamp((1.0 - seen / f32(kSlices) - kFloor) / (1.0 - kFloor), 0.0, 1.0);
  textureStore(dst, t, vec4f(shut * params.look.y, z, 0.0, 1.0));
}
