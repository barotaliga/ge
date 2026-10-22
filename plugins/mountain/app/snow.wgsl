// THE SNOW'S MATERIAL (mountain/app/snow.hpp): the terrain's `fs_main` for a snow mountain, over
// render/wgsl/terrain_draw.wgsl. Every term is light: nothing here moves the ground.
//
// - THE GROUND MAP (group 3, `snow_ground`): the packed share (the groomed piste), the rock share
//   (too steep to hold snow) and the piste's direction as the doubled angle (a comb line has no
//   front and back), faded to nought off the piste.
// - THE MICRO-RELIEF, bent into the baked normal as a slope (dh/dx, dh/dz): wind-packed swells a
//   few metres long and a finer grain on the loose snow; on the piste the groomer's CORDUROY
//   across its direction (a comb 14 cm a rib, worn in patches) and the machine's LANES a swath
//   wide, each a shade of its own; on rock, a coarse broken face. Every term fades out, by the
//   pixel's own footprint, before it aliases.
// - THE LIGHT: Lambert on the bent normal; the WRAP, the soft terminator light that has passed
//   through a few centimetres of ice brings round the shadow side, tinted its blue; the GLITTER,
//   one crystal at most a cell of world space at a random facet, flaring as a point where it
//   lines the sun up with the eye (two sizes of cell, none on the piste or rock, none past
//   140 m), marked reactive so TAA does not smear it; the GLARE, the sun scattered forward off
//   the snow, broad on powder and gathered into a harder streak on the polished piste; the
//   sky's light on the bent normal.

#include "render/wgsl/terrain_draw.wgsl"

struct SnowParams {
  look: vec4f,   // glitter, corduroy, relief, wrap (each a gain; 1 is the tuned look)
  look2: vec4f,  // glare, lanes, 0, 0
}

@group(3) @binding(0) var<uniform> snow: SnowParams;
@group(3) @binding(1) var snow_ground: texture_2d<f32>;

