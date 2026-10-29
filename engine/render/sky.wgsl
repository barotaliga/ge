// THE SKY PASS (render/sky.hpp): the sky-view table behind everything the opaque passes drew,
// the sun's disc, and at night the moon's disc, the stars and the galaxy's band. A full-screen
// triangle at the far plane (depth 0, reversed-z) tested `equal`, so only the pixels nothing
// covered are shaded. Its motion is the camera's turn alone: a direction at infinity does not
// move with the eye.
//
// THE NIGHT (render/night.hpp's numbers in `frame.moon`, `frame.night`):
// - the MOON's disc at its true size, its lit part by Lommel-Seeliger's law toward the real sun
//   over its real face (render/moon.hpp's albedo map, turned by core/moon.hpp's pole and prime
//   meridian) and the rest earthlit, the Earth's shadow on it in an eclipse, dimmed and reddened
//   by the air as the sun is, drawn as the eye adapted to it sees it (`as_seen`);
// - the STARS on the celestial sphere, turned by the sidereal angle about the pole the latitude
//   lifts: the REAL stars (render/stars.hpp: a bright-star catalogue's 9,096, each at its place,
//   magnitude and colour, read off a cube map of 6 x 160 x 160 cells of up to three stars, the
//   pixel's cell and the eight round it), each seen only when brighter than the faintest star the
//   eye finds against the sky behind it (the naked-eye threshold's fit to the sky's brightness: the
//   sixth and a half magnitude under a dark sky, the fifth under a full moon, the first at the end
//   of civil twilight); a point is drawn as the eye sees it, at least a few times the sky behind
//   it at the threshold and brighter by its light above it;
// - the GALAXY's band about the galactic equator, brightest toward its centre, mottled;
// - OTHER WORLDS' MOONS (render/wgsl/moons.wgsl), drawn as the moon is, the nearer over the farther;
// all of it behind the deck's share and lost in a fog's or a fall's air.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/moons.wgsl"

// The real stars' map (render/stars.hpp): kStarSlots layers of the cube's cells.
@group(1) @binding(0) var star_map: texture_2d_array<f32>;
// The moon's albedo map (render/moon.hpp): selenographic longitude -180..180 east along u,
// latitude 90 N..90 S down v, its bytes over 255 times `frame.moon_shadow.w` the albedo against
// the near side's mean; mipmapped.
@group(1) @binding(1) var moon_map: texture_2d<f32>;
// Other worlds' moons' maps (render/moon_maps.hpp), a layer a kind: the luminance (a byte over
// kMoonMapMean the albedo against the kind's mean) and the colour over it (128 a byte of 1).
@group(1) @binding(2) var moon_maps: texture_2d_array<f32>;
@group(1) @binding(3) var moon_colours: texture_2d_array<f32>;

