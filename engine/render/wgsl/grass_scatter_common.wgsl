// THE GRASS'S SCATTER (render/grass.hpp's "grass.scatter"): the cover's triangles and the scatter
// of one blade off its triangle's seed, the WGSL twin of engine/grass's cover.cpp `scatter_one`,
// float for float in the same order; tests/render_grass_test.cpp holds them to it. Needs
// grass_common.wgsl first; only the scatter pass includes it, so the draw's shader stays small.

// grass/cover.hpp's Tri.
struct GrassTri {
  a: vec4f,      // xyz, share at a
  b: vec4f,      // xyz, share at b
  c: vec4f,      // xyz, share at c
  info: vec4u,   // kind, its seed, 0, 0
}

fn grass_area(t: GrassTri) -> f32 {
  let e1 = t.b.xyz - t.a.xyz;
  let e2 = t.c.xyz - t.a.xyz;
  let x = e1.y * e2.z - e1.z * e2.y;
  let y = e1.z * e2.x - e1.x * e2.z;
  let z = e1.x * e2.y - e1.y * e2.x;
  return 0.5 * sqrt(x * x + y * y + z * z);
}

fn blade_slots(t: GrassTri, k: GrassKind) -> u32 {
  let most = max(max(t.a.w, t.b.w), t.c.w);
  let expect = grass_area(t) * k.shape.w * most;
  var n = floor(expect);
  if (grass_unit(grass_hash(t.info.y ^ 0xa511e9b3u)) < expect - n) {
    n = n + 1.0;
  }
  return u32(min(n, f32(kMaxBladesPerTri)));
}

struct GrassBlade {
  blade: Blade,
  rank: f32,
  kept: bool,
}

fn scatter_one(t: GrassTri, k: GrassKind, slot: u32) -> GrassBlade {
  var out: GrassBlade;
  out.kept = false;
  let h0 = grass_hash(t.info.y + slot * 0x9e3779b9u);
  let h1 = grass_hash(h0);
  let h2 = grass_hash(h1);
  let h3 = grass_hash(h2);
  let h4 = grass_hash(h3);
  let h5 = grass_hash(h4);
  let h6 = grass_hash(h5);
  let h7 = grass_hash(h6);
  let h8 = grass_hash(h7);
  let s = sqrt(grass_unit(h0));
  let u = 1.0 - s;
  let v = grass_unit(h1) * s;
  let w = 1.0 - u - v;
  let p = t.a.xyz * u + t.b.xyz * v + t.c.xyz * w;
  let share = t.a.w * u + t.b.w * v + t.c.w * w;
  let most = max(max(t.a.w, t.b.w), t.c.w);
  if (grass_unit(h2) * most >= share) {
    return out;
  }
  out.rank = grass_unit(h3);

  var scale = 1.0;
  var toward = vec2f(0.0);
  let clump = k.lean.z;
  if (clump > 0.0) {
    let gx = floor(p.x / clump);
    let gz = floor(p.z / clump);
    var best = 1e30;
    var best_h = 0u;
    for (var dz = -1; dz <= 1; dz++) {
      for (var dx = -1; dx <= 1; dx++) {
        let cx = gx + f32(dx);
        let cz = gz + f32(dz);
        let ch = grass_hash(grass_hash(bitcast<u32>(i32(cx))) ^ bitcast<u32>(i32(cz)));
        let mx = (cx + 0.5 + (grass_unit(ch) - 0.5) * 0.8) * clump;
        let mz = (cz + 0.5 + (grass_unit(grass_hash(ch)) - 0.5) * 0.8) * clump;
        let ex = mx - p.x;
        let ez = mz - p.z;
        let d2 = ex * ex + ez * ez;
        if (d2 < best) {
          best = d2;
          best_h = ch;
          toward = vec2f(ex, ez);
        }
      }
    }
    scale = 0.75 + 0.5 * grass_unit(grass_hash(best_h ^ 0x5bd1e995u));
    let tl = sqrt(best);
    if (tl > 1e-6) {
      toward = toward / tl;
    }
  }
  var d = vec2f(grass_unit(h4) * 2.0 - 1.0, grass_unit(h5) * 2.0 - 1.0);
  var dl = sqrt(d.x * d.x + d.y * d.y) + 1e-6;
  d = d / dl;
  let pull = k.lean.w;
  var l = vec2f(d.x * (1.0 - pull) + toward.x * pull, d.y * (1.0 - pull) + toward.y * pull);
  dl = sqrt(l.x * l.x + l.y * l.y);
  if (dl < 1e-4) {
    l = d;
  } else {
    l = l / dl;
  }
  let angle = k.lean.x + k.lean.y * (grass_unit(h6) * 2.0 - 1.0);

  out.blade.root = p;
  out.blade.seed = (h0 & 0xffffffu) | (t.info.x << 24u);
  out.blade.height = k.shape.x * (1.0 + k.shape.y * (grass_unit(h7) * 2.0 - 1.0)) * scale;
  out.blade.width = k.shape.z * (0.8 + 0.4 * grass_unit(h8));
  out.blade.lean = l * angle;
  out.kept = true;
  return out;
}
