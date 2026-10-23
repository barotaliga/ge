// THE SKY PASS (render/sky.hpp): the sky-view table behind everything the opaque passes drew,
// the sun's disc, and at night the moon's disc, the stars and the galaxy's band. A full-screen
// triangle at the far plane (depth 0, reversed-z) tested `equal`, so only the pixels nothing
// covered are shaded. Its motion is the camera's turn alone: a direction at infinity does not
// move with the eye.
//
// THE NIGHT (render/night.hpp's numbers in `frame.moon`, `frame.night`):
// - the MOON's disc at its true size, its lit part by Lommel-Seeliger's law toward the real sun
//   over its seas and the rest earthlit, dimmed and reddened by the air as the sun is, drawn as
//   the eye adapted to it sees it (`as_seen`);
// - the STARS on the celestial sphere, turned by the sidereal angle about the pole the latitude
//   lifts: the REAL stars (render/stars.hpp: a bright-star catalogue's 9,096, each at its place,
//   magnitude and colour, read off a cube map of 6 x 160 x 160 cells of up to three stars, the
//   pixel's cell and the eight round it), each seen only when brighter than the faintest star the
//   eye finds against the sky behind it (the naked-eye threshold's fit to the sky's brightness: the
//   sixth and a half magnitude under a dark sky, the fifth under a full moon, the first at the end
//   of civil twilight); a point is drawn as the eye sees it, at least a few times the sky behind
//   it at the threshold and brighter by its light above it;
// - the GALAXY's band about the galactic equator, brightest toward its centre, mottled;
// all of it behind the deck's share and lost in a fog's or a fall's air.
#include "render/wgsl/frame.wgsl"

// The real stars' map (render/stars.hpp): kStarSlots layers of the cube's cells.
@group(1) @binding(0) var star_map: texture_2d_array<f32>;

// The sun's angular radius, rad.
const kSunRadius = 0.00465;
// The moon's colour (its dust is a little red), luminance 1.
const kMoonTint = vec3f(1.04, 0.99, 0.93);
// The stars: cells along a cube face's edge and stars a cell, a magnitude-0 star's illuminance
// outside the air (lx), the point's spread (pixels) and how many times the sky behind it a star
// at the threshold is drawn.
const kStarCells = 160.0;  // render/stars.hpp's kStarCells and kStarSlots
const kStarSlots = 3;
const kStarZeroLux = 2.54e-6;
const kStarSpread = 1.0;
const kStarSeen = 6.0;
// The galactic pole and centre on the celestial sphere (right ascension and declination
// 192.859 and 27.128 degrees; 266.405 and -28.936).
const kGalacticPole = vec3f(-0.8676, -0.1981, 0.4560);
const kGalacticCentre = vec3f(-0.0548, -0.8734, -0.4838);

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  out.uv = vec2f(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv); the sky turns with the camera only
}

// A world direction (x east, y up, z north) on the celestial sphere: x toward the vernal
// equinox, z toward the north pole.
fn celestial(d: vec3f) -> vec3f {
  let sl = sin(frame.night.w);
  let cl = cos(frame.night.w);
  let sin_dec = d.y * sl + d.z * cl;
  let ch = d.y * cl - d.z * sl;  // cos(dec) cos(hour angle)
  let sh = -d.x;                 // cos(dec) sin(hour angle), the hour angle west
  // The right ascension is the sidereal angle less the hour angle.
  let cs = cos(frame.night.z);
  let ss = sin(frame.night.z);
  return vec3f(cs * ch + ss * sh, ss * ch - cs * sh, sin_dec);
}