// The sun's angular radius, rad.
const kSunRadius = 0.00465;
// The moon's colour (its dust is a little red), luminance 1.
const kMoonTint = vec3f(1.04, 0.99, 0.93);
// The moon's map's width (render/moon.hpp's kMoonMapWidth).
const kMoonMapWidth = 1024.0;
// The light the air bends into the Earth's umbra: its colour, luminance 1, and its share of full
// sunlight at the axis (render/night.cpp's kUmbraLight; ten times it at the umbra's edge).
const kUmbraTint = vec3f(2.24, 0.70, 0.36);
const kUmbraLight = 1.0e-4;
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
// across, the faintest seen of magnitude `limit`: the pixel's cell and the eight round it on its
// face.
fn stars(c: vec3f, bg: f32, px: f32, limit: f32) -> vec3f {
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

// The light a point of the moon `s` rad from the Earth's shadow's axis gets against full
// sunlight (render/night.hpp's `shadow_light`): the sun's share the Earth leaves it, white, and
// the air's red light bent into the umbra.
fn earth_shadow(s: f32) -> vec3f {
  let umbra = frame.moon_pole.w;
  let penumbra = frame.moon_prime.w;
  if (penumbra <= 0.0 || s >= penumbra) {
    return vec3f(1.0);
  }
  let t = clamp(2.0 * (s - umbra) / (penumbra - umbra) - 1.0, -1.0, 1.0);
  let sun = 1.0 - (acos(t) - t * sqrt(1.0 - t * t)) / PI;
  let red = kUmbraLight * pow(10.0, min(s, umbra) / umbra);
  return vec3f(sun) + kUmbraTint * ((1.0 - sun) * red);
}

// The moon's disc along `d`, through the air to the eye, `px` rad a pixel: its lit part toward
// the real sun by Lommel-Seeliger's law over its albedo map (read at the level the disc's size
// asks), the rest earthlit, the Earth's shadow over both.
fn moon_disc(d: vec3f, r: f32, px: f32) -> vec3f {
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
  // The face: the point's selenographic place on the map, at the level a pixel covers.
  let lat = asin(clamp(dot(n, frame.moon_pole.xyz), -1.0, 1.0));
  let lon = atan2(dot(n, frame.moon_east.xyz), dot(n, frame.moon_prime.xyz));
  let uv = vec2f(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
  let texel = 2.0 * PI * rad / kMoonMapWidth;
  let lod = max(log2(px / max(texel, 1e-12)), 0.0);
  let albedo = textureSampleLevel(moon_map, linear_clamp, uv, lod).r * frame.moon_shadow.w;
  // A soft edge a twentieth of the radius wide against the stair of the pixels.
  let edge = 1.0 - smoothstep(0.95, 1.0, sqrt(rho2));
  let air = transmittance_to_top(transmittance_lut, linear_clamp, r, m.y);
  // The lit surface's and the earthlit part's brightness as seen, at the disc's brightest point
  // under the shadow; their shape, face and shadow as they are against it, so the seas keep
  // their contrast however bright the moon.
  let through = dot(kMoonTint * air, kLuma);
  let peak = max(frame.moon_east.w, 1e-9);
  // A faint part beside a bright one is seen as the eye sees it: its ratio to it to the power
  // 0.35 (a crescent's ashen light a twenty-fifth of its lit limb, a quarter's a thirtieth); the
  // Earth's shadow to the power 0.5, so the umbra's red stays a deep thirtieth of the penumbra.
  let sunlit = as_seen(frame.moon_light.x * peak * through);
  // Never brighter than the earthshine is on its own: in twilight the lit part's compression
  // would otherwise lift it with the sky.
  let earthlit = min(as_seen(frame.moon_light.y * through),
                     sunlit * pow(frame.moon_light.y / max(frame.moon_light.x * peak, 1e-30), 0.35));
  // The shadow's light: its brightness as seen against the peak, its colour as it is.
  let shadow = earth_shadow(length(d - frame.moon_shadow.xyz));
  let level = max(dot(shadow, kLuma), 1e-30);
  let shade = shadow / level * pow(level / peak, 0.5);
  return kMoonTint * air * ((sunlit * lit * shade + earthlit) * albedo * edge /
                            max(through, 1e-30));
}

// THE SKY'S STYLE (render/sky_style.hpp; frame_types.wgsl's `style` rows): what a world that is
// not the Earth shows over its sky. Each part is drawn only when its strength is set, so the
// Earth's sky (every strength 0) is drawn as it always was.

// How much a halo round a disc is seen: the air's scattering against the Earth's, so an airless
// sky has none.
fn air_halo() -> f32 {
  let air = frame.style[0];
  return min(dot(air.xyz, kLuma) * 0.6 + air.w * 0.25, 2.0);
}

// The second sun along `d`: its disc as bright as the sun's times its share, darkened toward the
// limb, and a halo round it in the air, all through the air to the eye.
fn second_sun(d: vec3f) -> vec3f {
  let s = frame.style[3];
  let look = frame.style[4];
  let r = kSunRadius * look.w;
  let ang = acos(clamp(dot(d, s.xyz), -1.0, 1.0));
  let sun = frame.night_sun.w * frame.weather.x * s.w;
  var light = sun * (0.5 * exp(-ang / (6.0 * r)) + 0.06 * exp(-ang / 0.3)) * air_halo();
  if (ang < r) {
    let x = ang / r;
    let limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - x * x, 0.0)));
    light += sun / (PI * kSunRadius * kSunRadius) * limb;
  }
  let through = transmittance_to_top(transmittance_lut, linear_clamp, radius_at(frame.eye.y), s.y);
  return look.xyz * through * light;
}

// How much of the sky the planet's ball and ring dim behind them.
const kPlanetDims = 0.5;

// The planet's bands: a stripe along its axis's latitude, mottled.
fn bands(lat: f32, q: vec3f) -> f32 {
  return 0.82 + 0.18 * sin(lat * 23.0 + 2.0 * mottle(q * 0.2)) + 0.08 * (mottle(q * 0.6) - 0.5);
}

