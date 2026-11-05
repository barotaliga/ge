// A BODY OF WATER AS THE SHADERS READ IT (render/water.hpp's `WaterGpuBody` and its waves), shared
// by the surface (render/water.wgsl) and the view under water (render/underwater.wgsl). The
// including shader declares `body: WaterBody` (a uniform) and `waves: array<Wave>` (read-only
// storage) before including this.
//
// - THE GUSTS AND THE STIR (`gust_at`, `stir_at`): the short waves' strength in drifting patches,
//   and a still surface's own wandering slope (render/water.hpp § The gusts).
// - THE CAUSTICS (`caustic_at`): the sun's light the surface focuses onto a point under it. A ray
//   through the surface is bent by its slope (by 1 - 1/n of it, about a quarter), so the patch of
//   surface over a point maps onto the bed with the Jacobian I + s H (H the surface height's
//   curvature, s the quarter of the way down along the sun's refracted ray); the light there is
//   1 / |det| of it: bright where the crests focus it, dim between. The sun's disc (half a degree)
//   blurs it with the depth, so a wave shorter than that blur is left out: near the surface the
//   net is fine and sharp, deep down coarse and soft; a still surface throws none.

struct WaterBody {
  info: vec4f,       // level (world m), the waves' amplitude scale, components, detail components
  box: vec4f,        // x0, z0, x1, z1 (world m)
  grid: vec4f,       // levels, sink (spacings), morph start, the rim's reach (times)
  rough: vec4f,      // the tail's slope variance, the calm surface's, refraction's strength, 0
  c: vec4f,          // beam attenuation, rgb (1/m)
  kd: vec4f,         // the daylight's diffuse attenuation, rgb (1/m)
  deep: vec4f,       // deep water's colour, pi Rrs, rgb
  emission: vec4f,   // a body's own glow, rgb (white = a white face in daylight)
  wake: vec4f,       // rings, their life (s), the shortest wave kept (m), 0
  air: vec4f,        // the day's share on the water, the stir's slope, the gusts, their drift (m/s)
  sun: vec4f,        // the share of the surface the sun reaches (the caustics, the shafts), 0, 0, 0
  view: vec4f,       // the mirror (0 none, 1 over the surface, 2 under it), the view up (1), the
                     // wind's heading (sin, cos)
  foam: vec4f,       // the fold's threshold on the Jacobian, the whitecap cover, its drift (x, z)
  surf: vec4f,       // the laid band's Hs (m), the run-up over the level (m), the folds' wavelength (m), 0
  up_view_proj: mat4x4f,  // the view up: world relative to the surface over the eye -> its clip
  levels: array<vec4f, 12>,  // origin x, z, spacing, 0
}

// A wave: kx, kz, amplitude, angular frequency; phase, k, 0, 0.
struct Wave {
  dir: vec4f,
  more: vec4f,
}

// A lattice point's value, 0..1: an integer hash, the same on every GPU.
fn lattice(c: vec3i) -> f32 {
  var h = bitcast<vec3u>(c) * vec3u(1664525u, 22695477u, 1103515245u) + 1013904223u;
  h.x += h.y * h.z;
  h.y += h.z * h.x;
  h.z += h.x * h.y;
  h ^= h >> vec3u(16u);
  h.x += h.y * h.z;
  return f32(h.x >> 8u) * (1.0 / 16777216.0);
}

