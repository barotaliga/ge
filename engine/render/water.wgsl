// THE WATER'S SURFACE (render/water.hpp; docs/water.md § Drawn): a body of water drawn over the
// opaque scene, a grid of nested square levels round the eye at the body's level.
//
// THE GEOMETRY: level i is kGrid x kGrid quads of `spacing * 2^i`, centred on the eye snapped to
// twice its spacing, so no vertex slides over the waves as the eye moves; toward its edge a
// level's odd vertices morph onto the coarser level's grid, and a coarse vertex a finer level
// covers sinks under it (the terrain's scheme, render/terrain.hpp). The last level's rim runs
// out to the horizon. Every vertex is clamped into the body's box, and a fragment outside it is
// dropped; the shore is where the opaque ground stands over the water (the depth test).
//
// THE WAVES are the simulation's components (water/waves.hpp), summed in its formula: a linear
// wave with its second-order crest, eta = a sin(phi) - 1/2 k a^2 cos(2 phi), the height a
// function of the undisplaced point, so the drawn crest is the crest a float rides. A vertex
// keeps the components at least four of its level's spacings long (faded in up to eight). Every
// pixel sums their slopes and the DETAIL components' (the picture's own, shorter than the
// simulation keeps), each faded out as its wavelength falls under four pixels; the slope
// variance a pixel cannot resolve (the faded components' and the spectrum's tail past the
// detail band) becomes the sun's glitter's roughness, so far water glints as a broad path and
// never shimmers. The WAKES' rings (water/wake.hpp) are summed the same way, kept by the vertex
// and the pixel as a wave is.
//
// THE RAIN'S RINGS (`rain_rings`): every drop of 2 mm and more that lands sets off a ring that
// runs out at about the slowest a ripple runs (0.25 m/s) and dies in about a second, its
// crests lengthening as the longer waves run ahead (a smaller drop's ring is a few centimetres
// and gone in a blink, too small to draw). The drops are the frame's (frame.mist.z, a
// m^2 a second, under the body's open sky); they are dealt to cells in ten offset layers, each
// cell at most one ring at a time, so past what the cells can hold (about 30 a m^2 a second) the
// near water is all rings. A ring's crests too short for the pixel go to the glitter's
// roughness, as a wave's do, so a lake far off in the rain is dulled rather than ringed.
//
// THE GUSTS AND THE STIR: the short waves (the detail band, its tail) raised and laid by a field of
// gusts that drifts downwind and changes as it goes (value noise in the plan and in time, two
// sizes), so patches of ripples cross calmer water and nothing repeats; and a still surface's
// stir, a faint slope off a noise that wanders in time.
//
// THE MIRROR: a body the game drew a planar mirror for (render/water.hpp's WaterViews) reads each
// pixel's reflected ray where its mirror image lands on the mirror's picture (the reflected leg's
// length off the mirror's own depth), the sky's model where the mirror shows the sky.
//
// FROM BELOW (`fs_below`, "water.below"): the same surface seen from under it: the view refracted
// out through the surface within Snell's window, the world above read along the bent ray (the
// game's view up, else marched in screen space, else the sky's model), total internal reflection
// past it mirroring the water below (the mirror under the surface, else marched), weighed by the
// exact Fresnel terms for light leaving water; what the reflection finds is fogged over its own
// way through the water, and the share it lacks (nothing to mirror, the water's own light) goes
// out beside it for the view under water to fill with its veil.
//
// THE LIGHT is render/wgsl/water_light.wgsl's `shade_water`, the moving water's too.
#include "render/wgsl/frame.wgsl"

override kGrid: u32 = 64u;

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
  view: vec4f,       // the mirror (0 none, 1 over the surface, 2 under it), the view up (1), the
                     // wind's heading (sin, cos)
  up_view_proj: mat4x4f,  // the view up: world relative to the surface over the eye -> its clip
  levels: array<vec4f, 12>,  // origin x, z, spacing, 0
}

