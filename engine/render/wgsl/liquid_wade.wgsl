// WADING (render/liquid_wade.hpp; docs/liquids.md § Wading): a body's track through a lava or a
// slime as the surface takes it, from the body's stamps (`body.wades`, `body.wade_v`): the furrow
// that closes slowly behind it with its lips raised, the bow ahead of it, the rings out of every
// stamp, and lava's crust torn open along the furrow. The twin of liquid_wade.cpp's
// `wade_height`, which the test holds the numbers of; included after liquid_body.wgsl.

const kHeadMostS = 0.4;  // s: render/liquid_wade.hpp's

// The nearest point of the segment p0 -> p1 (times t0 -> t1) to `p`: its distance, its time.
fn wade_nearest(p: vec2f, p0: vec2f, t0: f32, p1: vec2f, t1: f32) -> vec2f {
  let e = p1 - p0;
  let len2 = dot(e, e);
  let u = select(0.0, clamp(dot(p - p0, e) / len2, 0.0, 1.0), len2 > 1e-8);
  return vec2f(length(p0 + u * e - p), mix(t0, t1, u));
}

// At plan point `p` at `t`: x the height over the level (m), y the crust torn open (0..1), z how
// far into a furrow (0..1). `foot` the pixel's footprint (m): rings shorter than a few of it are
// left out (0: none are; the vertices pass `rings` false and take none).
fn wade_at(p: vec2f, t: f32, foot: f32, rings: bool) -> vec3f {
  let n = u32(body.wade0.x);
  if (n == 0u) {
    return vec3f(0.0);
  }
  let leg = body.wade0.y;
  let depth = body.wade0.z;
  let amp = body.wade0.w;
  let speed = body.wade1.x;
  let lambda = body.wade1.y;
  let ring_s = body.wade1.z;
  let furrow_s = body.wade1.w;
  let heal_s = body.wade2.x;
  let life = body.wade2.y;
  let seen = select(0.0, select(1.0, smoothstep(2.0 * foot, 4.0 * foot, lambda), foot > 0.0), rings);
  var trough = 0.0;
  var lip = 0.0;
  var sum = 0.0;
  var bow = 0.0;
  var open = 0.0;
  var furrow = 0.0;
  for (var i = 0u; i < n; i++) {
    let a = body.wades[i];
    let b = body.wade_v[i];
    let age = t - a.z;
    if (age < 0.0 || age > life || a.w <= 0.0) {
      continue;
    }
    let v = b.xy;
    let vs = length(v);
    let r = length(p - a.xy);
    if (r > speed * age + 2.0 * lambda + 3.0 * leg + vs * (b.z + kHeadMostS)) {
      continue;
    }
    var near = wade_nearest(p, a.xy - v * b.z, a.z - b.z, a.xy, a.z);
    let ahead = min(age, kHeadMostS);
    let h = a.xy + v * ahead;
    if (b.w > 0.5) {
      let n2 = wade_nearest(p, a.xy, a.z, h, a.z + ahead);
      if (n2.x < near.x) {
        near = n2;
      }
    }
    let at = max(t - near.y, 0.0);
    let rho2 = (near.x / leg) * (near.x / leg);
    let closing = exp(-at / furrow_s);
    let f = -depth * a.w * (1.0 - rho2) * exp(-rho2) * closing;
    if (f < 0.0) {
      trough = min(trough, f);
    } else {
      lip = max(lip, f);
    }
    furrow = max(furrow, a.w * exp(-rho2) * closing);
    if (heal_s > 0.0) {
      open = max(open, a.w * exp(-2.0 * rho2) * exp(-at / heal_s));
    }
    if (seen > 0.0) {
      let u = r - speed * age;
      let env = exp(-u * u / (lambda * lambda));
      sum += amp * a.w * exp(-age / ring_s) * env * sin(6.2831853 * u / lambda) *
             inverseSqrt(1.0 + r / leg);
    }
    if (b.w > 0.5 && vs > 0.2) {
      let c = h + v / vs * leg * 1.2;
      let q = p - c;
      bow += 0.6 * depth * a.w * min(vs / 2.0, 1.0) * exp(-dot(q, q) / (leg * leg)) *
             exp(-max(age - kHeadMostS, 0.0) / 0.15);
    }
  }
  return vec3f(trough + lip + sum * seen + bow, open, furrow);
}
