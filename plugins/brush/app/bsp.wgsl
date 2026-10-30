// A CLASSIC BSP MAP DRAWN (plugins/brush/app/bsp_draw.hpp): its surfaces with their textures
// (from the atlas, wrapped inside each texture's rectangle: point-sampled at the mip the pixel's
// footprint picks in the classic mode or v2's crisp one, else filtered by hand, bilinear between
// texels, between two mips and with up to four taps along a slanted footprint, since an atlas
// cannot wrap a sampler's filter) and either the map's own LIGHTMAPS or REAL-TIME LIGHTS.
//
// - MODE 0, CLASSIC: the textures times the lightmaps (a luxel of 128 is the texture as it is),
//   the liquids drawn as their own warped textures, unlit, and the sky faces as two scrolling
//   layers of the sky texture.
// - MODE 1, V2: the liquids are not drawn here (our water, lava and slime bodies are), and the sky
//   faces write a MASK the fix-up pass turns into the cleared depth, so our sky and clouds fill
//   them. Lit by the lightmaps (`info.y` 0) or by the lights (`info.y` 1): every point light's
//   inverse square under a window that closes at its reach, its shadow from a cube of depth
//   (the nearest lights'), the sun through the sky faces with its own shadow map, the sky's light
//   by how open a point is to the zenith (a second map from straight overhead), a floor of
//   ambient light, and the texels that glow.
//
// THE WEATHER (`bsp.weather.x` 1): a face the fall reaches (the fall's map, along the way it comes,
// and the face turned toward it) takes the frame's wet ground (`frame.wet.x`: darker, a film that
// mirrors the sky), its puddles on the flats (`frame.wet.y`: pools where a slow noise is lowest,
// filling as it rises, level, sharp, rippled by the drops while it rains) and its snow
// (`frame.wet.z`: on what faces up, settling first where the noise is highest), and a lightning
// flash (`frame.wet.w`) lights what is open to the sky and leaks a little into the rooms.
//
// A light's strength is in the lightmap's units (1: a white face as the open day lights it, so
// the classic map's balance holds whatever the hour) or, for a glowing liquid's light, physical
// (cd); `colour.w` says which.
//
// ge:summary A classic BSP map drawn: atlas textures, lightmaps or real-time lights, the sky mask
// ge:keywords bsp brush render atlas mip lightmap point-light cube-shadow sun-shadow zenith
// ge:keywords openness sky-mask classic v2 warp fullbright weather wet puddle ripples snow lightning
// ge:skill brush render
#include "render/wgsl/frame.wgsl"

struct Bsp {
  info: vec4f,   // mode (0 classic, 1 v2), lighting (0 lightmaps, 1 lights, 2 rebaked), lights,
                 // ambient
  info2: vec4f,  // time s, the cube face's size, the ortho map's size, the brightness
  sky: vec4f,    // the classic sky texture's rectangle
  info3: vec4f,  // v2's soft floor of light (minlight), the baked light's share as bounce, the
                 // sky's light (r_skylight: the sun's and the open sky's in the rooms), the
                 // textures filtered (1) or crisp (0)
  info4: vec4f,  // the relief's depth from the textures' luminance, m (r_bump); unused
  veil: vec4f,   // the sky faces' pixels times these (bsp_fix.wgsl's veil)
  weather: vec4f,  // the weather on (1), the rain falling now (0 .. 1), the fall's spread, 0
  ortho: array<Job, 3>,  // the sun's map, the zenith's and the fall's
  info5: vec4f,  // a lightmap layer's height in the atlas's uv, the layers
  styles: array<vec4f, 16>,  // the 64 light styles' levels now, four a vector
};

// A shadow map's camera: an origin, three axes (z the way it looks) and its reach: a point
// light's cube face (`info.x` 0; `o.w` its far) or an orthographic map (`info.x` 1; `o.w` its
// half width, `info.y` its half depth).
struct Job {
  o: vec4f,
  x: vec4f,
  y: vec4f,
  z: vec4f,
  info: vec4f,
};

struct Light {
  pos: vec4f,     // world m; w: its reach, m
  colour: vec4f,  // its strength (rgb, the flicker in it); w: 1 physical (cd), 0 lightmap units
  info: vec4f,    // x: its shadow's first layer (six a light), -1 none; y: its source's size^2, m^2;
                  // z: 1 a dynamic light added to the lightmaps (colour its middle's light, pos.w
                  // its radius)
};

