// THE TERRAIN'S DRAW, the half every ground material shares (render/terrain.hpp,
// `TerrainSource::shader`): the bindings of groups 1 and 2, the vertex stage (a tile of the
// clipmap, its vertices pulled from the vertex index and placed by its instance's level and tile;
// the odd vertices morphed onto the coarser grid toward the level's edge, the vertices a finer
// level covers sunk under it), the overdraw probe's fragment, and what a material's `fs_main`
// starts and ends with: `ground_at` (the pixel's place, its normal, the game's albedo and the sun
// that reaches it past the ground's own horizon and what stands in it) and `ground_out` (the
// colour seen through the haze, the motion and the reactive mask). A material is a shader that
// includes this file and defines `fs_main`; its own bindings are group 3. Drawn twice: depth only
// first (`terrain.depth`), then shaded where the depth is `equal`, so every visible pixel is
// shaded once.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/sun_shadow.wgsl"

override kTile: u32 = 32u;
// The morph runs from params.info.w to this share of a level's half side.
const kMorphEnd = 0.95;

@group(1) @binding(0) var<uniform> params: TerrainParams;
@group(1) @binding(1) var heights: texture_2d<f32>;
@group(1) @binding(2) var normals: texture_2d<f32>;
@group(1) @binding(3) var albedo: texture_2d<f32>;
@group(1) @binding(4) var<storage, read> tiles: array<u32>;
@group(1) @binding(5) var horizon0: texture_2d<f32>;
@group(1) @binding(6) var horizon1: texture_2d<f32>;

@group(2) @binding(0) var<uniform> shadow: SunShadow;
@group(2) @binding(1) var shadow_maps: texture_depth_2d_array;
@group(2) @binding(2) var shadow_cmp: sampler_comparison;

struct VertexOut {
  // Invariant: the depth prepass and the colour pass run this same vertex code, and the colour
  // pass's `equal` test needs the very same depth bits from both.
  @invariant @builtin(position) position: vec4f,
  @location(0) rel: vec3f,  // camera-relative world position, m
  @location(1) xz: vec2f,   // world plan position, m
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  let packed = tiles[iid];
  let level = packed & 0xffu;
  let tx = (packed >> 8u) & 0xffu;
  let ty = (packed >> 16u) & 0xffu;
  let n = kTile + 1u;
  let g = vec2f(f32(tx * kTile + vid % n), f32(ty * kTile + vid / n));
  let lv = params.levels[level];
  let s = lv.z;
  let at = lv.xy + g * s;
  let half = f32(kTile * 2u) * s;
  let d = max(abs(at.x - frame.eye.x), abs(at.y - frame.eye.z)) / half;
  var m = clamp((d - params.info.w) / (kMorphEnd - params.info.w), 0.0, 1.0);
  if (level + 1u >= u32(params.info.x)) {
    m = 0.0;
  }
  let xz = lv.xy + (g - fract(g * 0.5) * 2.0 * m) * s;
  var y = field_height(heights, params, xz);
  if (level > 0u) {
    let f = params.levels[level - 1u];
    let lo = f.xy + vec2f(0.25 * s);
    let hi = f.xy + vec2f(f32(kTile * 4u) * f.z) - vec2f(0.25 * s);
    if (all(xz > lo) && all(xz < hi)) {
      y -= params.info.z * s;
    }
  }
  var out: VertexOut;
  out.rel = vec3f(xz.x, y, xz.y) - frame.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.xz = xz;
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv), b: reactive (0: a solid surface)
}

// What a material reads of a ground pixel.
struct Ground {
  xz: vec2f,      // world plan position, m
  rel: vec3f,     // camera-relative world position, m
  y: f32,         // world height, m
  dist: f32,      // from the eye, m
  uv: vec2f,      // the grid's uv (mirrored past the map's edge)
  n: vec3f,       // the baked normal (mirrored past the edge)
  albedo: vec3f,  // the game's albedo, linear
  sun: vec3f,     // the sun's light that reaches the point (before the cosine): its colour at
                  // this height, past the ground's horizon and the casters' shadows
}

fn ground_at(in: VertexOut) -> Ground {
  var g: Ground;
  let maxc = grid_max(params);
  let gg = (in.xz - params.field.xy) / params.field.z;
  g.uv = (mirror_grid(gg, maxc) + 0.5) / vec2f(params.field_size.xy);
  let flip = mirror_sign(gg, maxc);
  g.n = normalize(textureSampleLevel(normals, linear_clamp, g.uv, 0.0).xyz * vec3f(flip.x, 1.0, flip.y));
  g.albedo = textureSampleLevel(albedo, linear_clamp, g.uv, 0.0).rgb;
  g.xz = in.xz;
  g.rel = in.rel;
  g.y = in.rel.y + frame.eye.y;
  g.dist = length(in.rel);
  g.sun = sun_light(g.y, in.xz) *
          ground_shadow(params, horizon0, horizon1, linear_clamp, frame.sun.xyz, in.xz) *
          sun_shadow(shadow, shadow_maps, shadow_cmp, in.rel, g.n, in.position.xy, frame.time.y);
  return g;
}

// The shaded colour `c` seen through the haze, with the pixel's motion; `reactive` (0..1) tells
// TAA how little of its history to trust here (a flash that comes and goes: glitter).
fn ground_out(in: VertexOut, c: vec3f, reactive: f32) -> FragmentOut {
  var out: FragmentOut;
  out.colour = vec4f(haze(c, in.position.xy * frame.size.zw, length(in.rel)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), reactive, 0.0);
  return out;
}

// The overdraw probe (labs only): one per fragment that passes the depth test, summed by
// additive blending.
@fragment
fn fs_count() -> @location(0) vec4f {
  return vec4f(1.0, 0.0, 0.0, 0.0);
}

// The wire probe (labs only): a tile's edges as lines, a hair nearer than the ground under them
// (reversed-z: a larger depth is nearer) so the ground hides the lines behind it, coloured by the
// distance from the eye in doublings from 32 m (the clipmap's rings at 64, 128, 256 m ...): near
// magenta, through blue and cyan to green, grey from 2 km.
struct WireOut {
  @location(0) colour: vec4f,
  @builtin(frag_depth) depth: f32,
}

@fragment
fn fs_wire(in: VertexOut) -> WireOut {
  let band = clamp(log2(max(length(in.rel), 32.0) / 32.0), 0.0, 6.0) / 6.0;
  var out: WireOut;
  let k = clamp(vec3f(abs(band * 4.0 - 3.0) - 1.0, 2.0 - abs(band * 4.0 - 2.0),
                      2.0 - abs(band * 4.0 - 1.0)), vec3f(0.0), vec3f(1.0));
  out.colour = vec4f(mix(vec3f(0.25), vec3f(1.0), k), 1.0);
  out.depth = in.position.z * 1.002;
  return out;
}