// A wake's ring (water/wake.hpp): x, z, the moment it was set off, its volume; its source's
// size, its own shortest wave, a body's crater (m across, 0: none) and its depth; its lean (the
// water pushed forward: x, z) and the crater drawn out along it (m), 0.
struct Ring {
  at: vec4f,
  more: vec4f,
  lean: vec4f,
}

// A wave: kx, kz, amplitude, angular frequency; phase, k, 0, 0.
struct Wave {
  dir: vec4f,
  more: vec4f,
}

@group(1) @binding(0) var<uniform> body: WaterBody;
@group(1) @binding(1) var<storage, read> waves: array<Wave>;  // the simulation's, then the detail
@group(1) @binding(2) var scene_colour: texture_2d<f32>;
@group(1) @binding(3) var scene_depth: texture_2d<f32>;      // R32F: the opaque depth
@group(1) @binding(4) var<storage, read> rings: array<Ring>;  // the body's wake, newest first
@group(1) @binding(5) var shore: texture_2d<f32>;  // R8: where on its box it may be drawn
@group(1) @binding(6) var mirror: texture_2d<f32>;  // the planar mirror (a 0: sky), or a blank
@group(1) @binding(7) var mirror_depth: texture_depth_2d;
@group(1) @binding(8) var up_view: texture_2d<f32>;  // the view straight up (a 0: sky), or a blank

#include "render/wgsl/water_light.wgsl"

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,  // camera-relative world position, m
  @location(1) xz: vec2f,   // the undisplaced world plan position, m
}

const kMorphEnd = 0.95;

// The height of the components at least `shortest` m long (faded in to twice that) at `xz`.
fn height_at(xz: vec2f, t: f32, shortest: f32) -> f32 {
  var h = 0.0;
  let n = u32(body.info.z);
  for (var i = 0u; i < n; i++) {
    let w = waves[i];
    let k = w.more.y;
    let keep = smoothstep(shortest, 2.0 * shortest, 6.2831853 / k);
    if (keep <= 0.0) {
      continue;
    }
    let a = w.dir.z * body.info.y;
    let phi = dot(w.dir.xy, xz) - w.dir.w * t + w.more.x;
    h += keep * (a * sin(phi) - 0.5 * k * a * a * cos(2.0 * phi));
  }
  return h;
}

const kGravity = 9.80665;

// Where `e` (m from a ring's point) lies against its lean: along it from the drawn-out crater's
// centre and across it (water/wake.cpp's `frame_of`).
fn lean_frame(r: Ring, e: vec2f) -> vec2f {
  let share = length(r.lean.xy);
  if (share <= 0.0) {
    return vec2f(length(e), 0.0);
  }
  let u = r.lean.xy / share;
  return vec2f(dot(e, u) - 0.5 * r.lean.z, e.y * u.x - e.x * u.y);
}

// A ring's crater and the jet after it at `e` (water/wake.cpp's `near_field`): the height.
fn crater_at(r: Ring, e: vec2f, tau: f32) -> f32 {
  let crater = r.more.z;
  let open = 2.0 * sqrt(crater / kGravity);
  let height = 0.6 * r.more.w;
  let jet = 2.0 * sqrt(2.0 * height / kGravity);
  let f = lean_frame(r, e);
  if (tau < open) {
    let q = vec2f(f.x / (crater + 0.5 * r.lean.z), f.y / crater);
    let rho2 = dot(q, q);
    let share = length(r.lean.xy);
    let ahead = 1.0 + share * q.x / sqrt(rho2 + 0.0625);
    return -r.more.w * sin(PI * tau / open) * (1.0 - rho2) * exp(-rho2) * ahead;
  }
  if (tau < open + jet) {
    let wide = crater / 3.0;
    return height * sin(PI * (tau - open) / jet) * exp(-dot(f, f) / (wide * wide));
  }
  return 0.0;
}

