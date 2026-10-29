// THE SNOW LIGHT MODEL (docs/snow.md § Drawn): what snow does to the light, for any material
// that draws snow (a terrain, a sheet over a yard, a roof). A material works out its own surface
// (where it is snow, how packed, how pressed, its albedo) and calls these; it keeps its own
// additions (a groomed piste's corduroy and lanes) on top.
//
// - `snow_relief`: the micro-relief as a world slope (dh/dx, dh/dz) to bend into the normal:
//   swells a few metres long on loose snow, a finer grain over everything, a fine grain on the
//   loose share alone, and a coarse broken face where the ground is bare. Every layer fades out,
//   by the pixel's own footprint, before it aliases.
// - `snow_light`: the light on the bent normal. Lambert and the sky's light (dimmed by the
//   caller's cavity, a deep trough seeing less sky); the WRAP, the soft terminator light that has
//   passed through a few centimetres of ice, tinted its blue; the GLARE, the sun scattered forward
//   off the snow, broad on powder and gathered into a harder streak the more packed it is; the wet
//   film's sheen; the GLITTER, one crystal at most a cell of world space at a random facet, flaring
//   as a point where it lines the sun up with the eye (two sizes of cell, none on bare ground or a
//   pressed floor, none past 140 m). It returns the colour and the glint, which the caller marks
//   reactive so TAA does not smear it.
//
// The building blocks (`snow_hash1`, `snow_hash3`, `snow_noise`, `snow_noise_grad`,
// `snow_relief_layer`, `snow_glints`) are here too, for a material's own additions.
//
// ge:summary WGSL library: the snow light model (relief, wrap, glare, glitter) for any material
// ge:keywords snow snow-light snow-material micro-relief grain wrap-lighting terminator glare
// ge:keywords glitter glints crystal snow_noise snow_noise_grad snow_relief snow_light wgsl library
// ge:skill render snow
// ge:test tests/mountain_snow_test.cpp

#include "render/wgsl/frame.wgsl"

