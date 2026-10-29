// THE LENS PASS (render/lens.hpp): one texel of the quarter-size target a thread. The sun's rays
// through what stands (a march toward the sun's place over the open sky round it) and the flare
// (ghosts on the line through the middle, a ring round the sun), both in pre-exposure light so
// the post pass adds them before its exposure.
#include "render/wgsl/frame.wgsl"

override kRays: bool = false;
override kFlare: bool = false;

@group(1) @binding(0) var src: texture_2d<f32>;  // the post pass's input, at the output's size
@group(1) @binding(1) var lin: sampler;
@group(1) @binding(2) var depth: texture_depth_2d;  // reversed-z: 0 where nothing was drawn
@group(1) @binding(3) var dst: texture_storage_2d<rgba16float, write>;

struct Params {
  strength: vec4f,  // the rays', the flare's
}
@group(1) @binding(4) var<uniform> params: Params;

const kRaySteps = 32;
const kRayReach = 0.9;    // the share of the way to the sun a march covers
const kRayDecay = 0.97;   // each step's weight over the last's: far gaps light less
const kRayRadius = 0.25;  // how far round the sun the open sky lights the rays, screen heights
const kRayGain = 0.8;     // the light next to an open sun at full strength, in exposed white
const kFlareGain = 0.5;

// 1 where the depth target holds nothing (the sky, and the cloud drawn behind depth).
fn sky_at(uv: vec2f) -> f32 {
  let size = vec2i(textureDimensions(depth));
  let p = clamp(vec2i(uv * vec2f(size)), vec2i(0), size - 1);
  return select(0.0, 1.0, textureLoad(depth, p, 0) <= 0.0);
}

// Interleaved gradient noise, 0..1: the march's start, fixed per texel.
fn ign(p: vec2f) -> f32 {
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
}

// THE RAYS at `uv`, exposed light before the key's tint: the march toward the sun sums the open
// sky it crosses near the sun (falling as the fourth power toward kRayRadius), weighed nearer
// first, and dimmed where that sky is dark (a cloud's shaded base hides the sun as a trunk does).
// Only the steps inside kRayRadius can add anything, so the march starts at the first of them
// (its weight the decay to that step, the total in closed form): a texel far from the sun costs
// a few instructions, not kRaySteps taps.
fn rays(uv: vec2f, sun: vec2f, aspect: f32, e: f32, jitter: f32) -> f32 {
  let n = f32(kRaySteps);
  let far = length((uv - sun) * vec2f(aspect, 1.0));
  // Step i stands (1 - (i + jitter) kRayReach / n) of `far` from the sun.
  let first = max(ceil((1.0 - kRayRadius / max(far, 1e-6)) * n / kRayReach - jitter), 0.0);
  let total = (1.0 - pow(kRayDecay, n)) / (1.0 - kRayDecay);
  if (first >= n) {
    return 0.0;
  }
  let d = (sun - uv) * (kRayReach / n);
  var p = uv + d * (first + jitter);
  var w = pow(kRayDecay, first);
  var sum = 0.0;
  for (var i = i32(first); i < kRaySteps; i++) {
    let off = (p - sun) * vec2f(aspect, 1.0);
    let near = max(1.0 - dot(off, off) / (kRayRadius * kRayRadius), 0.0);
    if (near > 0.0) {
      let bright = dot(textureSampleLevel(src, lin, p, 0.0).rgb, kLuma) * e;
      let n2 = near * near;
      sum += sky_at(p) * clamp(bright - 0.5, 0.0, 1.0) * n2 * n2 * w;
    }
    w *= kRayDecay;
    p += d;
  }
  return sum / total * (kRayGain * params.strength.x);
}

// A ghost's shape at `q` (screen heights from its centre) of radius `r`: a soft disc, or a soft
// hexagon (the aperture's blades) when `hex`.
fn ghost(q: vec2f, r: f32, hex: bool) -> f32 {
  var d = length(q);
  if (hex) {
    let a = abs(q);
    d = max(a.x * 0.866 + a.y * 0.5, a.y);
  }
  let inside = clamp((r - d) / (0.3 * r), 0.0, 1.0);
  return inside * inside * (0.6 + 0.4 * d / r);  // a little brighter toward its rim
}