// THE WAKES: every ring's far field (water/wake.cpp's `wake_at`: its source's spectrum, its
// front faded in), its waves shorter than
// `shortest` m faded out (and their slopes' variance handed back in `lost`); the height and its
// slope.
fn wake_at(xz: vec2f, t: f32, shortest: f32) -> vec4f {
  var out = vec4f(0.0);  // height, d/dx, d/dz, the slope variance dropped
  let n = u32(body.wake.x);
  let life = body.wake.y;
  for (var i = 0u; i < n; i++) {
    let r = rings[i];
    let tau = t - r.at.z;
    if (tau <= 0.0) {
      continue;
    }
    var e = xz - r.at.xy;
    if (r.more.z > 0.0) {
      // The crater's slope by central differences a thousandth of it apart.
      let s = 1e-3 * r.more.z;
      out.x += crater_at(r, e, tau);
      out.y += (crater_at(r, e + vec2f(s, 0.0), tau) - crater_at(r, e - vec2f(s, 0.0), tau)) /
               (2.0 * s);
      out.z += (crater_at(r, e + vec2f(0.0, s), tau) - crater_at(r, e - vec2f(0.0, s), tau)) /
               (2.0 * s);
    }
    let share = length(r.lean.xy);
    if (share > 0.0) {
      e -= 0.5 * r.lean.z * r.lean.xy / share;  // set off from the drawn-out crater's centre
    }
    let rs = sqrt(dot(e, e) + r.more.x * r.more.x);
    let phase = kGravity * tau * tau / (4.0 * rs);
    let k = phase / rs;
    let lambda = 6.2831853 / k;
    let lowest = max(body.wake.z, r.more.y);
    let ks = k * r.more.x;
    let a = r.at.w * k / (1.4142136 * PI * rs) * exp(-0.25 * ks * ks) *
            smoothstep(lowest, 2.0 * lowest, lambda) * smoothstep(0.25 * PI, 1.25 * PI, phase) *
            (1.0 - smoothstep(life * 0.6666667, life, tau));
    let keep = smoothstep(shortest, 2.0 * shortest, lambda);
    out.w += (1.0 - keep) * 0.5 * (k * a) * (k * a);
    if (keep <= 0.0) {
      continue;
    }
    // The phase taken round in f32 from where it is still exact enough.
    let p = phase - 6.2831853 * floor(phase / 6.2831853);
    // Tipped forward by 1 + lean . e / rs (1 without a lean), its own slope carried by the height.
    let d = dot(e, r.lean.xy);
    let m = 1.0 + d / rs;
    let dm = r.lean.xy / rs - d * e / (rs * rs * rs);
    let h = keep * a * cos(p);
    out.x += h * m;
    out.y += keep * a * k * sin(p) * e.x / rs * m + h * dm.x;
    out.z += keep * a * k * sin(p) * e.y / rs * m + h * dm.y;
  }
  return out;
}

const kRainLayers = 10;
const kRainCell = 0.6;    // m: a layer's cells, at most one ring alive in each
const kRainLife = 0.9;    // s: a ring's life
const kRainSpeed = 0.25;  // m/s: its front's, about the slowest ripple's
const kRainSlope = 0.4;   // a 2 mm drop's ring's steepest slope, new

// Four numbers from 0 up to 1 off a cell, an epoch and a layer: an integer hash, the same on
// every GPU.
fn rain_hash(c: vec2i, epoch: i32, layer: i32) -> vec4f {
  var h = vec4u(bitcast<u32>(c.x), bitcast<u32>(c.y), bitcast<u32>(epoch), u32(layer) + 1u);
  h = h * 1664525u + 1013904223u;
  h.x += h.y * h.w;
  h.y += h.z * h.x;
  h.z += h.x * h.y;
  h.w += h.y * h.z;
  h = h ^ (h >> vec4u(16u));
  h.x += h.y * h.w;
  h.y += h.z * h.x;
  h.z += h.x * h.y;
  h.w += h.y * h.z;
  return vec4f(h >> vec4u(8u)) * (1.0 / 16777216.0);
}

