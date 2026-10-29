// OTHER WORLDS' MOONS as the sky pass draws them (render/moons.hpp's kinds, placed by
// render/night.hpp's set_moons, their rows in `frame.moons`). Included by render/sky.wgsl after
// render/wgsl/frame.wgsl.
//
// - THE SHAPE: an ellipsoid of the kind's semi-axes in its body frame (x its 0 N 0 E, z its
//   pole), hit along the pixel's line of sight in closed form; a dented kind (Phobos, Deimos,
//   Miranda) is marched inside that bound to the first point under its dented surface, so its
//   outline is a lump's.
// - THE SURFACE: the kind's map of the real moon (render/moon_maps.hpp) over the body point, so
//   it turns with the moon, read at the mip level the pixel's footprint asks, so a small moon is
//   its mean and not a shimmer; coloured by its colour map or its tint.
// - THE LIGHT: Lommel-Seeliger's law (a dusty body's, the Earth's moon's) blended toward
//   Lambert's (an icy one's) by the kind's share, toward the real sun, at the surface's brightness
//   as seen (frame.wgsl's `as_seen`), through the air as the moon is; the night side dark.
// - A HAZE (Titan's): a shell round the body lit by the sun, brightest at the lit limb and in a
//   ring when the sun is behind it, and the face under it softened toward the haze's colour.
//
// ge:summary Other worlds' moons drawn: lumpy shapes, each kind's real map, light and haze
// ge:keywords render moons other-moons moon-shape ellipsoid raymarch moon-maps mosaic
// ge:keywords haze lommel-seeliger lambert other_moons
// ge:skill render
// ge:test tests/render_moons_test.cpp

const kMoonRowsWgsl = 6;  // render/moons.hpp's kMoonRows
const kMaxMoonsWgsl = 4;  // render/moons.hpp's kMaxMoons

fn moon_row(k: i32, r: i32) -> vec4f {
  return frame.moons[k * kMoonRowsWgsl + r];
}

// Four uniform numbers off a lattice cell and a seed.
fn moon_hash(cell: vec3i, seed: u32) -> vec4f {
  var v = vec3u(cell + vec3i(4096)) * vec3u(1664525u, 1013904223u, 2654435761u) +
          vec3u(1013904223u ^ seed, 12345u, 7u + seed);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v ^= v >> vec3u(16u);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  let w = v.x ^ v.y ^ v.z;
  return vec4f(vec4u(v, w * 747796405u + 2891336453u) >> vec4u(8u)) / 16777216.0;
}

// A value noise, 0..1, a cell a unit.
fn moon_noise(p: vec3f, seed: u32) -> f32 {
  let i = vec3i(floor(p));
  let f = p - floor(p);
  let w = f * f * (3.0 - 2.0 * f);
  let n000 = moon_hash(i, seed).x;
  let n100 = moon_hash(i + vec3i(1, 0, 0), seed).x;
  let n010 = moon_hash(i + vec3i(0, 1, 0), seed).x;
  let n110 = moon_hash(i + vec3i(1, 1, 0), seed).x;
  let n001 = moon_hash(i + vec3i(0, 0, 1), seed).x;
  let n101 = moon_hash(i + vec3i(1, 0, 1), seed).x;
  let n011 = moon_hash(i + vec3i(0, 1, 1), seed).x;
  let n111 = moon_hash(i + vec3i(1, 1, 1), seed).x;
  let x0 = mix(mix(n000, n100, w.x), mix(n010, n110, w.x), w.y);
  let x1 = mix(mix(n001, n101, w.x), mix(n011, n111, w.x), w.y);
  return mix(x0, x1, w.z);
}

// A body direction's latitude, rad (z its pole).
fn moon_lat(p: vec3f) -> f32 {
  return asin(clamp(normalize(p).z, -1.0, 1.0));
}

// Toward a place on the body (latitude and east longitude, degrees; x 0 N 0 E, y 0 N 90 E).
fn moon_place(lat: f32, lon: f32) -> vec3f {
  let a = radians(lat);
  let o = radians(lon);
  return vec3f(cos(a) * cos(o), cos(a) * sin(o), sin(a));
}

