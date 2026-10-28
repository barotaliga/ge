// THE GRASS'S SHARED HALF (render/grass.hpp): the kinds, a blade, the hash, the air at a blade,
// its press underfoot and its shape (the scatter of one blade is grass_scatter_common.wgsl's).
// Every function here is the WGSL twin of engine/grass's (sway.cpp's `wind_at_blade` and
// `blade_shape`, trample.cpp's `press_at`), float for float in the same order;
// tests/render_grass_test.cpp holds them to it.

// grass/kind.hpp's Kind, as render/grass.cpp packs it.
struct GrassKind {
  shape: vec4f,   // height, height_jitter, width, density
  lean: vec4f,    // lean, lean_jitter, clump, pull
  air: vec4f,     // give, sway, dry, spring
  root: vec4f,    // rgb, the share of blades in flower
  tip: vec4f,     // rgb, the share of blades in seed
  dry_tip: vec4f, // rgb
  flower: vec4f,  // rgb, a flower's head across (m)
  flower2: vec4f, // rgb, the share of the flowers in this second colour
  seed: vec4f,    // rgb, the seed head's share of the blade
}

// grass/cover.hpp's Blade.
struct Blade {
  root: vec3f,
  seed: u32,     // own hash (low 24 bits), kind (high 8)
  height: f32,
  width: f32,
  lean: vec2f,   // rest lean as a plan vector, rad
}

// grass/trample.hpp's PressCell.
struct GrassPress {
  key: u32,      // (column & 0xffff) | (row << 16); all ones: never pressed
  t: f32,        // s, when last pressed
  amount: f32,   // 0..1
  way: u32,      // the plan way its blades lie, two snorm16s
}

struct GrassParams {
  cover: vec4u,  // triangles, capacity (each list), segments near, segments far
  lod: vec4f,    // full_m (every blade kept within), near_m (fine blades within), reach_m, min_keep
  wind: vec4f,   // the grid's x0, z0, cell, nodes a side
  mean: vec4f,   // the mean plan air (x, z), the time (s), the last frame's time (s)
  trample: vec4f, // the cells' size (m), cells a side (0: none), 0, 0
}

const kGrassTau = 6.28318530717958647;
const kMaxBend = 1.35;
const kPressBend = 1.45;
const kRuffleCell = 6.0;
const kMaxBladesPerTri = 4096u;