// The rings' slope at `xz` (x, z) and the slope variance too fine for a pixel `foot` m across.
fn rain_rings(xz: vec2f, t: f32, foot: f32) -> vec3f {
  let drops = frame.mist.z * body.rough.w;
  if (drops <= 0.0) {
    return vec3f(0.0);
  }
  let size = clamp(frame.mist.w / 2.0, 0.4, 1.6);
  let s0 = kRainSlope * size;
  // The share of cells that hold a ring each life: the drops over what the layers can hold.
  let hold = f32(kRainLayers) / (kRainCell * kRainCell * kRainLife);
  let p = 1.0 - exp(-drops / hold);
  // A ring's crests cover about a tenth of its cell over its life; what the cells cannot hold
  // roughens the water whatever the pixel.
  // The variance the pixel cannot resolve is the rings' expected one (a fifth of the new slope's
  // square over a life), not the rings under this pixel, so far water dulls evenly.
  let cover = 0.1;
  let unresolved = 1.0 - smoothstep(2.0 * foot, 4.0 * foot, 0.08);
  let mean = 0.5 * 0.21 * s0 * s0 * cover;
  var out = vec3f(0.0, 0.0,
                  min(0.05, mean * (f32(kRainLayers) * p * unresolved + max(0.0, drops / hold - p))));
  // Past twice a pixel the longest crest a ring has, every ring is the roughness alone.
  if (2.0 * foot >= 0.13) {
    return out;
  }
  for (var l = 0; l < kRainLayers; l++) {
    let shift = fract(vec2f(0.7548777, 0.5698403) * f32(l)) * kRainCell;
    let g = (xz + shift) / kRainCell;
    let base = vec2i(floor(g - 0.5));
    for (var j = 0; j < 4; j++) {
      let c = base + vec2i(j & 1, j >> 1);
      let lead = rain_hash(c, 0, l).x;
      let clock = t / kRainLife + lead;
      let epoch = i32(floor(clock));
      let age = clock - f32(epoch);
      let h = rain_hash(c, epoch, l);
      if (h.z >= p) {
        continue;
      }
      // Within the middle fifth of its cell, so the four nearest cells hold every ring in reach.
      let at = (vec2f(c) + 0.4 + 0.2 * h.xy) * kRainCell - shift;
      let e = xz - at;
      let r = length(e);
      let front = kRainSpeed * kRainLife * age;
      let lambda = 0.03 + 0.1 * age;  // 0.13 at its end
      let k = 6.2831853 / lambda;
      let x = (r - front) / (0.6 * lambda);
      let fade = (1.0 - age) * (1.0 - age);
      let a = s0 * fade * (0.6 + 0.8 * h.w);
      let keep = smoothstep(2.0 * foot, 4.0 * foot, lambda);
      if (keep <= 0.0 || abs(x) > 2.0) {
        continue;
      }
      let w = keep * a * exp(-x * x) * sin(k * (r - front));
      out.x += w * e.x / max(r, 1e-4);
      out.y += w * e.y / max(r, 1e-4);
    }
  }
  return out;
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

// THE SURFACE'S SLOPE at a pixel's point: the waves its footprint `foot` m resolves (the detail
// band's raised and laid by the gusts), the wakes, the rain's rings and the stir; x, y the slope
// (d/dx, d/dz), z the slope variance the pixel cannot resolve.
fn surface_slope(xz: vec2f, t: f32, foot: f32) -> vec3f {
  var slope = vec2f(0.0);
  let gust = gust_at(xz, t);
  var lost = body.rough.x * gust * gust + body.rough.y;
  let laid = u32(body.info.z);
  let total = u32(body.info.z + body.info.w);
  for (var i = 0u; i < total; i++) {
    let w = waves[i];
    let k = w.more.y;
    let a = w.dir.z * body.info.y * select(1.0, gust, i >= laid);
    let keep = smoothstep(2.0 * foot, 4.0 * foot, 6.2831853 / k);
    lost += (1.0 - keep) * 0.5 * (k * a) * (k * a);
    if (keep <= 0.0) {
      continue;
    }
    let phi = dot(w.dir.xy, xz) - w.dir.w * t + w.more.x;
    slope += keep * (a * cos(phi) + k * a * a * sin(2.0 * phi)) * w.dir.xy;
  }
  let wake = wake_at(xz, t, 2.0 * foot);
  slope += wake.yz;
  lost += wake.w;
  let rain = rain_rings(xz, t, foot);
  slope += rain.xy;
  lost += rain.z;
  // The stir's ripples are a few tens of centimetres long: a far pixel takes them as roughness.
  let keep = smoothstep(2.0 * foot, 4.0 * foot, 0.3);
  slope += keep * stir_at(xz, t);
  lost += (1.0 - keep) * body.air.y * body.air.y;
  return vec3f(slope, lost);
}

// Where a camera-relative point lands on the screen (uv), and whether it is in front (z: 1).
fn screen_at(p: vec3f) -> vec3f {
  let c = frame.view_proj * vec4f(p, 1.0);
  if (c.w <= 1e-4) {
    return vec3f(0.0);
  }
  return vec3f(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5, 1.0);
}

// The planar mirror along the ray `r` from the surface at `rel` (`dist` m from the eye): where its
// mirror image lands (the reflected leg's length read off the mirror's depth at the pixel's own
// uv), its colour, and in a: 1 where the mirror shows the map, 0 where it shows a sky face, -1
// where the image falls off the mirror's picture or on nothing drawn (the caller's room, never
// the sky); `leg` the reflected leg's length, m.
fn mirror_along(rel: vec3f, r: vec3f, uv: vec2f, dist: f32, leg: ptr<function, f32>) -> vec4f {
  let size = vec2f(textureDimensions(mirror_depth));
  let at = vec2i(clamp(uv * size, vec2f(0.0), size - 1.0));
  let z = textureLoad(mirror_depth, at, 0);
  var far = 60.0;
  if (z > 0.0) {
    let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    let p = frame.inv_view_proj * vec4f(ndc, z, 1.0);
    far = length(p.xyz / p.w);
  }
  *leg = clamp(far - dist, 0.05, 200.0);
  // The ray's mirror image: its height turned over about the level.
  let s = screen_at(rel + vec3f(r.x, -r.y, r.z) * *leg);
  if (s.z < 0.5 || any(s.xy < vec2f(0.0)) || any(s.xy > vec2f(1.0))) {
    return vec4f(0.0, 0.0, 0.0, -1.0);
  }
  let there = vec2i(clamp(s.xy * size, vec2f(0.0), size - 1.0));
  if (textureLoad(mirror_depth, there, 0) <= 0.0) {
    return vec4f(0.0, 0.0, 0.0, -1.0);
  }
  return textureSampleLevel(mirror, linear_clamp, s.xy, 0.0);
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) level: u32) -> VertexOut {
  let n = kGrid + 1u;
  let g = vec2f(f32(vid % n), f32(vid / n));
  let lv = body.levels[level];
  let s = lv.z;
  let at = lv.xy + g * s;
  let half = f32(kGrid) * 0.5 * s;
  let d = max(abs(at.x - frame.eye.x), abs(at.y - frame.eye.z)) / half;
  let last = level + 1u >= u32(body.grid.x);
  var m = clamp((d - body.grid.z) / (kMorphEnd - body.grid.z), 0.0, 1.0);
  if (last) {
    m = 0.0;
  }
  var xz = lv.xy + (g - fract(g * 0.5) * 2.0 * m) * s;
  let edge = g.x == 0.0 || g.y == 0.0 || g.x == f32(kGrid) || g.y == f32(kGrid);
  if (last && edge) {
    xz = frame.eye.xz + (xz - frame.eye.xz) * body.grid.w;
  }
  xz = clamp(xz, body.box.xy, body.box.zw);
  var y = body.info.x + height_at(xz, frame.time.x, 4.0 * s);
  y += wake_at(xz, frame.time.x, 4.0 * s).x;
  if (level > 0u) {
    let f = body.levels[level - 1u];
    let lo = f.xy + vec2f(0.25 * s);
    let hi = f.xy + vec2f(f32(kGrid) * f.z) - vec2f(0.25 * s);
    if (all(xz > lo) && all(xz < hi)) {
      y -= body.grid.y * s;
    }
  }
  var out: VertexOut;
  out.rel = vec3f(xz.x, y, xz.y) - frame.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.xz = xz;
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  // The pixel's footprint on the water, m (before anything can discard).
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  if (any(in.xz < body.box.xy) || any(in.xz > body.box.zw)) {
    discard;
  }
  // Inside the shore's mask (water.hpp's `shore_mask`): a lake's box beyond its shore is dry.
  let cells = vec2f(textureDimensions(shore));
  let on_box = (in.xz - body.box.xy) / max(body.box.zw - body.box.xy, vec2f(1e-3));
  if (textureLoad(shore, vec2i(clamp(on_box * cells, vec2f(0.0), cells - 1.0)), 0).r < 0.5) {
    discard;
  }
  let t = frame.time.x;
  let sl = surface_slope(in.xz, t, foot);
  let lost = sl.z;
  let n = normalize(vec3f(-sl.x, 1.0, -sl.y));
  let dist = length(in.rel);
  let uv = in.position.xy * frame.size.zw;
  // The planar mirror where the game drew one: the reflected ray read where its image lands, the
  // sky's model where the mirror shows the sky, and the sun's glitter only there.
  var seen = vec4f(0.0);
  var sun_seen = 1.0;
  if (body.view.x > 0.5 && body.view.x < 1.5) {
    let v = -in.rel / dist;
    let r0 = reflect(-v, n);
    let r = normalize(vec3f(r0.x, max(r0.y, 0.01), r0.z));
    var leg = 0.0;
    let m = mirror_along(in.rel, r, uv, dist, &leg);
    if (m.a >= 0.0) {
      seen = vec4f(mix(sky_luminance(r), m.rgb, m.a), 1.0);
      sun_seen = 1.0 - m.a;
    } else {
      // Off the mirror's picture: the room's walls, lit as the room is; no sun.
      seen = vec4f(vec3f(white_luminance() * body.emission.w * kRoomAlbedo), 1.0);
      sun_seen = 0.0;
    }
  }
  // A body under a roof (a cave's pool) sees the sky through `open` of its dome (rough.w) and the
  // room's own light, a share of a white face's (emission.w), over the rest; the day lights the
  // water itself by its own share (air.x).
  let c = shade_water_lit(n, in.rel, in.xz, in.position.xyz, body.info.x, lost, body.rough.z,
                          Optics(body.c.rgb, body.kd.rgb, body.deep.rgb, body.emission.rgb), 0.0,
                          body.rough.w, body.emission.w, body.air.x, seen, sun_seen);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, uv, dist), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}

