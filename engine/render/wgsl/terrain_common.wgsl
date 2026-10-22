// What the terrain's shaders share (render/terrain.hpp): the parameters block (the horizon maps'
// blend in it, render/horizon.hpp), the grid's mirrored addressing past the map's edge, its
// bilinear height, and the ground's own shadow (`ground_shadow`), which the instanced draws
// read too (render/instances.hpp).

struct TerrainParams {
  levels: array<vec4f, 10>,  // each level's min corner x, z (world m) and spacing (m)
  info: vec4f,               // level count, tile quads, sink (in spacings), morph start (0..1)
  field: vec4f,              // the grid's origin x, z (world m) and cell (m)
  field_size: vec4u,         // cols, rows, pyramid mips
  horizon: vec4f,            // the horizon maps' blend: share toward the next, low slot, penumbra, on
  horizon_grid: vec4f,       // the maps' origin x, z (world m) and cell (m)
  horizon_size: vec4u,       // the maps' width, height
}

// Grid coordinates (in samples) folded back into the grid: past its edge the grid repeats
// mirrored, so the ground runs on past the map with no seam (a ridge at the edge continues as
// its own reflection) instead of stretching the edge's heights out to the horizon.
fn mirror_grid(g: vec2f, maxc: vec2f) -> vec2f {
  let t = g - 2.0 * maxc * floor(g / (2.0 * maxc));
  return maxc - abs(maxc - t);
}

// +1 where the grid is read the right way round, -1 where it is mirrored (per axis): a normal
// read from a mirrored stretch flips its x or z.
fn mirror_sign(g: vec2f, maxc: vec2f) -> vec2f {
  let t = g - 2.0 * maxc * floor(g / (2.0 * maxc));
  return select(vec2f(1.0), vec2f(-1.0), t > maxc);
}

fn grid_max(p: TerrainParams) -> vec2f {
  return vec2f(f32(p.field_size.x - 1u), f32(p.field_size.y - 1u));
}

// The grid's bilinear height at world (x, z), mirrored past the edge.
fn field_height(heights: texture_2d<f32>, p: TerrainParams, xz: vec2f) -> f32 {
  let maxc = grid_max(p);
  let g = mirror_grid((xz - p.field.xy) / p.field.z, maxc);
  let c0 = min(floor(g), maxc);
  let t = g - c0;
  let i0 = vec2i(c0);
  let i1 = min(i0 + vec2i(1), vec2i(maxc));
  let a = textureLoad(heights, vec2i(i0.x, i0.y), 0).r;
  let b = textureLoad(heights, vec2i(i1.x, i0.y), 0).r;
  let c = textureLoad(heights, vec2i(i0.x, i1.y), 0).r;
  let e = textureLoad(heights, vec2i(i1.x, i1.y), 0).r;
  return mix(mix(a, b, t.x), mix(c, e, t.x), t.y);
}

// How much of the sun reaches world (x, z) past the ground's own horizon (render/horizon.hpp):
// each of the two maps either side of the sun's bearing gives a soft step where the sun's
// elevation crosses the horizon, blended by where the sun lies between their bearings. `sun` is
// the unit direction toward it.
fn ground_shadow(p: TerrainParams, h0: texture_2d<f32>, h1: texture_2d<f32>, s: sampler,
                 sun: vec3f, xz: vec2f) -> f32 {
  if (p.horizon.w == 0.0) {
    return 1.0;
  }
  let maxc = vec2f(p.horizon_size.xy) - 1.0;
  let g = mirror_grid((xz - p.horizon_grid.xy) / p.horizon_grid.z, maxc);
  let uv = (g + 0.5) / vec2f(p.horizon_size.xy);
  let a = textureSampleLevel(h0, s, uv, 0.0).r;
  let b = textureSampleLevel(h1, s, uv, 0.0).r;
  let odd = p.horizon.y > 0.5;
  let lo = select(a, b, odd);
  let hi = select(b, a, odd);
  let sun_tan = sun.y / max(length(sun.xz), 1e-4);
  let w = p.horizon.z;
  return mix(smoothstep(-w, w, sun_tan - lo), smoothstep(-w, w, sun_tan - hi), p.horizon.x);
}