// Value noise in 3D with its gradient (x: the value 0..1; y, z, w: d/dx, d/dy, d/dz), smoothed by
// the quintic so the gradient is continuous.
fn value_noise(p: vec3f) -> vec4f {
  let i = vec3i(floor(p));
  let f = fract(p);
  let u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  let du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
  let a = lattice(i);
  let b = lattice(i + vec3i(1, 0, 0));
  let c = lattice(i + vec3i(0, 1, 0));
  let d = lattice(i + vec3i(1, 1, 0));
  let e = lattice(i + vec3i(0, 0, 1));
  let g = lattice(i + vec3i(1, 0, 1));
  let h = lattice(i + vec3i(0, 1, 1));
  let k = lattice(i + vec3i(1, 1, 1));
  let k1 = b - a;
  let k2 = c - a;
  let k3 = e - a;
  let k4 = a - b - c + d;
  let k5 = a - c - e + h;
  let k6 = a - b - e + g;
  let k7 = -a + b + c - d + e - g - h + k;
  let v = a + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z +
          k6 * u.z * u.x + k7 * u.x * u.y * u.z;
  let grad = du * vec3f(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
                        k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
                        k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
  return vec4f(v, grad);
}

// THE GUSTS at a point of the plan: the short waves' strength there now, about 1 on the mean,
// from near calm in a lull to nearly twice it under a puff; drifting downwind and changing as it
// goes, a puff some metres across in lulls a few times wider.
fn gust_at(xz: vec2f, t: f32) -> f32 {
  if (body.air.z <= 0.0) {
    return 1.0;
  }
  let toward = body.view.zw;
  let q = xz - toward * body.air.w * t;
  let n = 0.65 * value_noise(vec3f(q / 9.0, t * 0.05)).x +
          0.35 * value_noise(vec3f(q / 3.5 + 17.0, t * 0.12)).x;
  let g = 0.15 + 1.85 * smoothstep(0.3, 0.72, n);
  return mix(1.0, g, body.air.z);
}

// THE STIR: a still surface's own slope (x, z) at a point now, `air.y` the rms about: two sizes
// of noise in the plan wandering in time, never repeating.
fn stir_at(xz: vec2f, t: f32) -> vec2f {
  if (body.air.y <= 0.0) {
    return vec2f(0.0);
  }
  let a = value_noise(vec3f(xz / 0.9, t * 0.21));
  let b = value_noise(vec3f(xz / 0.31 + 5.3, t * 0.37));
  return body.air.y * 1.6 * (a.yz + 0.5 * b.yz);
}

// THE CAUSTICS at a point `depth` m under the surface whose plan place is `xz`, the sun's light
// coming `down` (unit, its way through the water): the focused light over the mean (1 on average;
// up to 5 at a focus, down to a fifth between). `soft` widens the blur (1: the sun's disc alone; the
// shafts, lit through the water's volume, read a softer net), and `foot` (m, the pixel's footprint
// where it is seen) floors it: a net finer than the pixel fades to its mean rather than sparkle.
fn caustic_at(xz: vec2f, depth: f32, down: vec3f, t: f32, soft: f32, foot: f32) -> f32 {
  if (depth <= 0.0) {
    return 1.0;
  }
  let dy = min(down.y, -0.2);
  let along = depth / -dy;
  let s_xz = xz + down.xz / dy * depth;  // where the sun's ray through it crossed the surface
  let s = 0.25 * along;
  // The sun's disc seen through the surface (0.53 degrees, three quarters of it under water)
  // spread over the way down, and a few millimetres the eye never resolves on a bed.
  let blur = max(soft * (0.007 * along + 0.003), foot);
  let gust = gust_at(s_xz, t);
  let laid = u32(body.info.z);
  let total = u32(body.info.z + body.info.w);
  var h = vec3f(0.0);  // the curvature: d2/dx2, d2/dxdz, d2/dz2
  for (var i = 0u; i < total; i++) {
    let w = waves[i];
    let k = w.more.y;
    let keep = smoothstep(2.0 * blur, 4.0 * blur, 6.2831853 / k);
    if (keep <= 0.0) {
      continue;
    }
    let a = w.dir.z * body.info.y * select(1.0, gust, i >= laid);
    let phi = dot(w.dir.xy, s_xz) - w.dir.w * t + w.more.x;
    let g = keep * (-a * sin(phi) + 2.0 * k * a * a * cos(2.0 * phi));
    h += g * vec3f(w.dir.x * w.dir.x, w.dir.x * w.dir.y, w.dir.y * w.dir.y);
  }
  let det = (1.0 + s * h.x) * (1.0 + s * h.z) - s * s * h.y * h.y;
  return clamp(1.0 / max(abs(det), 0.2), 0.2, 5.0);
}

// The sun's way down through a level surface (unit, y < 0): its direction bent by Snell's law.
fn sun_down() -> vec3f {
  let l = normalize(vec3f(frame.sun.x, max(frame.sun.y, 0.05), frame.sun.z));
  return refract(-l, vec3f(0.0, 1.0, 0.0), 1.0 / 1.333);
}

// The share of a level face's light under the open sky that is the sun's own (the rest the sky's):
// the share the caustics move about.
fn sun_share(world_y: f32, xz: vec2f) -> f32 {
  let sun = dot(sun_light(world_y, xz) * max(frame.sun.y, 0.0), kLuma);
  let sky = dot(sky_irradiance(vec3f(0.0, 1.0, 0.0)), kLuma);
  return sun / max(sun + sky, 1e-6);
}