@group(1) @binding(0) var<uniform> bsp: Bsp;
@group(1) @binding(1) var atlas: texture_2d<f32>;
@group(1) @binding(2) var light_atlas: texture_2d<f32>;
@group(1) @binding(3) var light_sampler: sampler;
@group(1) @binding(4) var<storage, read> lights: array<Light>;
@group(1) @binding(5) var cubes: texture_depth_2d_array;
@group(1) @binding(6) var shadow_cmp: sampler_comparison;
@group(1) @binding(7) var orthos: texture_depth_2d_array;
@group(1) @binding(8) var rebake_rgb: texture_2d<f32>;
@group(1) @binding(9) var rebake_dir: texture_2d<f32>;
@group(1) @binding(10) var<storage, read> frames: array<vec4f>;  // the animated textures' frames

const kSky = 1u;
const kWater = 2u;
const kSlime = 3u;
const kLava = 4u;
const kWarp = 6u;
const kTele = 7u;

struct VsIn {
  @location(0) pos: vec3f,
  @location(1) st: vec2f,
  @location(2) lm: vec2f,
  @location(3) nrm: vec3f,
  @location(4) rect: vec4f,
  @location(5) kind: u32,
  @location(6) lm2: vec2f,
  @location(7) styles: u32,
  @location(8) anim: u32,
};

struct VsOut {
  @builtin(position) clip: vec4f,
  @location(0) world: vec3f,
  @location(1) st: vec2f,
  @location(2) lm: vec2f,
  @location(3) nrm: vec3f,
  @location(4) @interpolate(flat) rect: vec4f,
  @location(5) @interpolate(flat) kind: u32,
  @location(6) lm2: vec2f,
  @location(7) @interpolate(flat) styles: u32,
};

// The light styles: a texture's frame played at five a second, a style's level now.
const kFrameRate = 5.0;

fn style_level(id: u32) -> f32 {
  return bsp.styles[id >> 2u][id & 3u];
}

@vertex
fn vs_main(v: VsIn) -> VsOut {
  var o: VsOut;
  o.clip = frame.view_proj * vec4f(v.pos - frame.eye.xyz, 1.0);
  o.world = v.pos;
  o.st = v.st;
  o.lm = v.lm;
  o.nrm = v.nrm;
  o.rect = v.rect;
  // An animated texture: the sequence's frame now, from this texture's own place in it.
  let count = (v.anim >> 16u) & 255u;
  if (count > 1u) {
    let at = (u32(max(bsp.info2.x, 0.0) * kFrameRate) + (v.anim >> 24u)) % count;
    o.rect = frames[(v.anim & 0xffffu) + at];
  }
  o.kind = v.kind;
  o.lm2 = v.lm2;
  o.styles = v.styles;
  return o;
}

// The texel at `st` (texels of mip 0) of the texture at `rect`, at mip `lod`, wrapped.
fn texel(st: vec2f, rect: vec4f, lod: f32) -> vec4f {
  let k = u32(clamp(lod, 0.0, f32(textureNumLevels(atlas) - 1u)));
  let s = f32(1u << k);
  let size = max(floor(rect.zw / s), vec2f(1.0));
  var t = floor(st / s);
  t = t - size * floor(t / size);
  return textureLoad(atlas, vec2i(floor(rect.xy / s) + t), i32(k));
}

// The texel at integer `t` of mip `k` of the texture at `rect`, wrapped.
fn texel_at(t: vec2f, rect: vec4f, k: u32) -> vec4f {
  let s = f32(1u << k);
  let size = max(floor(rect.zw / s), vec2f(1.0));
  let w = t - size * floor(t / size);
  return textureLoad(atlas, vec2i(floor(rect.xy / s) + w), i32(k));
}

