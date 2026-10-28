// THE LOCAL LIGHTS a frame holds (render/lights.hpp): group 0 binding 10, the light list and the
// clusters' lists, and the two readers: `local_lights` (a surface lit by every lamp whose sphere
// reaches its cluster) and `light_inscatter` (the light a point in the air scatters toward the
// eye, for a fog volume's march). Included by frame.wgsl, after the surface model.
//
// A light's colour is in candela (rgb, its level applied); the frame's units are the sun's
// 128 000 lx at the pre-exposure's scale, so irradiance in lux becomes the frame's by
// `frame.world.y / kSunLux`.
//
// ge:summary Local lights in WGSL: the cluster lookup, a lamp's irradiance, surfaces and air
// ge:keywords render lights local-lights local_lights light_inscatter cluster light-grid
// ge:keywords inverse-square window spot-cone point-light lamp_irradiance cluster_index
// ge:skill render

struct GpuLight {
  at: vec4f,      // camera-relative position, m; 1 / range^2
  colour: vec4f,  // rgb candela at its level; the emitter's radius^2
  axis: vec4f,    // a spot's axis; cos of its outer cone (-2: a point light)
  cone: vec4f,    // 1 / (cos inner - cos outer); shadow slot (-1: none)
}

struct LightGrid {
  info: vec4u,  // lights, tiles across, tiles down, slices
  grid: vec4f,  // a tile's px across and down; the near plane, m; slices a doubling of distance
  more: vec4f,  // slice 0's far edge, m
  pad: vec4f,
  lights: array<GpuLight, 256>,
  cells: array<vec2u, 3456>,  // (first, count) into `refs`, a cluster
  refs: array<u32>,
}
@group(0) @binding(10) var<storage, read> light_grid: LightGrid;

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
    let e = lamp_irradiance(light_grid.refs[cell.x + k], p, &l);
    c += e * surface_brdf(s, n, v, l);
  }
  return c;
}

// EVERY LAMP in the air at camera-relative `p` (a fog volume's sample), seen along `dir` (unit,
// from the eye): the light scattered toward the eye per unit of the medium's scattering, with a
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