fn hash3(p0: vec3f) -> vec3f {
  var p = fract(p0 * vec3f(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yxz + 33.33);
  return fract((p.xxy + p.yxx) * p.zyx);
}

fn hash1(p0: vec2f) -> f32 {
  var p = fract(p0 * vec2f(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}

fn noise(p: vec2f) -> f32 {
  let i = floor(p);
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash1(i), hash1(i + vec2f(1.0, 0.0)), u.x),
             mix(hash1(i + vec2f(0.0, 1.0)), hash1(i + vec2f(1.0, 1.0)), u.x), u.y);
}

// The gradient of `noise` at p, analytic (one cell's four hashes and the smoothstep's slope),
// per unit of p.
fn noise_grad(p: vec2f) -> vec2f {
  let i = floor(p);
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  let du = 6.0 * f * (1.0 - f);
  let a = hash1(i);
  let b = hash1(i + vec2f(1.0, 0.0));
  let c = hash1(i + vec2f(0.0, 1.0));
  let d = hash1(i + vec2f(1.0, 1.0));
  let k = a - b - c + d;
  return du * vec2f(b - a + k * u.y, c - a + k * u.x);
}

// A layer of relief: noise of wavelength 1/`freq` m and height `amp` m, as a world slope, faded to
// nought where a pixel spans more than a quarter of a wavelength (`foot`: m a pixel).
fn relief(p: vec2f, freq: f32, amp: f32, foot: f32, seed: f32) -> vec2f {
  let aa = 1.0 - smoothstep(0.12, 0.25, foot * freq);
  if (aa <= 0.0) {
    return vec2f(0.0);
  }
  return noise_grad(p * freq + seed) * freq * amp * aa;
}

// ONE LAYER OF CRYSTALS: a cell of `1/scale` m holds at most one (`keep` of them), at a random
// place in it with a random facet; it flares when that facet lines the sun up with the eye
// (`h`, the half vector). Drawn as a point at least a pixel across, dimmed as it is widened.
fn glints(wp: vec3f, scale: f32, sharp: f32, keep: f32, n: vec3f, h: vec3f, seed: f32,
          foot: f32) -> f32 {
  let q = wp * scale;
  let cell = floor(q);
  let r = hash3(cell + seed);
  if (r.z > keep) {
    return 0.0;
  }
  let facet = normalize(n + (hash3(cell + seed + 7.13) * 2.0 - 1.0) * 0.6);
  let g = pow(max(dot(facet, h), 0.0), sharp);
  let f = fract(q) - (0.2 + 0.6 * r);
  let rad = max(0.14, foot * scale * 0.75);
  let spot = 1.0 - smoothstep(rad * 0.45, rad, length(f));
  let k = 0.14 / rad;
  return g * spot * k * k;
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let g = ground_at(in);
  let gm = textureSampleLevel(snow_ground, linear_clamp, g.uv, 0.0);
  let packed = gm.r;
  let rock = gm.g;
  let dir = gm.ba * 2.0 - 1.0;
  let along = clamp(length(dir), 0.0, 1.0);
  let loose = (1.0 - 0.85 * packed) * (1.0 - rock);
  let p = g.xz;
  // m a pixel at this point (derivatives taken here, in uniform control flow).
  let dp = fwidth(p);
  let foot = max(dp.x, dp.y);
  let world = g.rel + frame.eye.xyz;

  // THE MICRO-RELIEF, as a slope.
  var grad = vec2f(0.0);
  let gain = snow.look.z;
  // Swells a few metres long on the loose snow, and the grain over everything.
  grad += relief(p, 0.23, 0.35, foot, 0.0) * (1.0 - packed) * (1.0 - rock);
  grad += relief(p, 1.7, 0.035, foot, 3.1) * (1.0 - 0.5 * packed);
  grad += relief(p, 7.0, 0.007, foot, 8.3) * loose;
  // Rock: a coarse, broken face.
  grad += (relief(p, 0.5, 0.6, foot, 17.0) + relief(p, 2.3, 0.08, foot, 5.0)) * rock;
  grad *= gain;

  // THE CORDUROY and THE LANES, across the piste's way.
  var shade = 1.0;
  let th = 0.5 * atan2(dir.y, dir.x);
  let across = vec2f(cos(th), -sin(th));
  let on = packed * smoothstep(0.05, 0.5, along) * (1.0 - rock);
  if (on > 0.01) {
    // The comb: 14 cm a rib, faded once a pixel spans a third of one; worn in patches a few
    // metres long and chipped a hand across.
    let phase = dot(p, across) * (2.0 * PI / 0.14);
    let aa = 1.0 - smoothstep(0.2, 0.45, foot / 0.14);
    let worn = smoothstep(0.3, 0.7, noise(p * 0.42 + 11.0));
    let chip = noise(p * 2.6 + 5.0);
    let k = on * aa * mix(0.3, 1.0, worn) * mix(0.55, 1.0, chip) * snow.look.y;
    grad += across * cos(phase) * 0.14 * k * 0.5;
    // The lanes: a swath 5.2 m wide each, a shade of its own, a low seam between two.
    let lane = dot(p, across) / 5.2;
    let seam = 1.0 - smoothstep(0.0, 0.035, abs(fract(lane) - 0.5));
    let laa = 1.0 - smoothstep(0.3, 0.8, foot / 5.2 * 12.0);
    let kl = on * snow.look2.y;
    grad += across * sign(fract(lane) - 0.5) * seam * 0.08 * kl * laa;
    shade += ((hash1(vec2f(floor(lane), 7.0)) * 2.0 - 1.0) * 0.025 - seam * laa * 0.02) * kl;
  }
  let n = normalize(g.n - vec3f(grad.x, 0.0, grad.y));

  // THE LIGHT.
  let l = frame.sun.xyz;
  let v = -g.rel / max(g.dist, 1e-3);
  let ndl = dot(n, l);
  let albedo = g.albedo * shade;
  var c = albedo / PI * (g.sun * max(ndl, 0.0) + sky_irradiance(n));
  // The wrap: what the terminator gains, the blue of light through a few centimetres of ice.
  let wrap = max(0.0, (ndl + 0.45) / 1.45) - max(ndl, 0.0);
  c += albedo / PI * g.sun * wrap * vec3f(0.55, 0.78, 1.0) * 0.9 * snow.look.w * (1.0 - rock);
  // The glare: forward scatter off the snow, the brighter the more grazing the look; the
  // groomed piste gathers it into a harder streak.
  let rv = max(dot(reflect(-l, n), v), 0.0);
  let gloss = clamp(0.35 * packed, 0.0, 1.0) * (1.0 - rock);
  let sharp = mix(6.0, 140.0, gloss * gloss);
  var graze = 1.0 - max(dot(n, v), 0.0);
  graze *= graze;
  let lobe = pow(rv, sharp) * (0.25 + 2.5 * gloss * gloss) * (0.15 + 1.6 * graze);
  c += g.sun * lobe * 0.35 * snow.look2.x * (1.0 - rock) / PI;
  // The glitter, on the loose snow in the sun: fine cells close in, coarser ones a little
  // further, none past 140 m.
  var glint = 0.0;
  if (g.dist < 140.0 && snow.look.x > 0.0) {
    let h = normalize(l + v);
    glint += glints(world, 7.0, 300.0, 0.6, n, h, 0.0, foot) *
             (1.0 - smoothstep(15.0, 45.0, g.dist));
    glint += glints(world, 2.5, 450.0, 0.35, n, h, 31.0, foot) * smoothstep(8.0, 25.0, g.dist) *
             (1.0 - smoothstep(50.0, 140.0, g.dist));
    glint *= loose * snow.look.x;
    c += g.sun * glint * 400.0 / PI;
  }
  return ground_out(in, c, clamp(glint * 4.0, 0.0, 1.0));
}