// The view up's reach: the tangent, sine and cosine of 72 degrees off straight up, inside the 75
// degrees its square reaches along its sides (brush/app/bsp_views.hpp's kUpFov).
const kUpRim = 3.0776835;
const kUpRimSin = 0.9510565;
const kUpRimCos = 0.3090170;

struct BelowOut {
  @location(0) colour: vec4f,  // the surface's light, its distance from the eye (m)
  @location(1) veil: vec4f,    // the share of it the water's own light fills
}

// Light leaving water into air at `cos_i` to the normal: the exact Fresnel reflectance of the two
// polarisations, averaged; 1 past the critical angle (total internal reflection).
fn fresnel_exit(cos_i: f32) -> f32 {
  let eta = 1.333;
  let s2 = eta * eta * max(1.0 - cos_i * cos_i, 0.0);
  if (s2 >= 1.0) {
    return 1.0;
  }
  let ct = sqrt(1.0 - s2);
  let rs = (eta * cos_i - ct) / (eta * cos_i + ct);
  let rp = (eta * ct - cos_i) / (eta * ct + cos_i);
  return 0.5 * (rs * rs + rp * rp);
}

@fragment
fn fs_below(in: VertexOut) -> BelowOut {
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  if (any(in.xz < body.box.xy) || any(in.xz > body.box.zw)) {
    discard;
  }
  let cells = vec2f(textureDimensions(shore));
  let on_box = (in.xz - body.box.xy) / max(body.box.zw - body.box.xy, vec2f(1e-3));
  if (textureLoad(shore, vec2i(clamp(on_box * cells, vec2f(0.0), cells - 1.0)), 0).r < 0.5) {
    discard;
  }
  let t = frame.time.x;
  let sl = surface_slope(in.xz, t, foot);
  let n = normalize(vec3f(-sl.x, 1.0, -sl.y));  // up, out of the water
  let dist = length(in.rel);
  let d = in.rel / dist;  // the eye's ray, up toward the surface
  let uv = in.position.xy * frame.size.zw;
  let cos_i = clamp(dot(d, n), 0.0, 1.0);
  let refl = fresnel_exit(cos_i);
  let room = vec3f(white_luminance() * body.emission.w);
  let open = body.rough.w;

  // OUT THROUGH THE WINDOW: the ray bent away from the normal by Snell's law.
  var above = vec3f(0.0);
  if (refl < 1.0) {
    let tr = refract(d, -n, 1.333);
    let sky = mix(room * kRoomAlbedo, sky_luminance(normalize(vec3f(tr.x, max(tr.y, 0.01), tr.z))),
                  open);
    if (body.view.y > 0.5) {
      // The game's view up, its camera on the surface straight over the eye: the point a few
      // metres along the bent ray. A ray bent out nearer level than the view reaches is read at
      // its rim along the same bearing (the walls at the waterline are much like those just over
      // them), so the window's last ring is never a hole.
      var up_ray = tr;
      let across = length(tr.xz);
      if (across > kUpRim * max(tr.y, 0.0) && across > 1e-5) {
        up_ray = vec3f(tr.x / across * kUpRimSin, kUpRimCos, tr.z / across * kUpRimSin);
      }
      let c = body.up_view_proj *
              vec4f(in.rel + vec3f(0.0, frame.eye.y - body.info.x, 0.0) + up_ray * 6.0, 1.0);
      let st = clamp(vec2f(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5), vec2f(0.0), vec2f(1.0));
      let u = textureSampleLevel(up_view, linear_clamp, st, 0.0);
      above = mix(sky, u.rgb, u.a);
    } else {
      var hit = 0.0;
      let m = march_scene(in.rel, tr, max(0.02 * dist, 0.25), &hit);
      // Nearly level and found on no screen: the walls round the water, lit as the room is.
      let low = mix(room * kRoomAlbedo, sky, open * smoothstep(0.05, 0.3, tr.y));
      above = mix(low, m.rgb, m.a);
    }
  }

  // BACK DOWN: what the surface mirrors of the water below, fogged over its way through it.
  var below = vec3f(0.0);
  var veil = refl;
  let r = reflect(d, -n);
  if (body.view.x > 1.5) {
    var leg = 0.0;
    let m = mirror_along(in.rel, normalize(vec3f(r.x, min(r.y, -0.01), r.z)), uv, dist, &leg);
    let found = max(m.a, 0.0);
    let through = exp(-body.c.rgb * leg);
    below = m.rgb * found * through;
    veil = refl * (1.0 - found * dot(through, vec3f(0.2126, 0.7152, 0.0722)));
  } else {
    var hit = 0.0;
    let m = march_scene(in.rel, r, max(0.02 * dist, 0.25), &hit);
    if (m.a > 0.0) {
      let through = exp(-body.c.rgb * hit);
      below = m.rgb * m.a * through;
      veil = refl * (1.0 - m.a * dot(through, vec3f(0.2126, 0.7152, 0.0722)));
    }
  }
  var out: BelowOut;
  out.colour = vec4f(above * (1.0 - refl) + below * refl, dist);
  out.veil = vec4f(veil, 0.0, 0.0, 0.0);
  return out;
}