// THE PLANET along `d`: rgb its light (lit by the sun as a ball is, its phase following, its
// night side faint; banded) and its ring's (a flat lit annulus round it, tilted toward the eye,
// the near half over the ball and the far half behind it), through the air; a: the share of the
// sky behind it the ball and the ring hide.
fn planet(d: vec3f) -> vec4f {
  let p = frame.style[5];
  let c = dot(d, p.xyz);
  if (c <= 0.0) {
    return vec4f(0.0);
  }
  // The disc's own frame: east and up across it, and a place on it in planet radii.
  let east = normalize(vec3f(p.z, 0.0, -p.x) + vec3f(1e-6, 0.0, 0.0));
  let up = cross(p.xyz, east);
  let q = vec2f(dot(d, east), dot(d, up)) / sin(p.w);
  let rho2 = dot(q, q);
  let tilt = frame.style[10].x;
  let outer = frame.style[6].w;
  // The ring's plane: its normal leans from the disc's up toward the eye by the tilt.
  let axis = normalize(up * cos(tilt) - p.xyz * sin(tilt));
  let rq = vec2f(q.x, q.y / max(sin(tilt), 0.02));
  let rr = length(rq);
  let sun = frame.night_sun.xyz;
  let e = frame.night_sun.w * frame.weather.x;
  var light = vec3f(0.0);
  var hide = 0.0;
  // The ball.
  let ball = rho2 < 1.0;
  if (ball) {
    let n = q.x * east + q.y * up - sqrt(1.0 - rho2) * p.xyz;
    let lambert = max(dot(n, sun), 0.0);
    let lat = asin(clamp(dot(n, axis), -1.0, 1.0));
    light = frame.style[6].xyz * (bands(lat, n * 40.0) * (lambert + 0.004) * e / PI);
    hide = 1.0 - smoothstep(0.96, 1.0, sqrt(rho2));
  }
  // The ring: the near half (under the disc's middle on screen) over the ball, the far half
  // where the ball does not cover it.
  if (outer > 1.0 && rr > 1.2 && rr < outer) {
    let near = rq.y < 0.0;
    if (near || !ball) {
      let t = (rr - 1.2) / (outer - 1.2);
      let gaps = smoothstep(0.0, 0.05, abs(t - 0.62)) * (0.6 + 0.4 * sin(t * 61.0));
      let lit = 0.25 + 0.75 * abs(dot(axis, sun));
      let alpha = 0.75 * gaps * smoothstep(0.0, 0.05, t) * smoothstep(1.0, 0.95, t);
      light = mix(light, frame.style[6].xyz * (0.7 * lit * e / PI), alpha);
      hide = max(hide, alpha);
    }
  }
  let through = transmittance_to_top(transmittance_lut, linear_clamp, radius_at(frame.eye.y), p.y);
  return vec4f(light * through, hide);
}

// THE NEBULA along celestial direction `c`: a mottled cloud between its two tints, at its
// brightest its strength times the sunlit snow's white.
fn nebula(c: vec3f) -> vec3f {
  let a = frame.style[7];
  let size = max(frame.style[8].w, 0.05);
  let n1 = mottle(c * (0.12 / size));
  let n2 = mottle(c * (0.31 / size) + 1.7);
  let n3 = mottle(c * (0.83 / size) + 4.1);
  let n4 = mottle(c * (2.2 / size) + 7.3);
  let field = n1 * 0.55 + n2 * 0.27 + n3 * 0.12 + n4 * 0.06;
  let dense = smoothstep(0.42, 0.8, field);
  let wisp = dense * dense * (0.6 + 0.8 * n3);
  return mix(a.xyz, frame.style[8].xyz, smoothstep(0.3, 0.7, n2)) *
         (a.w * white_luminance() * wisp);
}

