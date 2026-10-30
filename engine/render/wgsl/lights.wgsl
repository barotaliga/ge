// THE LOCAL LIGHTS a frame holds (render/lights.hpp): group 0 binding 10, the light list and the
// clusters' lists, and the two readers: `local_lights` (a surface lit by every lamp whose sphere
// reaches its cluster, past what casts into its shadow tiles) and `light_inscatter` (the light a point in the air scatters toward the
// eye, for a fog volume's march). Included by frame.wgsl, after the surface model.
//
// A light's colour is in candela (rgb, its level applied); the frame's units are the sun's
// 128 000 lx at the pre-exposure's scale, so irradiance in lux becomes the frame's by
// `frame.world.y / kSunLux`.
//
// ge:summary Local lights in WGSL: the cluster lookup, a lamp's irradiance, surfaces and air
// ge:keywords render lights local-lights local_lights light_inscatter cluster light-grid
// ge:keywords inverse-square window spot-cone point-light lamp_irradiance cluster_index
// ge:keywords lamp_shadow lamp-atlas shadow-tile cube-face pcf
// ge:skill render

struct GpuLight {
  at: vec4f,      // camera-relative position, m; 1 / range^2
  colour: vec4f,  // rgb candela at its level; the emitter's radius^2
  axis: vec4f,    // a spot's axis; cos of its outer cone (-2: a point light)
  cone: vec4f,    // 1 / (cos inner - cos outer); shadow slot (-1: none)
}

struct LampTile {
  view: mat4x4f,  // a point relative to the lamp -> the tile's clip (reversed z: near / distance)
  rect: vec4f,    // the tile's corner in the atlas (uv); its side (uv); a texel's size 1 m out, m
}

struct LightGrid {
  info: vec4u,  // lights, tiles across, tiles down, slices
  grid: vec4f,  // a tile's px across and down; the near plane, m; slices a doubling of distance
  more: vec4f,  // slice 0's far edge, m; a shadow tile's texel (uv); the depth bias (share)
  pad: vec4f,
  lights: array<GpuLight, 256>,
  tiles: array<LampTile, 64>,  // the lamps' shadow atlas (render/lamp_shadows.hpp)
  cells: array<vec2u, 3456>,   // (first, count) into `refs`, a cluster
  refs: array<u32>,
}
@group(0) @binding(10) var<storage, read> light_grid: LightGrid;
// The lamps' shadow atlas: depth near / distance away each tile's lamp, 0 where nothing casts.
@group(0) @binding(13) var lamp_atlas: texture_depth_2d;
@group(0) @binding(14) var lamp_compare: sampler_comparison;

const kLampTilesAcross = 8u;
const kLampTaps = array<vec2f, 4>(vec2f(-0.94, -0.34), vec2f(0.34, -0.94), vec2f(0.94, 0.34),
                                  vec2f(-0.34, 0.94));

// The share of light `i`'s light that reaches camera-relative `p` (normal `n`) past what casts
// into its tiles: 1 for a light with no tiles. A point light's tile is the cube's face the way to
// `p` leaves by; four rotated taps (each a hardware 2x2 comparison) turned per pixel at `frag`,
// held inside the tile.
fn lamp_shadow(i: u32, p: vec3f, n: vec3f, frag: vec3f) -> f32 {
  let lt = light_grid.lights[i];
  let slot = i32(lt.cone.y);
  if (slot < 0) {
    return 1.0;
  }
  let away = p - lt.at.xyz;
  var tile = u32(slot);
  if (lt.axis.w < -1.5) {
    let a = abs(away);
    if (a.x >= a.y && a.x >= a.z) {
      tile += select(1u, 0u, away.x > 0.0);
    } else if (a.y >= a.z) {
      tile += select(3u, 2u, away.y > 0.0);
    } else {
      tile += select(5u, 4u, away.z > 0.0);
    }
  }
  let t = light_grid.tiles[tile];
  // Off the surface by a texel and a half of the tile at this distance, toward the light.
  let q = away + n * (1.5 * t.rect.w * length(away));
  let c = t.view * vec4f(q, 1.0);
  if (c.w <= 0.0) {
    return 1.0;
  }
  let ndc = c.xyz / c.w;
  let side = t.rect.z;
  let texel = light_grid.more.y;
  let lo = t.rect.xy + vec2f(texel);
  let hi = t.rect.xy + vec2f(side - texel);
  let uv = t.rect.xy + clamp(vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5), vec2f(0.0),
                             vec2f(1.0)) * side;
  let depth = ndc.z * (1.0 + light_grid.more.z);
  let px = frag.xy + 5.588238 * f32(u32(frame.time.y) % 64u);
  let angle = 6.2831853 * fract(52.9829189 * fract(dot(px, vec2f(0.06711056, 0.00583715))));
  let cs = cos(angle);
  let sn = sin(angle);
  var sum = 0.0;
  for (var k = 0; k < 4; k++) {
    let o = kLampTaps[k];
    let at = clamp(uv + vec2f(cs * o.x - sn * o.y, sn * o.x + cs * o.y) * (1.5 * texel), lo, hi);
    sum += textureSampleCompareLevel(lamp_atlas, lamp_compare, at, depth);
  }
  return sum * 0.25;
}

