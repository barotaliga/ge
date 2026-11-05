// THE HEAT HAZE DRAWN (render/haze.hpp): with the frame's colour so far copied (haze_copy.wgsl),
// every warm stretch of a trail a ribbon turned to the eye ("haze") whose pixels read the copy pushed off
// by the hot air's cells: two octaves of smooth 3D noise drifting up through the air, the push
// largest along the ribbon's middle, nothing at its edges, its size the stretch's bend in radians
// turned into pixels at its distance. A push that lands on something nearer than the ribbon keeps
// the pixel's own colour. Nothing is added: the light is only moved.
#include "render/wgsl/frame.wgsl"

struct HazeParams {
  info: vec4f,  // stretches, the near plane (m), the frame's time (s), 0
}

struct HazeSeg {
  a: vec4f,  // camera-relative start; the heat
  b: vec4f,  // camera-relative end; the half width, m
  c: vec4f,  // the bend (rad), the cells' size (m), how far it has risen (m), the seed (0..1)
}

@group(1) @binding(0) var<uniform> hp: HazeParams;
@group(1) @binding(1) var<storage, read> segs: array<HazeSeg>;
@group(1) @binding(2) var scene_depth: texture_depth_2d;
@group(1) @binding(3) var scene: texture_2d<f32>;

const kHazeCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                     vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));

struct HazeOut {
  @builtin(position) position: vec4f,
  @location(0) world: vec3f,   // the point in the air, world m (the cells' frame)
  @location(1) across: f32,    // -1..1 across the ribbon
  @location(2) px_rad: f32,    // pixels a radian at its distance
  @location(3) @interpolate(flat) seg: u32,
}

@vertex
fn haze_vs(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> HazeOut {
  var out: HazeOut;
  if (f32(iid) >= hp.info.x) {
    out.position = vec4f(2.0, 2.0, 2.0, 1.0);
    return out;
  }
  let s = segs[iid];
  let c = kHazeCorners[vid];
  let rise = vec3f(0.0, s.c.z, 0.0);
  let a = s.a.xyz + rise;
  let b = s.b.xyz + rise;
  let d = b - a;
  let len = max(length(d), 1e-4);
  let along = d / len;
  let end = c.x * 0.5 + 0.5;
  let base = mix(a, b, end);
  let to_eye = -normalize(base);
  var across = cross(along, to_eye);
  let al = length(across);
  across = select(across / max(al, 1e-6), normalize(cross(vec3f(0.0, 1.0, 0.0), to_eye)),
                  al < 1e-4);
  // Metres a pixel here, so the ribbon is never thinner than two pixels.
  let clip0 = frame.view_proj * vec4f(base, 1.0);
  let clip1 = frame.view_proj * vec4f(base + across, 1.0);
  let px = length((clip1.xy / clip1.w - clip0.xy / clip0.w) * frame.size.xy * 0.5);
  let m_px = 1.0 / max(px, 1e-4);
  let hw = max(s.b.w, 2.0 * m_px);
  // Round the ends a little past them, so two stretches meeting at a joint overlap.
  let x = select(len + hw * 0.5, -hw * 0.5, c.x < 0.0);
  let corner = a + along * x + across * (c.y * hw);
  out.position = frame.view_proj * vec4f(corner, 1.0);
  out.world = corner + frame.eye.xyz - rise;
  out.across = c.y;
  out.px_rad = length(base) / m_px;
  out.seg = iid;
  return out;
}

fn haze_hash(p: vec3f) -> f32 {
  let q = fract(p * vec3f(0.1031, 0.1030, 0.0973));
  let r = q + dot(q, q.yxz + 33.33);
  return fract((r.x + r.y) * r.z);
}

// Smooth value noise, -1..1.
fn haze_noise(p: vec3f) -> f32 {
  let i = floor(p);
  let f = p - i;
  let u = f * f * (3.0 - 2.0 * f);
  let n000 = haze_hash(i);
  let n100 = haze_hash(i + vec3f(1.0, 0.0, 0.0));
  let n010 = haze_hash(i + vec3f(0.0, 1.0, 0.0));
  let n110 = haze_hash(i + vec3f(1.0, 1.0, 0.0));
  let n001 = haze_hash(i + vec3f(0.0, 0.0, 1.0));
  let n101 = haze_hash(i + vec3f(1.0, 0.0, 1.0));
  let n011 = haze_hash(i + vec3f(0.0, 1.0, 1.0));
  let n111 = haze_hash(i + vec3f(1.0, 1.0, 1.0));
  let x0 = mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y);
  let x1 = mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y);
  return mix(x0, x1, u.z) * 2.0 - 1.0;
}

@fragment
fn haze_fs(in: HazeOut) -> @location(0) vec4f {
  let s = segs[in.seg];
  let pixel = vec2i(in.position.xy);
  let own = textureLoad(scene, pixel, 0);
  // Hidden where the scene is nearer than the air.
  let near = hp.info.y;
  let here = near / max(in.position.z, 1e-7);
  let behind = near / max(textureLoad(scene_depth, pixel, 0), 1e-7);
  if (behind < here) {
    return own;
  }
  // The cells: the air's own pattern, churning in place a few times a second.
  let t = hp.info.z;
  let q = in.world / max(s.c.y, 1e-3) + vec3f(s.c.w * 61.0, -t * 3.0, s.c.w * 17.0);
  let n = vec2f(haze_noise(q), haze_noise(q + vec3f(19.1, 7.3, 3.7))) * 0.7 +
          vec2f(haze_noise(q * 2.3 + vec3f(5.0)), haze_noise(q * 2.3 + vec3f(11.0))) * 0.3;
  let middle = 1.0 - in.across * in.across;
  let push = n * (s.c.x * s.a.w * middle * middle * in.px_rad);
  let uv = (in.position.xy + push) * frame.size.zw;
  if (any(uv < vec2f(0.0)) || any(uv >= vec2f(1.0))) {
    return own;
  }
  // A push onto something nearer than the air keeps the pixel as it was.
  let there = near / max(textureLoad(scene_depth, vec2i(uv * frame.size.xy), 0), 1e-7);
  if (there < here) {
    return own;
  }
  return vec4f(textureSampleLevel(scene, linear_clamp, uv, 0.0).rgb, own.a);
}