fn grass_hash(v: u32) -> u32 {
  let state = v * 747796405u + 2891336453u;
  let word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

fn grass_unit(h: u32) -> f32 {
  return f32(h >> 8u) * (1.0 / 16777216.0);
}

// ── The air at a blade ──

fn grass_lattice(ix: f32, iz: f32) -> f32 {
  return grass_unit(grass_hash(grass_hash(bitcast<u32>(i32(ix))) ^ bitcast<u32>(i32(iz))));
}

fn grass_noise(x: f32, z: f32) -> f32 {
  let ix = floor(x);
  let iz = floor(z);
  var fx = x - ix;
  var fz = z - iz;
  fx = fx * fx * (3.0 - 2.0 * fx);
  fz = fz * fz * (3.0 - 2.0 * fz);
  let a = grass_lattice(ix, iz);
  let b = grass_lattice(ix + 1.0, iz);
  let c = grass_lattice(ix, iz + 1.0);
  let d = grass_lattice(ix + 1.0, iz + 1.0);
  let top = a + (b - a) * fx;
  let bottom = c + (d - c) * fx;
  return top + (bottom - top) * fz;
}

// The plan air at (x, z) at `t`: the grid (`grass_wind`, a vec2 a node, row-major in z, which
// the including shader binds beside `gp`) bilinear, ruffled.
fn wind_at_blade(x: f32, z: f32, t: f32) -> vec2f {
  let n = u32(gp.wind.w);
  if (n < 2u) {
    return vec2f(0.0);
  }
  let top = f32(n - 1u);
  let gx = clamp((x - gp.wind.x) / gp.wind.z, 0.0, top);
  let gz = clamp((z - gp.wind.y) / gp.wind.z, 0.0, top);
  let ix = min(floor(gx), top - 1.0);
  let iz = min(floor(gz), top - 1.0);
  let fx = gx - ix;
  let fz = gz - iz;
  let c00 = u32(iz) * n + u32(ix);
  let ruffle = 0.7 + 0.6 * grass_noise((x - gp.mean.x * t) / kRuffleCell,
                                        (z - gp.mean.y * t) / kRuffleCell);
  let a = grass_wind[c00] + (grass_wind[c00 + 1u] - grass_wind[c00]) * fx;
  let b = grass_wind[c00 + n] + (grass_wind[c00 + n + 1u] - grass_wind[c00 + n]) * fx;
  return (a + (b - a) * fz) * ruffle;
}

// ── A blade underfoot ──

fn press_cell(key: u32, slot: u32, spring: f32, t: f32) -> f32 {
  let c = grass_press[slot];
  if (c.key != key) {
    return 0.0;
  }
  let r = clamp(1.0 - (t - c.t) / spring, 0.0, 1.0);
  return c.amount * r * r;
}

// The press on a blade of `k` rooted at (x, z) at `t`: how flat (x) and the plan way it lies
// (y, z). The cells (`grass_press`) are bound by the including shader beside `gp`.
fn press_at(k: GrassKind, x: f32, z: f32, t: f32) -> vec3f {
  let n = i32(gp.trample.y);
  if (n < 2) {
    return vec3f(0.0);
  }
  let gx = x / gp.trample.x - 0.5;
  let gz = z / gp.trample.x - 0.5;
  let fcol = floor(gx);
  let frow = floor(gz);
  let fx = gx - fcol;
  let fz = gz - frow;
  let col = i32(fcol);
  let row = i32(frow);
  var amount = 0.0;
  var way = vec2f(0.0);
  for (var dz = 0; dz < 2; dz++) {
    for (var dx = 0; dx < 2; dx++) {
      var wx = fx;
      if (dx == 0) {
        wx = 1.0 - fx;
      }
      var wz = fz;
      if (dz == 0) {
        wz = 1.0 - fz;
      }
      let c = col + dx;
      let r = row + dz;
      let key = (bitcast<u32>(c) & 0xffffu) | (bitcast<u32>(r) << 16u);
      let slot = u32(((r % n) + n) % n) * u32(n) + u32(((c % n) + n) % n);
      let p = press_cell(key, slot, k.air.w, t) * (wx * wz);
      if (p <= 0.0) {
        continue;
      }
      let w = max(unpack2x16snorm(grass_press[slot].way), vec2f(-1.0));
      amount += p;
      way += w * p;
    }
  }
  let wl = sqrt(way.x * way.x + way.y * way.y);
  var out = vec3f(min(amount, 1.0), 0.0, 0.0);
  if (wl > 1e-6) {
    out = vec3f(out.x, way / wl);
  }
  return out;
}

// ── A blade's shape ──

struct BladeShape {
  v0: vec3f,
  v1: vec3f,
  v2: vec3f,
}

fn bend_of(k: GrassKind, speed: f32) -> f32 {
  let w2 = speed * speed;
  return w2 / (w2 + k.air.x * k.air.x);
}

fn blade_shape(k: GrassKind, b: Blade, wind: vec2f, t: f32, press: vec3f) -> BladeShape {
  let h = b.height;
  let hp = grass_hash(b.seed ^ 0x27d4eb2fu);
  let phase = grass_unit(hp) * kGrassTau;
  let freq = k.air.y * (0.85 + 0.3 * grass_unit(grass_hash(hp)));
  let w = sqrt(wind.x * wind.x + wind.y * wind.y);
  let bend = bend_of(k, w);
  var d = vec2f(0.0);
  var o = vec2f(1.0, 0.0);
  if (w > 1e-4) {
    d = wind / w;
    o = d;
  } else {
    let l = sqrt(b.lean.x * b.lean.x + b.lean.y * b.lean.y);
    if (l > 1e-6) {
      o = b.lean / l;
    }
  }
  let osc = sin(kGrassTau * freq * t + phase);
  let osc2 = sin(kGrassTau * freq * 1.37 * t + phase * 1.7);
  let amp = 0.04 + 0.6 * bend * (1.0 - bend);
  var ax = b.lean.x * (1.0 - bend) + d.x * (bend * kMaxBend) + o.x * (osc * amp) -
           o.y * (osc2 * amp * 0.35);
  var az = b.lean.y * (1.0 - bend) + d.y * (bend * kMaxBend) + o.y * (osc * amp) +
           o.x * (osc2 * amp * 0.35);
  if (press.x > 0.0) {
    ax = ax * (1.0 - press.x) + press.y * (kPressBend * press.x);
    az = az * (1.0 - press.x) + press.z * (kPressBend * press.x);
  }
  let al = sqrt(ax * ax + az * az);
  let theta = min(al, kMaxBend + 0.1);
  var u = vec2f(1.0, 0.0);
  if (al > 1e-6) {
    u = vec2f(ax, az) / al;
  }
  let st = sin(theta);
  let ct = cos(theta);
  var s: BladeShape;
  s.v0 = b.root;
  let v2 = s.v0 + vec3f(h * u.x * st, h * ct, h * u.y * st);
  let v1 = s.v0 + vec3f(0.0, h * max(1.0 - st, 0.05), 0.0);
  let chord = length(v2 - s.v0);
  let poly = length(v1 - s.v0) + length(v2 - v1);
  let r = h / ((2.0 * chord + poly) / 3.0);
  s.v1 = s.v0 + r * (v1 - s.v0);
  s.v2 = s.v1 + r * (v2 - v1);
  return s;
}

fn bezier_at(s: BladeShape, u: f32) -> vec3f {
  return (1.0 - u) * (1.0 - u) * s.v0 + 2.0 * u * (1.0 - u) * s.v1 + u * u * s.v2;
}

fn bezier_tangent(s: BladeShape, u: f32) -> vec3f {
  return 2.0 * (1.0 - u) * (s.v1 - s.v0) + 2.0 * u * (s.v2 - s.v1);
}