// The cluster of the pixel at `frag` (a fragment's position: xy its pixel, z its reversed-z depth).
fn cluster_index(frag: vec3f) -> u32 {
  let g = light_grid.grid;
  let tx = min(u32(frag.x / g.x), light_grid.info.y - 1u);
  let ty = min(u32(frag.y / g.y), light_grid.info.z - 1u);
  let d = g.z / max(frag.z, 1e-9);
  var s = 0u;
  if (d >= light_grid.more.x) {
    s = min(1u + u32(floor(log2(d / light_grid.more.x) * g.w)), light_grid.info.w - 1u);
  }
  return (s * light_grid.info.z + ty) * light_grid.info.y + tx;
}

// Light `i`'s irradiance perpendicular to the way to it at camera-relative `p` (frame units);
// the unit way toward the light is written to `l`.
fn lamp_irradiance(i: u32, p: vec3f, l: ptr<function, vec3f>) -> vec3f {
  let lt = light_grid.lights[i];
  let to = lt.at.xyz - p;
  let d2 = dot(to, to);
  *l = to * inverseSqrt(max(d2, 1e-8));
  let w = clamp(1.0 - d2 * d2 * lt.at.w * lt.at.w, 0.0, 1.0);
  var e = lt.colour.rgb * (w * w / (d2 + lt.colour.w));
  if (lt.axis.w > -1.5) {
    let c = dot(-*l, lt.axis.xyz);
    let k = clamp((c - lt.axis.w) * lt.cone.x, 0.0, 1.0);
    e *= k * k;
  }
  return e * (frame.world.y / kSunLux);
}

// EVERY LAMP on a surface: the radiance toward `v` (unit, toward the eye) off surface `s` at
// camera-relative `p` with normal `n`, seen at pixel `frag`.
fn local_lights(s: Surface, p: vec3f, n: vec3f, v: vec3f, frag: vec3f) -> vec3f {
  if (light_grid.info.x == 0u) {
    return vec3f(0.0);
  }
  let cell = light_grid.cells[cluster_index(frag)];
  var c = vec3f(0.0);
  for (var k = 0u; k < cell.y; k++) {
    var l: vec3f;
    let i = light_grid.refs[cell.x + k];
    let e = lamp_irradiance(i, p, &l);
    c += e * (surface_brdf(s, n, v, l) * lamp_shadow(i, p, n, frag));
  }
  return c;
}

// EVERY LAMP in the air at camera-relative `p` (a fog volume's sample), seen along `dir` (unit,
// away the eye): the light scattered toward the eye per unit of the medium's scattering, with a
// Henyey-Greenstein phase of asymmetry `g`, read off the cluster `p` falls in (`frag` its pixel
// and depth, as a surface there would have them).
fn light_inscatter(p: vec3f, dir: vec3f, frag: vec3f, g: f32) -> vec3f {
  if (light_grid.info.x == 0u) {
    return vec3f(0.0);
  }
  let cell = light_grid.cells[cluster_index(frag)];
  var c = vec3f(0.0);
  for (var k = 0u; k < cell.y; k++) {
    var l: vec3f;
    let e = lamp_irradiance(light_grid.refs[cell.x + k], p, &l);
    let mu = dot(-dir, l);
    let hg = (1.0 - g * g) / (4.0 * PI * pow(max(1.0 + g * g + 2.0 * g * mu, 1e-4), 1.5));
    c += e * hg;
  }
  return c;
}