// THE GLOW FROM BELOW along `d`: strongest at and under the horizon (its strength times the snow's
// white there), fading by its height.
fn glow_below(d: vec3f) -> vec3f {
  let g = frame.style[9];
  return g.xyz * (white_luminance() * exp(-max(d.y, 0.0) / max(g.w, 1e-3)));
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let d = view_ray(frame, in.uv);
  let px = max(length(dpdx(d)), length(dpdy(d)));
  // Under the horizon while there is a reach, the horizon's sky (frame.wgsl's `mist_dir`).
  let h = mist_dir(d);
  var colour = sky_luminance(h);
  // The shafts: the cloud's shadow across the air in front of the sky, strongest toward the sun
  // where the air scatters most of what it scatters forward.
  let toward = pow(max(dot(h, frame.sun.xyz), 0.0), 4.0);
  colour *= mix(1.0, shafts(h, kShaftReach, in.position.xy), kShaftShare * mix(0.4, 1.0, toward) * (1.0 - frame.weather.y));
  // The disc: the sun's light over its solid angle, darkened toward the limb, and only where
  // the planet does not hide it; behind a deck it is lost in the deck's grey. It is the real
  // sun's, whichever light is the key.
  let c = dot(d, frame.night_sun.xyz);
  let sun_r = kSunRadius * frame.style[2].w;
  let edge = cos(sun_r);
  if (c > edge) {
    let x = sqrt(max(1.0 - c * c, 0.0)) / sin(sun_r);
    let limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - x * x, 0.0)));
    let through = transmittance_to_top(transmittance_lut, linear_clamp, radius_at(frame.eye.y),
                                       frame.night_sun.y);
    let disc = frame.night_sun.w * frame.weather.x * through / (PI * sun_r * sun_r);
    colour += frame.style[2].xyz * disc * limb * (1.0 - frame.weather.y);
  }
  // THE SKY'S STYLE: the second sun, the nebula and the stars by day, the planet over them, the
  // glow from below; behind the deck's share and lost in a fog's or a fall's air as the night is.
  let open = (1.0 - frame.weather.y) * exp(-frame.weather.z * 10000.0);
  if (frame.style[3].w > 0.0) {
    colour += second_sun(d) * (1.0 - frame.weather.y);
  }
  var hidden = 0.0;
  if (frame.style[5].w > 0.0) {
    let pl = planet(d);
    hidden = pl.w * (1.0 - frame.weather.y);
    // Not as the air would have it (its light in front of the ball is all there is of the sky
    // there) but as a world's sky is drawn: the ball and the ring stand out of it, dimming half
    // of the sky behind them.
    colour = colour * (1.0 - kPlanetDims * hidden) + pl.rgb * (1.0 - frame.weather.y);
  }
  let by_day = frame.style[10].y;
  if ((frame.style[7].w > 0.0 || by_day > 0.0) && d.y > -0.02 && open > 0.0) {
    let cel = celestial(d);
    let air = transmittance_to_top(transmittance_lut, linear_clamp,
                                   kGroundRadius + max(frame.eye.w, 0.0), d.y);
    var far = vec3f(0.0);
    if (frame.style[7].w > 0.0) {
      far += nebula(cel);
    }
    if (by_day > 0.0 && frame.night.x <= 0.0) {
      // Not the eye's threshold against the day's sky but the share's: down to the sixth
      // magnitude at 1, the faintest a hundredth of the snow's white.
      let bg = 0.0017 * white_luminance();
      far += stars(cel, bg, px, mix(1.0, 6.0, by_day));
    }
    colour += far * air * open * (1.0 - hidden);
  }
  if (frame.style[9].w > 0.0 && dot(frame.style[9].xyz, vec3f(1.0)) > 0.0) {
    colour += glow_below(d) * (1.0 - 0.6 * frame.weather.y);
  }
  // THE NIGHT: behind the deck's share, lost in a fog's or a fall's air, under the horizon none.
  let clear = (1.0 - frame.weather.y) * exp(-frame.weather.z * 10000.0);
  if (frame.night.x > 0.0 && clear > 0.0 && d.y > -0.02) {
    let r = kGroundRadius + max(frame.eye.w, 0.0);
    let air = transmittance_to_top(transmittance_lut, linear_clamp, r, d.y);
    let cel = celestial(d);
    let bg = dot(colour, kLuma);
    // The disc hides the stars behind it; the post pass leaves its colour to the cones, so the
    // sky in front of it is given to the rods here (frame.wgsl's night_vision).
    let cover = on_moon(d);
    let limit = limiting_magnitude(bg * kSunLux / frame.world.y);
    colour += (stars(cel, bg, px, limit) + galaxy(cel)) * air * clear * (1.0 - max(cover, hidden));
    // Other worlds' moons as the moon is: the stars behind them hidden (`cover`), the air's light
    // in front of them kept.
    colour = mix(colour, rods(colour), cover) +
             (moon_disc(d, r, px) + other_moons(d, r, px).rgb) * clear;
  } else {
    let r = kGroundRadius + max(frame.eye.w, 0.0);
    if (frame.moon_light.z > 0.0) {
      colour += moon_disc(d, r, px) * (1.0 - frame.weather.y);
    }
    // Other worlds' moons by day, pale in the blue as the moon is.
    colour += other_moons(d, r, px).rgb * (1.0 - frame.weather.y);
  }
  // Seen through the fog's or the fall's veil, and the fog in areas in front of it all.
  colour = local_fog(veil_sky(h, colour), in.uv, 1e30);
  // A LIGHTNING FLASH (frame.wet.w): the storm's cloud lit from within, a cool white brightest
  // overhead, over whatever the sky was.
  if (frame.wet.w > 0.0) {
    colour += vec3f(0.85, 0.9, 1.0) * white_luminance() * frame.wet.w * mix(0.6, 1.4, max(d.y, 0.0));
  }
  var out: FragmentOut;
  out.colour = vec4f(min(colour, vec3f(60000.0)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(d, 0.0), in.position.xy), 0.0, 0.0);
  return out;
}