// `st` (texels of mip 0) at mip `k`, bilinear between its four nearest texels.
fn bilinear(st: vec2f, rect: vec4f, k: u32) -> vec4f {
  let p = st / f32(1u << k) - 0.5;
  let i = floor(p);
  let f = p - i;
  let a = texel_at(i, rect, k);
  let b = texel_at(i + vec2f(1.0, 0.0), rect, k);
  let c = texel_at(i + vec2f(0.0, 1.0), rect, k);
  let d = texel_at(i + vec2f(1.0, 1.0), rect, k);
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// `st` filtered over the pixel's footprint (`dx`, `dy`: its texels along the screen's axes):
// the mip by the footprint's minor axis over up to four taps along its major one, blended
// between the two mips about it.
fn filtered(st: vec2f, rect: vec4f, dx: vec2f, dy: vec2f) -> vec4f {
  let lx = length(dx);
  let ly = length(dy);
  let major = select(dy, dx, lx > ly);
  let big = max(lx, ly);
  let small = max(min(lx, ly), 1e-6);
  let taps = clamp(ceil(big / small), 1.0, 4.0);
  let top = textureNumLevels(atlas) - 1u;  // 3, or 5 when a texture is upscaled
  let lod = clamp(log2(max(big / taps, 1e-6)), 0.0, f32(top));
  let k0 = u32(floor(lod));
  let k1 = min(k0 + 1u, top);
  let fr = lod - floor(lod);
  var sum = vec4f(0.0);
  for (var i = 0; i < i32(taps); i++) {
    let q = st + major * ((f32(i) + 0.5) / taps - 0.5);
    sum += mix(bilinear(q, rect, k0), bilinear(q, rect, k1), fr);
  }
  return sum / taps;
}

// THE TELEPORT'S MAGIC LIQUID (v2): a black ink marbled with grey, flowing UP the panel; silver
// threads climbing it in lanes; motes rising and twinkling; a pale shimmer where it is seen edge
// on; the whole breathing slowly. Its own light, in a white face's units.
fn t_hash(p: vec2f) -> f32 {
  let q = fract(p * vec2f(0.1031, 0.1030));
  let r = q + dot(q, q.yx + 33.33);
  return fract((r.x + r.y) * r.x);
}

fn t_noise(p: vec2f) -> f32 {
  let i = floor(p);
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  return mix(mix(t_hash(i), t_hash(i + vec2f(1.0, 0.0)), u.x),
             mix(t_hash(i + vec2f(0.0, 1.0)), t_hash(i + vec2f(1.0, 1.0)), u.x), u.y);
}

fn t_fbm(p: vec2f) -> f32 {
  var sum = 0.0;
  var a = 0.5;
  var q = p;
  for (var i = 0; i < 5; i++) {
    sum += a * t_noise(q);
    q = mat2x2f(1.6, 1.2, -1.2, 1.6) * q;
    a *= 0.5;
  }
  return sum;
}

fn teleport(world: vec3f, n: vec3f, eye_dir: vec3f, t: f32) -> vec3f {
  // The panel's own axes, m: across it and up.
  let across = normalize(vec3f(-n.z, 0.0, n.x) + vec3f(1e-4, 0.0, 0.0));
  let p = vec2f(dot(world, across), world.y);
  // The ink: black, smoke folded through it in thin grey wisps, warped twice and carried up.
  let up = vec2f(0.0, -0.35 * t);
  let w1 = vec2f(t_fbm(p * 1.1 + up), t_fbm(p * 1.1 + up + vec2f(5.2, 1.3)));
  let w2 = vec2f(t_fbm(p * 1.9 + 2.4 * w1 + 1.3 * up + vec2f(1.7, 9.2) + 0.05 * t),
                 t_fbm(p * 1.9 + 2.4 * w1 + 1.3 * up + vec2f(8.3, 2.8) - 0.05 * t));
  let m = t_fbm(p * 2.6 + 3.0 * w2 + up);
  let smoke = pow(max(1.0 - abs(2.0 * m - 1.0), 0.0), 9.0);
  let fine = pow(max(1.0 - abs(2.0 * t_fbm(p * 6.0 + 4.0 * w2 + 2.0 * up) - 1.0), 0.0), 14.0);
  var c = vec3f(0.003, 0.003, 0.005) + vec3f(0.025, 0.025, 0.03) * smoothstep(0.4, 0.8, w2.y);
  c += vec3f(0.09, 0.09, 0.1) * smoke + vec3f(0.3, 0.31, 0.36) * fine * smoke;
  // The threads: a liquid that runs UP the panel, a few lanes of it, each swaying and lit in
  // drops that climb it, with a halo.
  let lane_at = p.x * 5.0 + 0.6 * sin(1.3 * p.y + 0.7 * t) + 1.2 * (w1.x - 0.5);
  let lane_id = floor(lane_at);
  let live = step(0.55, t_hash(vec2f(lane_id, 7.0)));
  let lane = abs(fract(lane_at) - 0.5);
  let core = 1.0 - smoothstep(0.0, 0.025, lane);
  let halo = exp(-lane * 22.0);
  let speed = 1.2 + 1.4 * t_hash(vec2f(lane_id, 3.0));
  let drops = smoothstep(0.5, 0.95, t_noise(vec2f(lane_id * 3.1, p.y * 1.6 - speed * t)));
  let flow = live * (0.25 + 1.6 * drops);
  c += vec3f(1.1, 1.12, 1.2) * core * flow + vec3f(0.14, 0.15, 0.19) * halo * flow;
  // The motes: sparks in a few cells, rising and swaying, twinkling, each with a glow.
  let cp = vec2f(p.x * 6.0 + 0.3 * sin(p.y * 2.0 + t), p.y * 6.0 - 1.8 * t);
  let ci = floor(cp);
  let h = t_hash(ci);
  let at = vec2f(t_hash(ci + 17.0), t_hash(ci + 31.0)) * 0.6 + 0.2;
  let r = length(fract(cp) - at);
  let twinkle = pow(max(0.5 + 0.5 * sin(5.0 * t + 40.0 * h), 0.0), 3.0);
  c += vec3f(1.0, 1.0, 1.1) * step(0.8, h) * (smoothstep(0.05, 0.0, r) * 3.0 + exp(-r * 18.0) * 0.4) *
       twinkle;
  // The rim, and the breath.
  c += vec3f(0.3, 0.31, 0.4) * pow(max(1.0 - abs(dot(n, eye_dir)), 0.0), 3.0);
  return c * (0.85 + 0.15 * sin(1.7 * t));
}

// The dynamic lights on the lightmaps' light (the classic's and v2's lightmaps): each falls off
// in a straight line from its middle to nothing at its radius, whichever way the face faces.
fn dynamic_light(p: vec3f) -> vec3f {
  var e = vec3f(0.0);
  let count = u32(bsp.info.z);
  for (var i = 0u; i < count; i++) {
    let l = lights[i];
    if (l.info.z < 0.5) {
      continue;
    }
    let d = length(p - l.pos.xyz);
    e += l.colour.rgb * max(1.0 - d / l.pos.w, 0.0);
  }
  return e;
}

// V2's floor: no face darker than about `m`, a lit one nearly as it was (sqrt(e^2 + m^2)).
fn floor_light(e: vec3f, m: f32) -> vec3f {
  return sqrt(e * e + vec3f(m * m));
}

// The light the face's lightmaps give it now: each of its styles' layer times the style's level.
fn lightmap(lm: vec2f, styles: u32) -> vec3f {
  if (lm.x < 0.0) {
    return vec3f(1.0);
  }
  var e = vec3f(0.0);
  for (var k = 0u; k < 4u; k++) {
    let id = (styles >> (8u * k)) & 255u;
    if (id == 255u) {
      break;
    }
    if (id < 64u) {
      let at = lm + vec2f(0.0, f32(k) * bsp.info5.x);
      e += textureSampleLevel(light_atlas, light_sampler, at, 0.0).rgb * style_level(id);
    }
  }
  return e * (255.0 / 128.0);
}

// THE DIRECTIONAL SHADE (engine/bake/bake.hpp, `directional_scale`, restated): the rebaked
// irradiance's factor for the detailed normal `n` of a face whose flat normal is `ng`, under the
// light's mean direction `d` (its length the share from that way).
fn directional_scale(d: vec3f, n: vec3f, ng: vec3f) -> f32 {
  let a = max(dot(n, d) * 0.5 + 0.5, 0.0);
  let b = max(dot(ng, d) * 0.5 + 0.5, 0.05);
  return a / b;
}

// The texture's relief as a normal: its luminance taken as a height (`depth` m from black to
// white), differenced at the pixel's neighbours along the screen's axes (`dx`, `dy`: their
// texels) and turned into a normal by the surface gradient over the pixel's world steps
// (`dpx`, `dpy`), so no tangents are needed.
fn relief_normal(ng: vec3f, st: vec2f, rect: vec4f, lod: f32, dx: vec2f, dy: vec2f, dpx: vec3f,
                 dpy: vec3f, depth: f32) -> vec3f {
  if (depth <= 0.0) {
    return ng;
  }
  let k = u32(clamp(lod, 0.0, f32(textureNumLevels(atlas) - 1u)));
  let w = vec3f(0.2126, 0.7152, 0.0722);
  let h0 = dot(bilinear(st, rect, k).rgb, w);
  let hx = (dot(bilinear(st + dx, rect, k).rgb, w) - h0) * depth;
  let hy = (dot(bilinear(st + dy, rect, k).rgb, w) - h0) * depth;
  let r1 = cross(dpy, ng);
  let r2 = cross(ng, dpx);
  let det = dot(dpx, r1);
  if (abs(det) < 1e-12) {
    return ng;
  }
  let grad = sign(det) * (hx * r1 + hy * r2);
  return normalize(abs(det) * ng - grad);
}

// The classic sky: a far layer and a near one over it (its texel 0 clear), scrolling.
fn classic_sky(d: vec3f) -> vec3f {
  let k = 1.0 / (abs(d.y) * 3.0 + 0.35);
  let uv = d.xz * k * 48.0;
  let t = bsp.info2.x;
  let half = vec4f(bsp.sky.x + bsp.sky.z * 0.5, bsp.sky.y, bsp.sky.z * 0.5, bsp.sky.w);
  let near = vec4f(bsp.sky.x, bsp.sky.y, bsp.sky.z * 0.5, bsp.sky.w);
  let back = texel(uv + vec2f(t * 8.0), half, 0.0);
  let front = texel(uv + vec2f(t * 16.0), near, 0.0);
  return select(front.rgb, back.rgb, front.a > 0.002 && front.a < 0.006);
}

// The share of a point a cube of depth sees lit: its face by the major axis of `q` (from the
// light to the point), its texel there, 2 x 2 compared.
fn cube_shadow(q: vec3f, far: f32, base: i32) -> f32 {
  let a = abs(q);
  var i = 0;
  if (a.y > a.x && a.y >= a.z) {
    i = 1;
  } else if (a.z > a.x && a.z > a.y) {
    i = 2;
  }
  let m = a[i];
  let f = 2 * i + select(0, 1, q[i] < 0.0);
  let u = q[(i + 1) % 3] / m;
  let v = q[(i + 2) % 3] / m;
  let uv = vec2f(0.5 + 0.5 * u, 0.5 - 0.5 * v);
  let ref_depth = length(q) / far - 0.004;
  return textureSampleCompareLevel(cubes, shadow_cmp, uv, base + f, ref_depth);
}

// An orthographic map's lit share at `p` over a 3 x 3 spread of `spread` m.
fn ortho_lit(j: Job, layer: i32, p: vec3f, spread: f32) -> f32 {
  let q = p - j.o.xyz;
  let x = dot(q, j.x.xyz) / j.o.w;
  let y = dot(q, j.y.xyz) / j.o.w;
  let z = (dot(q, j.z.xyz) + j.info.y) / (2.0 * j.info.y);
  let step = spread / j.o.w * 0.5;
  var lit = 0.0;
  for (var dy = -1; dy <= 1; dy++) {
    for (var dx = -1; dx <= 1; dx++) {
      let uv = vec2f(0.5 + 0.5 * x + f32(dx) * step, 0.5 - 0.5 * y + f32(dy) * step);
      lit += textureSampleCompareLevel(orthos, shadow_cmp, uv, layer, z - 0.0015);
    }
  }
  return lit / 9.0;
}

// THE SHARE OF THE FALL THAT REACHES `p` (0 .. 1). The drops and the flakes do not come along
// one line: the gusts veer them and the eddies toss them, so they arrive on a spread of ways round
// the fall's (`bsp.weather.z`, the spread's tangent: wider for snow than for rain). Like a soft
// shadow: the faces between `p` and the sky along the fall (the blocker, searched in the fall's
// map) stand some metres above it, and the further below its opening a point lies, the wider the
// edge it sees: the twelve taps over that disc, turned by a hash of the place, say how much of
// the spread gets through. So a window's patch is densest where the line through it lands and
// thins out, scattered, round it.
fn fall_reach(p: vec3f) -> f32 {
  let j = bsp.ortho[2];
  let q = p - j.o.xyz;
  let half = j.o.w;
  let depth = j.info.y;
  let uv = vec2f(0.5 + 0.5 * dot(q, j.x.xyz) / half, 0.5 - 0.5 * dot(q, j.y.xyz) / half);
  let z = (dot(q, j.z.xyz) + depth) / (2.0 * depth) - 0.0015;
  let size = f32(textureDimensions(orthos).x);
  let spread = bsp.weather.z;
  // The blocker: the faces nearer the sky than `p` within the widest edge this map could give.
  let reach_m = min(spread * 10.0, 4.0);
  var blocker = 0.0;
  var blocked = 0.0;
  for (var k = 0; k < 9; k++) {
    let o = vec2f(f32(k % 3) - 1.0, f32(k / 3) - 1.0) * reach_m * 0.5 / half;
    let t = clamp(vec2i((uv + o) * size), vec2i(0), vec2i(i32(size) - 1));
    let d = textureLoad(orthos, t, 2, 0);
    if (d < z) {
      blocker += d;
      blocked += 1.0;
    }
  }
  if (blocked < 0.5) {
    return 1.0;
  }
  // How far below the blocker `p` lies along the fall, m, and the edge that gives.
  let below = max(z - blocker / blocked, 0.0) * 2.0 * depth;
  let edge = max(below * spread, 0.03) * 0.5 / half;
  let turn = hash12(floor(p.xz * 40.0) + vec2f(floor(p.y * 40.0))) * 6.2831853;
  let c = cos(turn);
  let sn = sin(turn);
  var lit = 0.0;
  for (var k = 0; k < 12; k++) {
    // A golden-angle spiral over the unit disc.
    let r = sqrt((f32(k) + 0.5) / 12.0);
    let a = f32(k) * 2.3999632;
    let d = vec2f(cos(a), sin(a)) * r;
    let o = vec2f(d.x * c - d.y * sn, d.x * sn + d.y * c) * edge;
    lit += textureSampleCompareLevel(orthos, shadow_cmp, uv + o, 2, z);
  }
  return lit / 12.0;
}

// A smooth value noise in 0 .. 1 over the plane.
fn noise2(p: vec2f) -> f32 {
  let i = floor(p);
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  let a = hash12(i);
  let b = hash12(i + vec2f(1.0, 0.0));
  let c = hash12(i + vec2f(0.0, 1.0));
  let d = hash12(i + vec2f(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

fn hash12(p: vec2f) -> f32 {
  let q = fract(p * vec2f(0.1031, 0.1030));
  let r = q + dot(q, q.yx + 33.33);
  return fract((r.x + r.y) * r.x);
}

// What the weather does to a face: its albedo, the light its wet film or puddle mirrors (added
// after the lighting), and the normal it is lit with (a puddle's, level and rippled).
struct Weathered {
  albedo: vec3f,
  sheen: vec3f,
  n: vec3f,
  flash: f32,  // the lightning's light on the face, a white face's units
};

// The rings a drop throws on a puddle round `xz` at time `t`: the surface's tilt (x, z).
fn ripples(xz: vec2f, t: f32) -> vec2f {
  var tilt = vec2f(0.0);
  for (var k = 0; k < 2; k++) {
    let cell = 0.22 + 0.1 * f32(k);
    let g = xz / cell + vec2f(f32(k) * 0.37);
    let id = floor(g);
    for (var j = 0; j < 4; j++) {
      let c = id + vec2f(f32(j & 1), f32(j >> 1)) - vec2f(0.5);
      let h = hash12(c * 1.7 + 3.1);
      let age = fract(t * 1.3 + h * 7.0);
      let at = (c + vec2f(hash12(c + 9.2), hash12(c + 4.7))) * cell;
      let d = xz - at;
      let r = length(d);
      let front = age * 0.11;
      let x = (r - front) / 0.012;
      let w = exp(-x * x) * (1.0 - age) * (1.0 - age);
      tilt += d / max(r, 1e-4) * w * sin((r - front) * 300.0);
    }
  }
  return tilt * 0.35;
}

fn weathered(p: vec3f, n: vec3f, albedo: vec3f, eye_dir: vec3f) -> Weathered {
  var w: Weathered;
  w.albedo = albedo;
  w.sheen = vec3f(0.0);
  w.n = n;
  w.flash = 0.0;
  if (bsp.weather.x < 0.5) {
    return w;
  }
  let fall = bsp.ortho[2].z.xyz;
  let facing = smoothstep(-0.05, 0.3, dot(n, -fall));
  var reached = 0.0;
  if (facing > 0.0) {
    reached = fall_reach(p + n * 0.04 - fall * 0.04) * facing;
  }
  // Where only a share of the fall reaches, the drops' spots: wet where they landed, dry between.
  let spots = noise2(p.xz * 9.0 + vec2f(p.y * 3.0)) * 0.6 + noise2(p.xz * 31.0) * 0.4;
  let struck = smoothstep(spots - 0.2, spots + 0.2, reached * 1.25);
  let up = n.y;
  // The snow: what faces up and the fall reaches, the noise's highest first.
  // A thin cover lies in the drifts first and the floor shows between; a deep one hides it all.
  let drift = noise2(p.xz * 1.3) * 0.5 + noise2(p.xz * 5.1) * 0.3 + noise2(p.xz * 23.0) * 0.2;
  let snowed = smoothstep(0.35, 0.75, up) * reached * frame.wet.z;
  let snow = smoothstep(0.0, 0.2, snowed - 0.85 * (1.0 - drift) + 0.15);
  // The wet: the film where the fall reaches, the puddles on the level where the noise is lowest.
  let wet = frame.wet.x * struck * (1.0 - snow);
  let low = noise2(p.xz * 0.55 + vec2f(17.0)) * 0.7 + noise2(p.xz * 1.9) * 0.3;
  let level = smoothstep(0.93, 0.985, up);
  let fill = frame.wet.y;
  let pool = level * smoothstep(0.4, 0.9, reached) * (1.0 - snow) *
             smoothstep(0.0, 0.08, fill * 0.75 - low + 0.2);
  var a = albedo * (1.0 - 0.45 * wet);
  a = mix(a, a * 0.55, pool);
  // The snow's white, a little grey in its hollows, with a glint here and there.
  let grain = noise2(p.xz * 61.0);
  let white = vec3f(0.78, 0.81, 0.86) * (0.9 + 0.1 * drift) + vec3f(0.12 * step(0.93, grain));
  a = mix(a, white, snow);
  w.albedo = a;
  // A pool lies level and the drops ring it while it rains.
  var m = normalize(mix(n, vec3f(0.0, 1.0, 0.0), pool));
  if (pool > 0.01 && bsp.weather.y > 0.0) {
    let tilt = ripples(p.xz, bsp.info2.x) * bsp.weather.y * pool;
    m = normalize(m + vec3f(tilt.x, 0.0, tilt.y));
  }
  w.n = m;
  let v = -eye_dir;
  let r = reflect(eye_dir, m);
  let cos_v = clamp(dot(m, v), 0.0, 1.0);
  let fresnel = 0.02 + 0.98 * pow(1.0 - cos_v, 5.0);
  let sky = sky_luminance(normalize(vec3f(r.x, max(r.y, 0.02), r.z)));
  let sharp = mix(60.0, 1500.0, pool);
  let glint = pow(max(dot(r, frame.sun.xyz), 0.0), sharp) * mix(4.0, 60.0, pool) *
              ortho_lit(bsp.ortho[0], 0, p + n * 0.05, 0.05);
  w.sheen = (sky + sun_light(p.y, p.xz) * glint / PI) * fresnel * max(wet, pool) *
            mix(0.5, 1.0, pool) * bsp.info3.z;
  // The lightning: through what is open overhead, and a little round the rooms.
  if (frame.wet.w > 0.0) {
    let open = ortho_lit(bsp.ortho[1], 1, p + n * 0.3, 1.2);
    w.flash = frame.wet.w * (0.04 + 1.6 * open * max(up * 0.5 + 0.5, 0.2));
  }
  return w;
}

struct FsOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
  @location(2) mask: vec4f,
};

@fragment
fn fs_main(in: VsOut) -> FsOut {
  var out: FsOut;
  out.motion = vec4f(0.0);
  out.mask = vec4f(0.0);
  let classic = bsp.info.x < 0.5;
  let t = bsp.info2.x;
  var st = in.st;
  let warped = in.kind == kWater || in.kind == kSlime || in.kind == kLava || in.kind == kWarp ||
               in.kind == kTele;
  if (warped) {
    st = st + 8.0 * sin(st.yx * (6.2831853 / 128.0) + vec2f(t));
  }
  let dx = dpdx(st);
  let dy = dpdy(st);
  let lod = log2(max(max(length(dx), length(dy)), 1e-6));
  let dpx = dpdx(in.world);
  let dpy = dpdy(in.world);
  // A lightmap's 1 (and a light in its units): a white face as the open day lights it, times the
  // brightness (the eye adapted to a dim room).
  let white = white_luminance() * bsp.info2.w;
  let eye_dir = normalize(in.world - frame.eye.xyz);

  if (in.kind == kSky) {
    if (classic) {
      out.colour = vec4f(srgb_to_linear(classic_sky(eye_dir)) * white, 1.0);
    } else {
      out.mask = vec4f(1.0);
      out.colour = vec4f(0.0, 0.0, 0.0, 1.0);
    }
    return out;
  }

  if (in.kind == kTele && !classic) {
    var nt = normalize(in.nrm);
    if (dot(nt, eye_dir) > 0.0) {
      nt = -nt;
    }
    out.colour = vec4f(teleport(in.world, nt, eye_dir, t) * white, 1.0);
    return out;
  }

  var tx = texel(st, in.rect, lod);
  if (!classic && bsp.info3.w > 0.5) {
    tx = filtered(st, in.rect, dx, dy);
  }
  let albedo = tx.rgb;
  // A glowing texel's share (alpha 1 a glowing one; filtering blends it with its neighbours).
  let glow = smoothstep(0.1, 0.9, tx.a);
  if (warped) {
    out.colour = vec4f(albedo * white, 1.0);
    return out;
  }
  var n = normalize(in.nrm);
  if (dot(n, eye_dir) > 0.0) {
    n = -n;
  }

  // ── Rebaked: the map's light worked out again (rebake.hpp), shaded by the light's direction ──
  if (!classic && bsp.info.y > 1.5 && in.lm2.x >= 0.0) {
    let ng = normalize(in.nrm);
    let raw = textureSampleLevel(rebake_dir, light_sampler, in.lm2, 0.0).xyz;
    let d = raw * (255.0 / 127.5) - vec3f(128.0 / 127.5);
    let nd = relief_normal(ng, st, in.rect, lod, dx, dy, dpx, dpy, bsp.info4.x);
    var e = textureSampleLevel(rebake_rgb, light_sampler, in.lm2, 0.0).rgb;
    e = floor_light(e * directional_scale(d, nd, ng), bsp.info3.x);
    out.colour = vec4f(albedo * e * white + albedo * white * glow, 1.0);
    return out;
  }

  if (classic || bsp.info.y < 0.5) {
    var lit = lightmap(in.lm, in.styles) + dynamic_light(in.world);
    if (classic) {
      lit = mix(lit, max(lit, vec3f(1.0)), glow);
      out.colour = vec4f(albedo * lit * white, 1.0);
      return out;
    }
    lit = floor_light(lit, bsp.info3.x);
    lit = mix(lit, max(lit, vec3f(1.0)), glow);
    let wl = weathered(in.world, n, albedo, eye_dir);
    out.colour = vec4f(wl.albedo * (lit + wl.flash) * white + wl.sheen, 1.0);
    return out;
  }

  // ── Real-time lights ──
  let wl = weathered(in.world, n, albedo, eye_dir);
  let p = in.world + n * 0.03;
  var e = vec3f(bsp.info.w);
  var phys = vec3f(0.0);
  let count = u32(bsp.info.z);
  for (var i = 0u; i < count; i++) {
    let l = lights[i];
    let q = p - l.pos.xyz;
    let d2 = dot(q, q);
    let reach = l.pos.w;
    if (d2 >= reach * reach) {
      continue;
    }
    let d = sqrt(d2);
    let ndl = dot(n, -q / max(d, 1e-4));
    if (ndl <= 0.0) {
      continue;
    }
    let r = d / reach;
    let r4 = r * r * r * r;
    let window = (1.0 - r4) * (1.0 - r4);
    // Softened near the light over its source's size (a fire's whole flame), 0.2 m at least.
    var lit = ndl * window / (d2 + max(l.info.y, 0.04));
    if (l.info.x >= 0.0) {
      lit *= cube_shadow(q, reach, i32(l.info.x));
    }
    if (l.colour.w > 0.5) {
      phys += l.colour.rgb * lit;
    } else {
      e += l.colour.rgb * lit;
    }
  }
  // The light bounced round the room: the map's baked light (its light probes) at a share.
  e += lightmap(in.lm, in.styles) * bsp.info3.y;
  e = floor_light(e, bsp.info3.x);
  e += wl.flash;
  var c = wl.albedo * e * white;
  // A physical light: illuminance (lx) on the face to its luminance in the frame's units.
  c += wl.albedo * phys / PI * frame.world.y / kSunLux;
  // The sun through the sky faces, and the sky's light by how open the point is overhead.
  let sun_dir = frame.sun.xyz;
  let sun_ndl = max(dot(wl.n, sun_dir), 0.0);
  if (sun_ndl > 0.0 && frame.sun.y > 0.0) {
    let lit = ortho_lit(bsp.ortho[0], 0, p, 0.05);
    c += wl.albedo / PI * sun_light(in.world.y, in.world.xz) * sun_ndl * lit * bsp.info3.z;
  }
  let open = ortho_lit(bsp.ortho[1], 1, in.world + n * 0.3, 1.2);
  c += wl.albedo / PI * sky_irradiance(wl.n) * open * bsp.info3.z;
  c += wl.sheen;
  c += albedo * white * glow;
  out.colour = vec4f(c, 1.0);
  // A pixel whose light came out not a number (a degenerate face) or past a half float: black,
  // never a NaN for the bloom to spread over the frame.
  let bits = bitcast<vec3u>(c) & vec3u(0x7f800000u);
  if (any(bits == vec3u(0x7f800000u)) || any(abs(c) > vec3f(6e4))) {
    out.colour = vec4f(0.0, 0.0, 0.0, 1.0);
  }
  return out;
}

fn srgb_to_linear(c: vec3f) -> vec3f {
  return select(pow((c + 0.055) / 1.055, vec3f(2.4)), c / 12.92, c <= vec3f(0.04045));
}