// THE DENTS of a lumpy kind at body point `p`, 0..1 (the share of `lump` it sinks): low lumps,
// and Phobos's great crater on its leading end.
fn moon_dent(kind: i32, p: vec3f) -> f32 {
  let u = normalize(p);
  var d = moon_noise(u * 1.7, 3u) * 0.6 + moon_noise(u * 3.4, 4u) * 0.25;
  if (kind == 0) {
    // A bowl a quarter of the body across, near the sub-planet end's leading side.
    let t = acos(clamp(dot(u, moon_place(-1.0, -49.0)), -1.0, 1.0)) / 0.45;
    d += select(0.0, (1.0 - t * t) * 0.9, t < 1.0);
  }
  return clamp(d, 0.0, 1.0);
}

// The field a lumpy kind's surface is the zero of: negative inside.
fn moon_field(kind: i32, p: vec3f, axes: vec3f, lump: f32) -> f32 {
  return length(p / axes) - (1.0 - lump * moon_dent(kind, p));
}

// THE SURFACE of `kind` at body point `p` (on the unit sphere), `fp` body radii a pixel: its map
// (render/moon_maps.hpp) at the body's east longitude and latitude, at the mip level the pixel's
// footprint asks, in its colour where its mosaic had one. Titan's map is its ground through the
// haze in the near infrared; in visible light the haze hides most of it, so a fifth of its
// contrast is kept.
fn moon_albedo(kind: i32, p: vec3f, fp: f32) -> vec3f {
  let lon = atan2(p.y, p.x);
  let lat = moon_lat(p);
  let uv = vec2f(lon / (2.0 * PI) + 0.5, clamp(0.5 - lat / PI, 0.001, 0.999));
  // A texel of the map is 2 pi / its width body radii along the equator.
  let lod = max(log2(fp * 1024.0 / (2.0 * PI)), 0.0);
  let lum = textureSampleLevel(moon_maps, linear_repeat, uv, kind, lod).r * (255.0 / 100.0);
  let colour = textureSampleLevel(moon_colours, linear_repeat, uv, kind, max(lod - 2.0, 0.0)).rgb *
               (255.0 / 128.0);
  var a = lum * colour;
  if (kind == 6) {
    a = mix(vec3f(1.0), a, 0.2);
  }
  return a;
}

