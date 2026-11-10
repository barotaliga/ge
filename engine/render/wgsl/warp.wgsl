// THE WARP (render/morph.hpp's MorphWarp): a figure coming into being or going out of it as one
// share, shared by the morph pass and the view-model pass. morph.cpp's `warp_vertex` and
// `warp_field` are its CPU twins. Needs frame.wgsl's `white_luminance`.

struct Warp {
  a: vec4f,  // shown, low, high, seed
  b: vec4f,  // the sweep's axis (unit), pinch
  c: vec4f,  // the edge's own light, shimmer
  d: vec4f,  // the ash the band at the cut turns to (a tint over the surface's light)
}

const kWarpEdge = 0.08;

// The warp's move of a point `v` (its offset from the figure's position, world axes, m): pinched
// toward the upright line and stretched up from `low`, and shimmering in bands that climb.
fn warp_vertex(w: Warp, t: f32, v: vec3f) -> vec3f {
  let g = 1.0 - clamp(w.a.x, 0.0, 1.0);
  if (g <= 0.0 || (w.b.w <= 0.0 && w.c.w <= 0.0)) {
    return v;
  }
  let gg = g * g * w.b.w;
  let pinch = 1.0 - 0.8 * gg;
  let stretch = 1.0 + 0.6 * gg;
  let amp = 0.06 * g * w.c.w;
  let ph = v.y * 20.0 - t * 12.0 + w.a.w;
  return vec3f(v.x * pinch + amp * sin(ph), w.a.y + (v.y - w.a.y) * stretch,
               v.z * pinch + amp * sin(ph * 1.31 + 1.7));
}

fn warp_hash(c: vec3i) -> f32 {
  var h = (bitcast<u32>(c.x) * 0x8da6b343u) ^ (bitcast<u32>(c.y) * 0xd8163841u) ^
          (bitcast<u32>(c.z) * 0xcb1ab31fu);
  h ^= h >> 16u;
  h *= 0x7feb352du;
  h ^= h >> 15u;
  h *= 0x846ca68bu;
  h ^= h >> 16u;
  return f32(h >> 8u) / 16777216.0;
}

fn warp_noise(p: vec3f) -> f32 {
  let i = vec3i(floor(p));
  let u = p - floor(p);
  let s = u * u * (3.0 - 2.0 * u);
  var face: array<f32, 2>;
  for (var k = 0; k < 2; k++) {
    let a = mix(warp_hash(i + vec3i(0, 0, k)), warp_hash(i + vec3i(1, 0, k)), s.x);
    let b = mix(warp_hash(i + vec3i(0, 1, k)), warp_hash(i + vec3i(1, 1, k)), s.x);
    face[k] = mix(a, b, s.y);
  }
  return mix(face[0], face[1], s.z);
}

// The share at which a point appears: noise fixed to the model's frame (`local`) and the point's
// place along the sweep's axis (`along`, m): the far end first in, the near end first out.
fn warp_field(w: Warp, local: vec3f, along: f32) -> f32 {
  let up = clamp((along - w.a.y) / max(w.a.z - w.a.y, 1e-3), 0.0, 1.0);
  let n = 0.65 * warp_noise(local * 14.0 + vec3f(w.a.w, 0.0, 0.0)) +
          0.35 * warp_noise(local * 31.0 + vec3f(0.0, w.a.w, 17.0));
  return 0.6 * clamp((n - 0.2) / 0.6, 0.0, 1.0) + 0.4 * (1.0 - up);
}

// A fragment under the warp: -1 when it is not drawn, else its burn 0..1 (1 at the cut).
fn warp_burn(w: Warp, local: vec3f, along: f32) -> f32 {
  let s = clamp(w.a.x, 0.0, 1.0);
  if (s >= 1.0) {
    return 0.0;
  }
  let f = warp_field(w, local, along);
  let cut = s * (1.0 + kWarpEdge);
  if (f >= cut) {
    return -1.0;
  }
  return clamp(1.0 - (cut - f) / kWarpEdge, 0.0, 1.0);
}

// The colour `c` (its light) of a fragment with `burn`: the band at the cut turned to ash (its
// luminance times the ash tint), then the edge's own light.
fn warp_shade(w: Warp, c: vec3f, burn: f32) -> vec3f {
  let gone = 1.0 - clamp(w.a.x, 0.0, 1.0);
  let grey = dot(c, vec3f(0.2126, 0.7152, 0.0722));
  let ash = mix(c, grey * w.d.rgb, burn);
  return ash + w.c.rgb * (0.12 * gone + 2.0 * burn * burn) * white_luminance();
}