// Four uniform numbers off a cell.
fn hash4(cell: vec3u) -> vec4f {
  var v = cell * vec3u(1664525u, 1013904223u, 2654435761u) + vec3u(1013904223u, 12345u, 7u);
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

// The magnitude of the faintest star seen against a sky of luminance `l` (cd/m2).
fn limiting_magnitude(l: f32) -> f32 {
  let b = -2.5 * log2(max(l, 1e-9) / 108000.0) * 0.30103;
  return 7.93 - 5.0 * log2(pow(10.0, 4.316 - b / 5.0) + 1.0) * 0.30103;
}

// A star's colour by its B-V, luminance 1: the blue-white of a hot star (-0.3) to the orange of
// a cool one (1.6).
fn star_tint(bv: f32) -> vec3f {
  let t = clamp((bv + 0.3) / 1.9, 0.0, 1.0);
  let c = vec3f(0.72 + 0.53 * t, 0.86 + 0.12 * sin(t * PI), 1.3 - 0.62 * t);
  return c / dot(c, kLuma);
}

// The point on the celestial sphere at a cube face's cell place `g` (cells, 0..kStarCells a side).
fn cube_point(face: u32, sgn: f32, g: vec2f) -> vec3f {
  let at = g / kStarCells * 2.0 - 1.0;
  var p = vec3f(0.0);
  p[face] = sgn;
  if (face == 0u) {
    p.y = at.x;
    p.z = at.y;
  } else if (face == 1u) {
    p.x = at.x;
    p.z = at.y;
  } else {
    p.x = at.x;
    p.y = at.y;
  }
  return normalize(p);
}

// One star of the map, seen from celestial direction `c` a pixel `px` rad across, against a sky
// of luminance `bg` whose faintest seen star is of magnitude `limit`.
fn star_light(c: vec3f, p: vec3f, m: f32, bv: f32, bg: f32, px: f32, limit: f32) -> vec3f {
  let off = length(c - p) / px;
  if (off > 3.0 * kStarSpread) {
    return vec3f(0.0);
  }
  // Its light, the sun's units, spread over its pixels; at the threshold at least kStarSeen
  // times the sky behind it at its peak, and brighter by its light above the threshold.
  let e = kStarZeroLux / kSunLux * frame.world.y * pow(10.0, -0.4 * m);
  let solid = 2.0 * PI * kStarSpread * kStarSpread * px * px;
  let seen = kStarSeen * bg * pow(10.0, 0.4 * (limit - m));
  let peak = max(e / solid, seen);
  let fade = 1.0 - smoothstep(limit - 0.3, limit + 0.5, m);
  let spot = exp(-0.5 * off * off / (kStarSpread * kStarSpread));
  return star_tint(bv) * (peak * fade * spot);
}

// THE REAL STARS along celestial direction `c` (render/stars.hpp's map: the cube's cells, up to
// kStarSlots stars a cell), against a sky of luminance `bg` (the sun's units), one pixel `px` rad
// across: the pixel's cell and the eight round it on its face.
fn stars(c: vec3f, bg: f32, px: f32) -> vec3f {
  let limit = limiting_magnitude(bg * kSunLux / frame.world.y);
  if (limit < -1.0) {
    return vec3f(0.0);
  }
  // The cube face and the cell on it.
  let a = abs(c);
  var face = 0u;
  var uv = vec2f(c.y, c.z) / a.x;
  if (a.y >= a.x && a.y >= a.z) {
    face = 1u;
    uv = vec2f(c.x, c.z) / a.y;
  } else if (a.z >= a.x && a.z >= a.y) {
    face = 2u;
    uv = vec2f(c.x, c.y) / a.z;
  }
  let sgn = select(1.0, -1.0, c[face] < 0.0);
  let axis = face + select(0u, 3u, c[face] < 0.0);
  let g = (uv * 0.5 + 0.5) * kStarCells;
  let cell = vec2i(min(floor(g), vec2f(kStarCells - 1.0)));
  var light = vec3f(0.0);
  for (var dy = -1; dy <= 1; dy++) {
    for (var dx = -1; dx <= 1; dx++) {
      let at = cell + vec2i(dx, dy);
      if (any(at < vec2i(0)) || any(at >= vec2i(i32(kStarCells)))) {
        continue;
      }
      let texel = vec2i(i32(axis) * i32(kStarCells) + at.x, at.y);
      for (var slot = 0; slot < kStarSlots; slot++) {
        let s = textureLoad(star_map, texel, slot, 0);
        let m = s.z * 255.0 / 20.0 - 2.0;
        if (m > limit + 0.5) {
          continue;
        }
        let p = cube_point(face, sgn, vec2f(at) + s.xy);
        light += star_light(c, p, m, s.w * 255.0 / 70.0 - 0.5, bg, px, limit);
      }
    }
  }
  return light;
}

// A cheap value noise on the celestial sphere, 0..1.
fn mottle(c: vec3f) -> f32 {
  let q = c * 24.0;
  let i = floor(q);
  let f = q - i;
  let w = f * f * (3.0 - 2.0 * f);
  let o = vec3u(vec3i(i) + vec3i(64));
  let n000 = hash4(o).x;
  let n100 = hash4(o + vec3u(1u, 0u, 0u)).x;
  let n010 = hash4(o + vec3u(0u, 1u, 0u)).x;
  let n110 = hash4(o + vec3u(1u, 1u, 0u)).x;
  let n001 = hash4(o + vec3u(0u, 0u, 1u)).x;
  let n101 = hash4(o + vec3u(1u, 0u, 1u)).x;
  let n011 = hash4(o + vec3u(0u, 1u, 1u)).x;
  let n111 = hash4(o + vec3u(1u, 1u, 1u)).x;
  let x0 = mix(mix(n000, n100, w.x), mix(n010, n110, w.x), w.y);
  let x1 = mix(mix(n001, n101, w.x), mix(n011, n111, w.x), w.y);
  return mix(x0, x1, w.z);
}

// The galaxy's band along celestial direction `c`, the sun's units: one and a half times the
// dark sky's own glow again on its equator, brightest toward its centre, mottled by its dust.
fn galaxy(c: vec3f) -> vec3f {
  let b = dot(c, kGalacticPole);
  let toward = max(dot(c, kGalacticCentre), 0.0);
  let band = exp(-b * b / 0.018) * (0.45 + 0.55 * toward * toward);
  let dust = 0.55 + 0.9 * mottle(c) * mottle(c * 2.3 + 0.37);
  return vec3f(0.96, 0.98, 1.04) * (frame.night.x * 1.6 * band * dust);
}

// THE MOON AS SEEN: its lit surface is some thirty thousand times as bright as the moonlit snow
// the eye has adapted to, which no display holds; the eye adapts to it locally and sees its seas.
// Over a white face's luminance a brightness is drawn at that luminance times how many times
// brighter it is to the power 0.15 (the lit surface some five times the snow's white under a full
// moon, just short of the tone curve's white); under it, as it is (a moon by day, pale in the
// blue).
fn as_seen(l: f32, white: f32) -> f32 {
  if (l <= white) {
    return l;
  }
  return white * pow(l / white, 0.15);
}

// The moon's disc along `d`, through the air to the eye: its lit part toward the real sun by
// Lommel-Seeliger's law over its seas (the dark plains, two fifths darker, on its face as it turns
// with the sky about the pole), the rest earthlit.
fn moon_disc(d: vec3f, r: f32) -> vec3f {
  let m = frame.moon.xyz;
  let rad = frame.moon_light.z;
  let c = dot(d, m);
  if (rad <= 0.0 || c < cos(rad)) {
    return vec3f(0.0);
  }
  let q = (d - m * c) / sin(rad);
  let rho2 = min(dot(q, q), 1.0);
  let mu = sqrt(1.0 - rho2);
  let n = q - m * mu;
  let mu0 = dot(n, frame.night_sun.xyz);
  let lit = select(0.0, 2.0 * mu0 / (mu0 + mu), mu0 > 0.0);
  // The face: its up toward the celestial pole.
  let pole = vec3f(0.0, sin(frame.night.w), cos(frame.night.w));
  let up = normalize(pole - m * dot(pole, m) + vec3f(1e-6, 0.0, 0.0));
  let right = cross(up, m);
  let face = vec2f(dot(q, right), dot(q, up));
  let seas = mottle(vec3f(face * 0.11, 0.37)) * 0.65 + mottle(vec3f(face * 0.27, 0.71)) * 0.35;
  let albedo = mix(1.0, 0.6, smoothstep(0.48, 0.62, seas)) / 0.88;
  // A soft edge a twentieth of the radius wide against the stair of the pixels.
  let edge = 1.0 - smoothstep(0.95, 1.0, sqrt(rho2));
  let air = transmittance_to_top(transmittance_lut, linear_clamp, r, m.y);
  // The lit surface's and the earthlit part's brightness as seen; their shape and seas as they
  // are, so the seas keep their contrast however bright the moon.
  let through = dot(kMoonTint * air, kLuma);
  let white = white_luminance();
  // The earthlit part beside the lit one is seen as the eye sees a faint thing beside a bright
  // one: its ratio to it to the power 0.35 (a crescent's ashen light a twenty-fifth of its lit
  // limb, a quarter's a thirtieth).
  let sunlit = as_seen(frame.moon_light.x * through, white);
  let earthlit = sunlit * pow(frame.moon_light.y / max(frame.moon_light.x, 1e-30), 0.35);
  return kMoonTint * air * ((sunlit * lit + earthlit) * albedo * edge / max(through, 1e-30));
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let d = view_ray(frame, in.uv);
  let px = max(length(dpdx(d)), length(dpdy(d)));
  var colour = sky_luminance(d);
  // The disc: the sun's light over its solid angle, darkened toward the limb, and only where
  // the planet does not hide it; behind a deck it is lost in the deck's grey. It is the real
  // sun's, whichever light is the key.
  let c = dot(d, frame.night_sun.xyz);
  let edge = cos(kSunRadius);
  if (c > edge) {
    let x = sqrt(max(1.0 - c * c, 0.0)) / sin(kSunRadius);
    let limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - x * x, 0.0)));
    let through = transmittance_to_top(transmittance_lut, linear_clamp, radius_at(frame.eye.y),
                                       frame.night_sun.y);
    let disc = frame.night_sun.w * frame.weather.x * through / (PI * kSunRadius * kSunRadius);
    colour += disc * limb * (1.0 - frame.weather.y);
  }
  // THE NIGHT: behind the deck's share, lost in a fog's or a fall's air, under the horizon none.
  let clear = (1.0 - frame.weather.y) * exp(-frame.weather.z * 10000.0);
  if (frame.night.x > 0.0 && clear > 0.0 && d.y > -0.02) {
    let r = kGroundRadius + max(frame.eye.w, 0.0);
    let air = transmittance_to_top(transmittance_lut, linear_clamp, r, d.y);
    let cel = celestial(d);
    let bg = dot(colour, kLuma);
    colour += (stars(cel, bg, px) + galaxy(cel)) * air * clear + moon_disc(d, r) * clear;
  } else if (frame.moon_light.z > 0.0) {
    let r = kGroundRadius + max(frame.eye.w, 0.0);
    colour += moon_disc(d, r) * (1.0 - frame.weather.y);
  }
  var out: FragmentOut;
  out.colour = vec4f(min(colour, vec3f(60000.0)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(d, 0.0), in.position.xy), 0.0, 0.0);
  return out;
}