// THE k-TH OTHER MOON along `d` seen from radius `r` (km from the planet's centre), `px` rad a
// pixel: rgb its light through the air as seen, a the share of the sky behind it it hides.
fn other_moon(k: i32, d: vec3f, r: f32, px: f32) -> vec4f {
  let r0 = moon_row(k, 0);
  let rad = r0.w;
  let m = r0.xyz;
  let c = dot(d, m);
  if (rad <= 0.0 || c < cos(rad)) {
    return vec4f(0.0);
  }
  let r1 = moon_row(k, 1);
  let r2 = moon_row(k, 2);
  let r3 = moon_row(k, 3);
  let r4 = moon_row(k, 4);
  let kind = i32(r1.w + 0.5);
  let pole = r1.xyz;
  let prime = r2.xyz;
  let east = cross(prime, pole);
  let haze = moon_row(k, 5).x;
  // The pixel's point across the disc and the line of sight, in the body's frame, in drawn radii.
  let q = (d - m * c) / sin(rad);
  let qb = vec3f(dot(q, prime), dot(q, east), dot(q, pole));
  let mb = vec3f(dot(m, prime), dot(m, east), dot(m, pole));
  let body = 1.0 / (1.0 + haze);
  let axes = r4.xyz * body;
  let lump = r4.w;
  let fp = px / max(rad * body, 1e-9);
  // The bounding ellipsoid's near point.
  let qa = qb / axes;
  let ma = mb / axes;
  let A = dot(ma, ma);
  let B = dot(qa, ma);
  let C = dot(qa, qa) - 1.0;
  let disc = B * B - A * C;
  var hit = disc > 0.0;
  var s = (-B - sqrt(max(disc, 0.0))) / A;
  var edge = 1.0;
  if (hit && lump > 0.0) {
    // A dented kind: marched from the bound's near point to its far one, the first step under
    // the surface refined by halving.
    let far = (-B + sqrt(disc)) / A;
    let step = (far - s) / 24.0;
    var prev = s;
    var found = false;
    for (var i = 0; i < 24; i++) {
      let t = s + step * f32(i);
      if (moon_field(kind, qb + mb * t, axes, lump) < 0.0) {
        found = true;
        var lo = prev;
        var hi = t;
        for (var j = 0; j < 6; j++) {
          let mid = 0.5 * (lo + hi);
          if (moon_field(kind, qb + mb * mid, axes, lump) < 0.0) {
            hi = mid;
          } else {
            lo = mid;
          }
        }
        s = hi;
        break;
      }
      prev = t;
    }
    hit = found;
  } else if (hit) {
    // The outline's soft edge, a pixel and a half wide (half of `disc / A` is about how far inside
    // the outline the pixel lies, in body radii).
    edge = smoothstep(0.0, 1.5 * fp, 0.5 * disc / A);
  }
  let sun = frame.night_sun.xyz;
  let sun_b = vec3f(dot(sun, prime), dot(sun, east), dot(sun, pole));
  let air = transmittance_to_top(transmittance_lut, linear_clamp, r, m.y);
  let tint = r3.xyz;
  let through = dot(tint * air, kLuma);
  let surface = r2.w;
  let sunlit = as_seen(surface * through);
  var light = vec3f(0.0);
  var hide = 0.0;
  if (hit) {
    let p = qb + mb * s;
    // The normal: the ellipsoid's gradient, or the dented field's.
    var n = normalize(p / (axes * axes));
    if (lump > 0.0) {
      let e = 0.01;
      n = normalize(vec3f(
          moon_field(kind, p + vec3f(e, 0.0, 0.0), axes, lump) -
              moon_field(kind, p - vec3f(e, 0.0, 0.0), axes, lump),
          moon_field(kind, p + vec3f(0.0, e, 0.0), axes, lump) -
              moon_field(kind, p - vec3f(0.0, e, 0.0), axes, lump),
          moon_field(kind, p + vec3f(0.0, 0.0, e), axes, lump) -
              moon_field(kind, p - vec3f(0.0, 0.0, e), axes, lump)));
    }
    let u = normalize(p);
    let look = moon_albedo(kind, u, fp);
    let mu = max(dot(n, -mb), 1e-3);
    let mu0 = dot(n, sun_b);
    let ls = select(0.0, 2.0 * mu0 / (mu0 + mu), mu0 > 0.0);
    let lam = 1.5 * max(mu0, 0.0);
    var lit = mix(ls, lam, r3.w);
    var albedo = look;
    if (haze > 0.0) {
      // The haze over the face: the terminator softened, the limb toward the haze's own light.
      let wrap = max(dot(n, sun_b) + 0.25, 0.0) / 1.25;
      lit = mix(lit, 1.5 * wrap, 0.5);
      albedo = mix(albedo, vec3f(1.1), pow(1.0 - mu, 3.0));
    }
    light = tint * air * (sunlit * lit / max(through, 1e-30)) * albedo * edge;
    hide = edge;
  }
  if (haze > 0.0) {
    // THE HAZE'S SHELL past the body's limb: thinning outward, lit where the sun reaches it, and
    // brightest in a ring when the sun stands behind the moon (it scatters forward).
    let rho = length(q);
    let h = (rho - body) / (1.0 - body);
    if (h > -0.05 && h < 1.0) {
      let shell = normalize(q - m * sqrt(max(1.0 - rho * rho, 0.0)));
      let density = exp(-max(h, 0.0) * 4.0) * (1.0 - smoothstep(0.7, 1.0, h)) *
                    smoothstep(-0.05, 0.05, h);
      let side = max(dot(shell, sun) + 0.3, 0.0);
      let behind = pow(max(dot(sun, m), 0.0), 6.0);
      let glow = density * (0.45 * side + 1.6 * behind);
      light += tint * air * (sunlit * glow / max(through, 1e-30)) * vec3f(1.1, 0.95, 0.7);
      hide = max(hide, 0.6 * density);
    }
  }
  return vec4f(light, hide);
}

// EVERY OTHER MOON along `d`, the later (nearer) over the earlier: rgb their light, a the share
// of the sky behind them they hide.
fn other_moons(d: vec3f, r: f32, px: f32) -> vec4f {
  var out = vec4f(0.0);
  for (var k = 0; k < kMaxMoonsWgsl; k++) {
    if (moon_row(k, 0).w <= 0.0) {
      continue;
    }
    let o = other_moon(k, d, r, px);
    out = vec4f(out.rgb * (1.0 - o.a) + o.rgb, max(out.a, o.a));
  }
  return out;
}
