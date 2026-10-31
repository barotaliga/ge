// LAVA'S AND SLIME'S SHARED HALF (render/liquid.hpp): a body's uniform and bindings, the hashes
// and noise, the glow table's lookup, Fresnel and the scene's distance behind a pixel, read by the
// surfaces (liquid.wgsl) and the view from under a body (liquid_under.wgsl).

struct LiquidBody {
  info: vec4f,
  box: vec4f,
  flow: vec4f,
  lava0: vec4f,   // core K, crust_hot K, crust_cold K, cover
  lava1: vec4f,   // plate, crack, ropes, age_rate
  lava2: vec4f,   // emissivity, crust_rough, melt_rough, the melt's patches' size
  crust: vec4f,
  absorb: vec4f,
  scatter: vec4f,
  glow: vec4f,
  slime0: vec4f,  // rough, film nm, scum, bubbles
  slime1: vec4f,  // bubble size, grime, debris a m^2
  scum: vec4f,
  fx: vec4f,      // reflections on, the shimmer (rad/m), the breaks held, 0
  breaks: array<vec4f, 8>,  // x, z, radius (m), the time it fell in (s)
  table: array<vec4f, 32>,
}

struct Ripple {
  dir: vec4f,   // kx, kz, amplitude, angular frequency
  more: vec4f,  // phase, k
}

@group(1) @binding(0) var<uniform> body: LiquidBody;
@group(1) @binding(1) var<storage, read> ripples: array<Ripple>;
@group(1) @binding(2) var scene_colour: texture_2d<f32>;
@group(1) @binding(3) var scene_depth: texture_2d<f32>;

const kGlowLo = 600.0;
const kGlowHi = 1700.0;
const kSunLuxCd = 128000.0;
const kShimmerHeight = 3.0;  // m: render/liquid.hpp's

// HASHES AND NOISE.
fn hash2(p: vec2i) -> vec2f {
  var h = vec2u(bitcast<u32>(p.x), bitcast<u32>(p.y));
  h = h * vec2u(1664525u, 1013904223u) + vec2u(1013904223u, 1664525u);
  h.x += h.y * 1664525u;
  h.y += h.x * 1664525u;
  h = h ^ (h >> vec2u(16u));
  h.x += h.y * 1664525u;
  h.y += h.x * 1664525u;
  h = h ^ (h >> vec2u(16u));
  return vec2f(h) * (1.0 / 4294967296.0);
}

fn value_noise(p: vec2f) -> f32 {
  let i = vec2i(floor(p));
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  let a = hash2(i).x;
  let b = hash2(i + vec2i(1, 0)).x;
  let c = hash2(i + vec2i(0, 1)).x;
  let d = hash2(i + vec2i(1, 1)).x;
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

fn fbm(p: vec2f) -> f32 {
  var s = 0.0;
  var a = 0.5;
  var q = p;
  for (var i = 0; i < 4; i++) {
    s += a * value_noise(q);
    q = q * 2.03 + vec2f(17.1, 9.7);
    a *= 0.5;
  }
  return s / 0.9375;
}

fn glow_at(kelvin: f32) -> vec3f {
  let x = clamp((kelvin - kGlowLo) / (kGlowHi - kGlowLo), 0.0, 1.0) * 31.0;
  let i = u32(floor(x));
  let j = min(i + 1u, 31u);
  let c = mix(body.table[i].rgb, body.table[j].rgb, fract(x));
  // Under the table's first entry the glow falls on toward nothing.
  let below = clamp(kelvin / kGlowLo, 0.0, 1.0);
  return c * pow(below, 24.0) * frame.world.y / kSunLuxCd;
}

fn fresnel(cos_t: f32, f0: f32) -> f32 {
  return f0 + (1.0 - f0) * pow(1.0 - clamp(cos_t, 0.0, 1.0), 5.0);
}

// The scene behind a pixel: its camera-relative distance (huge for the sky).
fn behind_distance(pixel: vec2i, uv: vec2f) -> f32 {
  let z = textureLoad(scene_depth, pixel, 0).r;
  if (z <= 0.0) {
    return 1e6;
  }
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let p = frame.inv_view_proj * vec4f(ndc, z, 1.0);
  return length(p.xyz / p.w);
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