// THE FLARE at `uv`, exposed light: `vis` is the share of the sun that is open sky.
fn flare(uv: vec2f, sun: vec2f, aspect: f32, vis: f32) -> vec3f {
  let axis = vec2f(0.5) - sun;
  // Each ghost: where on the line (0 the sun, 1 the middle, 2 the mirror point), its radius in
  // screen heights, its weight, its coating's tint and whether it is a hexagon.
  let at = array<f32, 6>(-0.25, 0.45, 0.75, 1.3, 1.65, 2.1);
  let size = array<f32, 6>(0.05, 0.035, 0.075, 0.12, 0.06, 0.18);
  let weight = array<f32, 6>(0.35, 0.5, 0.3, 0.2, 0.4, 0.12);
  let tint = array<vec3f, 6>(vec3f(1.0, 0.7, 0.4), vec3f(0.5, 1.0, 0.6), vec3f(0.4, 0.6, 1.0),
                             vec3f(0.7, 0.5, 1.0), vec3f(1.0, 0.8, 0.5), vec3f(0.4, 0.8, 0.9));
  var sum = vec3f(0.0);
  for (var k = 0; k < 6; k++) {
    let q = (uv - (sun + axis * at[k])) * vec2f(aspect, 1.0);
    sum += tint[k] * (weight[k] * ghost(q, size[k], k % 2 == 1));
  }
  // The ring: a faint band round the sun, red outside and blue inside.
  let r = length((uv - sun) * vec2f(aspect, 1.0));
  let band = (r - 0.3) / 0.025;
  sum += vec3f(0.08 + 0.04 * band, 0.07, 0.08 - 0.04 * band) * exp(-band * band);
  return sum * vis;
}

// The share of the sun that is open sky: 13 depth taps round its place. The same for every texel,
// so one thread of a workgroup takes it and shares it.
var<workgroup> sun_open: f32;

fn open_share(sun: vec2f, aspect: f32) -> f32 {
  var vis = sky_at(sun);
  for (var k = 0; k < 12; k++) {
    let a = f32(k) * (PI / 6.0);
    vis += sky_at(sun + vec2f(cos(a) / aspect, sin(a)) * 0.012);
  }
  return vis / 13.0;
}

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3u, @builtin(local_invocation_index) lane: u32) {
  // The key light's place on the screen; nothing when it is behind the eye or under the horizon.
  let c = frame.view_proj * vec4f(frame.sun.xyz, 0.0);
  let up = c.w > 1e-4 && frame.sun.y > 0.0;
  let sun = vec2f(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5);
  let dims = vec2f(textureDimensions(src));
  let aspect = dims.x / dims.y;
  if (kFlare) {
    if (lane == 0u) {
      sun_open = select(0.0, open_share(sun, aspect), up);
    }
    workgroupBarrier();
  }
  let size = textureDimensions(dst);
  if (id.x >= size.x || id.y >= size.y) {
    return;
  }
  var out = vec3f(0.0);
  if (up) {
    let uv = (vec2f(id.xy) + 0.5) / vec2f(size);
    // Fades out over a third of the screen past its edge.
    let past = max(abs(sun - 0.5) - 0.5, vec2f(0.0));
    let onscreen = clamp(1.0 - length(past) * 3.0, 0.0, 1.0);
    let e = exposure();
    if (onscreen > 0.0) {
      // The sun's pass through the cloud: an overcast sky has no sun to stream or flare.
      let light = onscreen * frame.weather.x;
      let key = key_at_eye();
      let tint = key / max(dot(key, kLuma), 1e-6);  // the key's colour: warm when it is low
      if (kRays) {
        out += tint * (rays(uv, sun, aspect, e, ign(vec2f(id.xy))) * light);
      }
      if (kFlare) {
        out += tint * flare(uv, sun, aspect, sun_open) * (kFlareGain * params.strength.y * light);
      }
      out /= e;
    }
  }
  textureStore(dst, id.xy, vec4f(out, 1.0));
}