fn snow_hash3(p0: vec3f) -> vec3f {
  var p = fract(p0 * vec3f(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yxz + 33.33);
  return fract((p.xxy + p.yxx) * p.zyx);
}

fn snow_hash1(p0: vec2f) -> f32 {
  var p = fract(p0 * vec2f(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}

fn snow_noise(p: vec2f) -> f32 {
  let i = floor(p);
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  return mix(mix(snow_hash1(i), snow_hash1(i + vec2f(1.0, 0.0)), u.x),
             mix(snow_hash1(i + vec2f(0.0, 1.0)), snow_hash1(i + vec2f(1.0, 1.0)), u.x), u.y);
}

// The gradient of `snow_noise` at p, analytic (one cell's four hashes and the smoothstep's
// slope), per unit of p.
fn snow_noise_grad(p: vec2f) -> vec2f {
  let i = floor(p);
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  let du = 6.0 * f * (1.0 - f);
  let a = snow_hash1(i);
  let b = snow_hash1(i + vec2f(1.0, 0.0));
  let c = snow_hash1(i + vec2f(0.0, 1.0));
  let d = snow_hash1(i + vec2f(1.0, 1.0));
  let k = a - b - c + d;
  return du * vec2f(b - a + k * u.y, c - a + k * u.x);
}

// A layer of relief: noise of wavelength 1/`freq` m and height `amp` m, as a world slope, faded to
// nought where a pixel spans more than a quarter of a wavelength (`foot`: m a pixel).
fn snow_relief_layer(p: vec2f, freq: f32, amp: f32, foot: f32, seed: f32) -> vec2f {
  let aa = 1.0 - smoothstep(0.12, 0.25, foot * freq);
  if (aa <= 0.0) {
    return vec2f(0.0);
  }
  return snow_noise_grad(p * freq + seed) * freq * amp * aa;
}

// ONE LAYER OF CRYSTALS: a cell of `1/scale` m holds at most one (`keep` of them), at a random
// place in it with a random facet; it flares when that facet lines the sun up with the eye
// (`h`, the half vector). Drawn as a point at least a pixel across, dimmed as it is widened.
fn snow_glints(wp: vec3f, scale: f32, sharp: f32, keep: f32, n: vec3f, h: vec3f, seed: f32,
          foot: f32) -> f32 {
  let q = wp * scale;
  let cell = floor(q);
  let r = snow_hash3(cell + seed);
  if (r.z > keep) {
    return 0.0;
  }
  let facet = normalize(n + (snow_hash3(cell + seed + 7.13) * 2.0 - 1.0) * 0.6);
  let g = pow(max(dot(facet, h), 0.0), sharp);
  let f = fract(q) - (0.2 + 0.6 * r);
  let rad = max(0.14, foot * scale * 0.75);
  let spot = 1.0 - smoothstep(rad * 0.45, rad, length(f));
  let k = 0.14 / rad;
  return g * spot * k * k;
}

// The micro-relief at plan point p (m), as a world slope before any gain: `packed` 0..1 the
// packed share, `bare` 0..1 the share that is not snow (rock), `loose` 0..1 the share whose grain
// shows (a material dims it on a packed piste and a pressed floor).
fn snow_relief(p: vec2f, foot: f32, packed: f32, bare: f32, loose: f32) -> vec2f {
  var grad = vec2f(0.0);
  // Swells a few metres long on the loose snow, and the grain over everything.
  grad += snow_relief_layer(p, 0.23, 0.35, foot, 0.0) * (1.0 - packed) * (1.0 - bare);
  grad += snow_relief_layer(p, 1.7, 0.035, foot, 3.1) * (1.0 - 0.5 * packed);
  grad += snow_relief_layer(p, 7.0, 0.007, foot, 8.3) * loose;
  // Bare ground: a coarse, broken face.
  grad += (snow_relief_layer(p, 0.5, 0.6, foot, 17.0) +
           snow_relief_layer(p, 2.3, 0.08, foot, 5.0)) * bare;
  return grad;
}

// A point of snow as a material hands it to `snow_light`.
struct SnowPoint {
  albedo: vec3f,  // the colour lit, wet already folded in (`wet_albedo`)
  n: vec3f,       // the normal with the relief bent in
  geo_n: vec3f,   // the surface's own normal (the wet film pools on its flats)
  v: vec3f,       // towards the eye
  world: vec3f,   // world position, m (the crystals' cells)
  sun: vec3f,     // the sun's irradiance reaching the point
  dist: f32,      // m to the eye
  foot: f32,      // m a pixel
  packed: f32,    // 0 loose .. 1 packed: the glare's gloss
  bare: f32,      // 0 snow .. 1 bare ground: no wrap, glare or glitter
  loose: f32,     // the share the glitter shows on
  cavity: f32,    // 1 open sky .. less in a deep trough
}

// The gains, each 1 for the tuned look and 0 for none of it.
struct SnowGains {
  glitter: f32,
  wrap: f32,
  glare: f32,
}

// The light on a point of snow: the colour (rgb) and the glint (a), to mark reactive.
fn snow_light(s: SnowPoint, gains: SnowGains) -> vec4f {
  let n = s.n;
  let l = frame.sun.xyz;
  let v = s.v;
  let ndl = dot(n, l);
  var c = s.albedo / PI * (s.sun * max(ndl, 0.0) + sky_irradiance(n) * s.cavity);
  // The wrap: what the terminator gains, the blue of light through a few centimetres of ice.
  let wrap = max(0.0, (ndl + 0.45) / 1.45) - max(ndl, 0.0);
  c += s.albedo / PI * s.sun * wrap * vec3f(0.55, 0.78, 1.0) * 0.9 * gains.wrap * (1.0 - s.bare);
  // The glare: forward scatter off the snow, the brighter the more grazing the look; packed snow
  // gathers it into a harder streak.
  let rv = max(dot(reflect(-l, n), v), 0.0);
  let gloss = clamp(0.35 * s.packed, 0.0, 1.0) * (1.0 - s.bare);
  let sharp = mix(6.0, 140.0, gloss * gloss);
  var graze = 1.0 - max(dot(n, v), 0.0);
  graze *= graze;
  let lobe = pow(rv, sharp) * (0.25 + 2.5 * gloss * gloss) * (0.15 + 1.6 * graze);
  c += s.sun * lobe * 0.35 * gains.glare * (1.0 - s.bare) * (1.0 - 0.7 * frame.mist.z) / PI;
  // The wet film's mirror and the slush's pools on the flats.
  c += wet_sheen(n, v, s.sun, s.geo_n.y);
  // The glitter, on the loose snow in the sun: fine cells close in, coarser ones a little
  // further, none past 140 m.
  var glint = 0.0;
  if (s.dist < 140.0 && gains.glitter > 0.0) {
    let h = normalize(l + v);
    glint += snow_glints(s.world, 7.0, 300.0, 0.6, n, h, 0.0, s.foot) *
             (1.0 - smoothstep(15.0, 45.0, s.dist));
    glint += snow_glints(s.world, 2.5, 450.0, 0.35, n, h, 31.0, s.foot) *
             smoothstep(8.0, 25.0, s.dist) * (1.0 - smoothstep(50.0, 140.0, s.dist));
    // A glint is the sun's disc in a crystal: through cloud the disc spreads and the glint goes
    // before the light does.
    glint *= s.loose * gains.glitter * frame.weather.x * (1.0 - frame.mist.z);
    c += s.sun * glint * 400.0 / PI;
  }
  return vec4f(c, glint);
}
