// BEVELLED EDGES IN THE PICTURE (render/bevels.hpp's "bevels"): each pixel's surface against the
// surfaces near it along the screen's four ways; at a convex edge within the radius, the normal
// bent toward the other face's and the light's change multiplied onto the colour.
#include "render/wgsl/frame_types.wgsl"

struct BevelSettings {
  // the bevel's width on each face, m; the change's strength; the cosine an edge must turn past
  radius_m: f32,
  strength: f32,
  min_cos: f32,
  pad: f32,
}

@group(0) @binding(0) var depth: texture_depth_2d;
@group(0) @binding(1) var<uniform> frame: Frame;
@group(0) @binding(2) var<uniform> bevel: BevelSettings;

struct VertexOut {
  @builtin(position) position: vec4f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

// The camera-relative place of pixel `px` at depth `z`.
fn place_at(px: vec2f, z: f32) -> vec3f {
  let uv = px * frame.size.zw;
  let w = frame.inv_view_proj * vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, z, 1.0);
  return w.xyz / w.w;
}

fn depth_at(px: vec2i) -> f32 {
  let dims = vec2i(textureDimensions(depth));
  return textureLoad(depth, clamp(px, vec2i(0), dims - 1), 0);
}

// The place of pixel `px` (0 depth: nothing drawn there).
fn surface_at(px: vec2i) -> vec4f {
  let z = depth_at(px);
  return vec4f(place_at(vec2f(px) + 0.5, z), z);
}

// The shorter of two steps to a neighbour, never one of none (a pixel at the picture's edge).
fn nearer(a: vec3f, b: vec3f) -> vec3f {
  let la = dot(a, a);
  let lb = dot(b, b);
  if (la < 1e-12) {
    return b;
  }
  if (lb < 1e-12) {
    return a;
  }
  return select(b, a, la < lb);
}

// The plane's normal at pixel `px` (toward the eye) from its nearer neighbours on each axis, so a
// neighbour across an edge is not taken.
fn normal_at(px: vec2i, p: vec3f) -> vec3f {
  let r = surface_at(px + vec2i(1, 0)).xyz;
  let l = surface_at(px - vec2i(1, 0)).xyz;
  let d = surface_at(px + vec2i(0, 1)).xyz;
  let u = surface_at(px - vec2i(0, 1)).xyz;
  let ax = nearer(r - p, p - l);
  let ay = nearer(d - p, p - u);
  var n = normalize(cross(ay, ax));
  if (dot(n, p) > 0.0) {
    n = -n;
  }
  return n;
}

// A light from above and the eye: what a highlight on a rounded edge is mostly of.
fn key_light(n: vec3f, v: vec3f) -> f32 {
  let l = normalize(vec3f(0.0, 1.0, 0.0) + v);
  return 0.55 + 0.45 * max(dot(n, l), 0.0);
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let px = vec2i(in.position.xy);
  let here = surface_at(px);
  // The picture's border has no neighbour on one side to find its plane by.
  if (here.w <= 0.0 || any(px < vec2i(1)) || any(px >= vec2i(textureDimensions(depth)) - 1)) {
    return vec4f(1.0);
  }
  let p = here.xyz;
  let dist = length(p);
  let n0 = normal_at(px, p);
  // How long a pixel is at this distance (screen-parallel), and the radius in pixels.
  let up = place_at(vec2f(px) + vec2f(0.5, 1.5), here.w);
  let pixel_m = max(length(up - p), 1e-6);
  let reach = bevel.radius_m / pixel_m;
  if (reach < 1.0) {
    return vec4f(1.0);  // under a pixel: nothing to see
  }
  let k = min(reach, 24.0);
  // A hair: what a flat face's own depth wobbles by over the radius.
  let hair = 0.15 * bevel.radius_m + 0.002 * dist;
  var bend = vec3f(0.0);
  var found = 0.0;
  var ways = array<vec2f, 4>(vec2f(1.0, 0.0), vec2f(-1.0, 0.0), vec2f(0.0, 1.0), vec2f(0.0, -1.0));
  for (var w = 0; w < 4; w++) {
    for (var s = 1; s <= 3; s++) {
      let f = f32(s) / 3.0;
      let q = px + vec2i(round(ways[w] * k * f));
      let dims = vec2i(textureDimensions(depth));
      if (any(q < vec2i(1)) || any(q >= dims - 1)) {
        break;  // off the picture: nothing known there
      }
      let there = surface_at(q);
      if (there.w <= 0.0) {
        break;
      }
      // Behind this pixel's plane: the face beyond a convex edge.
      if (dot(there.xyz - p, n0) < -hair) {
        let n1 = normal_at(q, there.xyz);
        if (dot(n1, n0) < bevel.min_cos && dot(n1, n0) > -0.2) {
          // Half way to the other face's normal at the edge, none a radius in.
          let near = 1.0 - (f - 1.0 / 3.0) * 1.5;
          bend += (n1 - n0) * 0.5 * clamp(near, 0.0, 1.0);
          found += 1.0;
        }
        break;
      }
    }
  }
  if (found == 0.0) {
    return vec4f(1.0);
  }
  let nb = normalize(n0 + bend);
  let v = -p / dist;
  let change = key_light(nb, v) / key_light(n0, v);
  // Faded out as the radius shrinks toward a pixel.
  let fade = clamp(reach - 1.0, 0.0, 1.0);
  let m = clamp(1.0 + (change - 1.0) * bevel.strength * fade, 0.6, 1.7);
  return vec4f(vec3f(m), 1.0);
}
